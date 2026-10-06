#!/usr/bin/env python3
"""Compare referenced scalar tables to STABS layouts; emit counts/hashes only."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import shlex
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from arm64_test import MachO64
from layout import align_up, shape, pointer_offsets
from macho32 import MachO32
from stabs import Database, Unsupported


def native_layout(ty):
    kind = ty['kind']
    if kind in ('scalar', 'pointer'):
        size = 8 if kind == 'pointer' or ty['ctype'] in ('long', 'unsigned long') else ty['size']
        return dict(ty, size=size, align=min(size, 8))
    if kind == 'array':
        child = native_layout(ty['child'])
        return dict(ty, child=child, size=child['size'] * ty['count'], align=child['align'])
    if kind == 'struct':
        fields, end, align = [], 0, 1
        for name, _, child in ty['fields']:
            child = native_layout(child)
            off = align_up(end, child['align'])
            fields.append((name, off, child))
            end, align = off + child['size'], max(align, child['align'])
        return dict(ty, fields=fields, size=align_up(end, align), align=align)
    raise ValueError('Unsupported union/aggregate')


def scalar_fields(old, new, old_offset=0, new_offset=0):
    kind = old['kind']
    if kind == 'scalar':
        yield old_offset, new_offset, old['ctype'], old['size'], new['size']
    elif kind == 'array':
        for index in range(old['count']):
            yield from scalar_fields(old['child'], new['child'], old_offset + index * old['child']['size'],
                                     new_offset + index * new['child']['size'])
    elif kind == 'struct':
        for (_, a, child), (_, b, native) in zip(old['fields'], new['fields']):
            yield from scalar_fields(child, native, old_offset + a, new_offset + b)
    else:
        raise ValueError('Pointer/union table requires relocation-aware comparison')


def mismatch_count(reference, native, ty):
    count = 0
    for a, b, ctype, old_size, new_size in scalar_fields(ty, native_layout(ty)):
        left, right = reference[a:a + old_size], native[b:b + new_size]
        if len(left) != old_size or len(right) != new_size:
            raise ValueError('Table exceeds object extent')
        if ctype in ('float', 'double'):
            count += left != right
        else:
            signed = not ctype.startswith('unsigned') and ctype != '_Bool'
            count += int.from_bytes(left, 'little', signed=signed) != int.from_bytes(right, 'little', signed=signed)
    return count


def read_native(obj, symbol, size):
    index, address, _ = obj.symbols[symbol]
    if not index:
        raise ValueError('Undefined symbol')
    section = obj.sections[index - 1]
    end = min((value for sec, value, _ in obj.symbols.values() if sec == index and value > address),
              default=section[2] + section[3])
    if address + size > end:
        raise ValueError('STABS native layout exceeds next-symbol extent')
    if section[8] & 0xff in (1, 12):
        return bytes(size)
    offset = section[4] + address - section[2]
    return obj.data[offset:offset + size]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('audit', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    if options.output.resolve().is_relative_to(ROOT):
        parser.error('Evidence must be outside the repository')
    audit = json.loads(options.audit.read_text())
    image = MachO32(audit['binary'])
    db = Database().read(audit['binary'])
    uses = defaultdict(set)
    for row in audit['functions']:
        for name in row.get('native', {}).get('variables', []):
            uses[name.removeprefix('imp_')].add(row['reference']['name'])
    objects, definitions = {}, defaultdict(list)
    for entry in json.loads(Path(audit['compile_commands']).read_text()):
        args = entry.get('arguments') or shlex.split(entry['command'])
        if 'cod2_macos.dir' not in ' '.join(args) or '-o' not in args:
            continue
        path = Path(entry['directory']) / args[args.index('-o') + 1]
        try:
            obj = MachO64(path)
        except (ValueError, OSError):
            continue
        objects[str(path)] = obj
        for name, (section, _, _) in obj.symbols.items():
            if section:
                definitions[name.removeprefix('_')].append((str(path), name))
    records, seen = [], set()
    for var in db.variables:
        if var.name not in uses or var.name in seen:
            continue
        seen.add(var.name)
        record = dict(name=var.name, functions=sorted(uses[var.name]), address=var.address)
        try:
            ty = shape(db, var.type)
            if ty['kind'] != 'array':
                continue
            record['reference_bytes'] = ty['size']
            if pointer_offsets(ty):
                record['status'] = 'pointer-bearing: use datagen relocation gate'
            else:
                native = definitions.get(var.name, [])
                if len(native) != 1:
                    record['status'] = 'missing-or-ambiguous-native-storage'
                else:
                    record['native_bytes'] = native_layout(ty)['size']
                    data, meta = image.read_object(var.name, ty['size'])
                    record['address'] = meta['address']
                    emitted = read_native(objects[native[0][0]], native[0][1], record['native_bytes'])
                    record['mismatched_fields'] = mismatch_count(data, emitted, ty)
                    record['reference_sha256'] = hashlib.sha256(data).hexdigest()
                    record['native_sha256'] = hashlib.sha256(emitted).hexdigest()
                    record['status'] = 'different' if record['mismatched_fields'] else 'equal'
        except (ValueError, Unsupported, RecursionError, KeyError) as exc:
            record['status'] = 'unresolved'
            record['reason'] = str(exc)
        records.append(record)
    result = dict(counts=dict(Counter(r['status'] for r in records)), tables=records)
    options.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result['counts'], indent=2))


if __name__ == '__main__': main()
