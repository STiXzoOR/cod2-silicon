#!/usr/bin/env python3
"""Inventory import address uses and zero-filled link placeholders.

No binary bytes or decompilation are emitted. --compile-commands preprocesses
the actual target to distinguish guarded legacy uses from live native uses.
The lexical inventory deliberately retains uncertain accesses for review;
only proven wrong loads from import slots or struct/union/non-pointer arrays fail --check.
"""
import argparse
import bisect
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
import json
import hashlib
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from stabs import Database, Unsupported  # noqa: E402
from macho32 import MachO32  # noqa: E402

TOKEN = re.compile(r'\bimp_(\w+)\b')
POINTER_LOAD = re.compile(r'\*\s*\([^()\n]+\*\s*\*\s*\)\s*$')
IMPORT_SLOT_LOAD = re.compile(r'\*\s*\([^()\n]+\*\s*\*\s*\)\s*&\s*$')
CAST = re.compile(r'\(([^()\n]+\*)\)\s*$')
PLACEHOLDER = re.compile(r'^(char|int) (\w+)(?:\[(\d+)\])?[^;\n]*=\s*(?:\{\s*0\s*\}|0)\s*;', re.M)
LINE = re.compile(r'^#\s+(\d+)\s+"([^"]+)"')


def clean(text):
    # Preserve line numbers and offsets while removing strings and comments.
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]),
                  text, flags=re.S)


def classification(prefix):
    if IMPORT_SLOT_LOAD.search(prefix):
        return 'import-slot-load'
    if POINTER_LOAD.search(prefix):
        return 'pointer-load'
    cast = CAST.search(prefix)
    if cast:
        return 'cast-address' if not re.search(r'\*\s*$', prefix[:cast.start()]) else 'scalar-load'
    return 'address-or-expression'


def extra_pointer_load(site, shapes):
    return site['access'] == 'pointer-load' and any(
        shape.startswith(('struct', 'union')) or
        (shape.startswith('array:') and shape != 'array:pointer') for shape in shapes)


def incorrect_import_load(site, shapes):
    if extra_pointer_load(site, shapes):
        return 'extra-pointer-load'
    if site['access'] == 'import-slot-load' and shapes == ['pointer']:
        return 'missing-pointer-load'
    return None


def source_sites(files):
    for path, text in files.items():
        if path.name in ('import_pointers.c', 'generated_syms.h'):
            continue
        for number, line in enumerate(clean(text).splitlines(), 1):
            if line.lstrip().startswith('extern '):
                continue
            for match in TOKEN.finditer(line):
                yield dict(file=str(path.relative_to(ROOT)), line=number,
                           column=match.start() + 1, symbol=match[1],
                           access=classification(line[:match.start()]))


def preprocess(entry, placeholder_names=()):
    args = entry.get('arguments') or shlex.split(entry['command'])
    args = list(args)
    for flag in ('-o', '-MF', '-MT', '-MQ'):
        while flag in args:
            pos = args.index(flag)
            del args[pos:pos + 2]
    args = [a for a in args if a not in ('-c', '-MD', '-MMD', '-MP')]
    result = subprocess.run(args + ['-E', '-w'], cwd=entry['directory'],
                            capture_output=True, text=True)
    if result.returncode:
        raise ValueError(entry['file'] + ': preprocessing failed\n' + result.stderr)
    path, number = None, 0
    sites = []
    storage = []
    storage_token = re.compile(r'\b(' + '|'.join(re.escape(n) for n in placeholder_names) + r')\b') if placeholder_names else None
    for line in result.stdout.splitlines():
        directive = LINE.match(line)
        if directive:
            number = int(directive[1])
            candidate = Path(directive[2])
            if not candidate.is_absolute():
                candidate = Path(entry['directory']) / candidate
            path = candidate.resolve()
            continue
        if path and path.is_relative_to(ROOT / 'src') and path.name not in ('import_pointers.c', 'generated_syms.h'):
            if not line.lstrip().startswith('extern '):
                for match in TOKEN.finditer(line):
                    sites.append(dict(file=str(path.relative_to(ROOT)), line=number,
                                      column=match.start() + 1, symbol=match[1],
                                      access=classification(line[:match.start()])))
                if storage_token and '/stubs/' not in str(path):
                    for match in storage_token.finditer(line):
                        storage.append(dict(file=str(path.relative_to(ROOT)), line=number,
                                            symbol=match[1], source=line.strip()))
        number += 1
    return sites, storage


def reference(binary):
    db = Database().read(binary)
    macho = MachO32(binary)
    variables = defaultdict(list)
    for variable in db.variables:
        variables[variable.name].append(variable)
    result = {}
    for name, candidates in variables.items():
        shapes = set()
        for variable in candidates:
            try:
                ty = db.resolve(variable.type)
                shape = ty.kind
                if ty.kind == 'array':
                    shape += ':' + db.resolve(ty.args[0]).kind
                if ty.name:
                    shape += ':' + ty.name
                shapes.add(shape)
            except (Unsupported, ValueError, UnboundLocalError):
                shapes.add('unresolved')
        item = dict(stabs_shapes=sorted(shapes))
        try:
            symbol, section, address, extent = macho.named_object(name)
            item.update(binary_symbol=symbol, address=hex(address),
                        next_symbol_extent=extent, section=section.name)
        except ValueError as error:
            item['extent_error'] = str(error)
        result[name] = item
    for function in db.functions:
        match = re.match(r'^_Z(\d+)', function)
        if match:
            start = match.end()
            name = function[start:start + int(match[1])]
        else:
            name = function.removeprefix('_')
        result.setdefault(name, dict(stabs_shapes=['function']))
    # Imported C++ members/vtables use reconstructed linkage spelling rather
    # than the debug variable's spelling. Keep unknown data explicitly unknown.
    section_addresses = defaultdict(list)
    for symbol in macho.symbols:
        section_addresses[symbol[2]].append(symbol[4])
    section_addresses = {s: sorted(set(a)) for s, a in section_addresses.items()}
    for symbol, kind, section, _, address in macho.symbols:
        name = symbol.removeprefix('_')
        item = result.setdefault(name, dict(stabs_shapes=['vtable' if name.startswith('_ZTV') else 'data-unresolved']))
        addresses = section_addresses[section]
        index = bisect.bisect(addresses, address)
        end = addresses[index] if index < len(addresses) else macho.sections[section - 1].address + macho.sections[section - 1].size
        if 'address' not in item:
            item.update(binary_symbol=symbol, address=hex(address),
                        next_symbol_extent=end - address,
                        section=macho.sections[section - 1].name)
        if name.startswith('_Z'):
            result.setdefault(symbol, item)
    return result


def placeholders(files):
    result = []
    stripped_files = {p: clean(t) for p, t in files.items()}
    original_lines = {p: t.splitlines() for p, t in files.items()}
    external_files = defaultdict(set)
    for path, stripped in stripped_files.items():
        if path.suffix in ('.c', '.cpp'):
            for declaration in re.findall(r'\bextern\b[^;()\n]*', stripped):
                for name in re.findall(r'\b\w+\b', declaration):
                    external_files[name].add(path)
    for path, text in files.items():
        if '/stubs/' not in str(path):
            continue
        for match in PLACEHOLDER.finditer(clean(text)):
            element_type, name, elements = match[1], match[2], int(match[3] or 1)
            sites = []
            # Only TUs with an explicit external declaration qualify. Generic
            # names still require semantic review because locals can shadow.
            for use_path in sorted(external_files[name]):
                if use_path == path:
                    continue
                stripped = stripped_files[use_path]
                for number, line in enumerate(stripped.splitlines(), 1):
                    if line.lstrip().startswith('extern '):
                        continue
                    if re.search(r'\b' + name + r'\b', line):
                        sites.append(dict(file=str(use_path.relative_to(ROOT)), line=number,
                                          source=original_lines[use_path][number - 1].strip()))
            result.append(dict(symbol=name, file=str(path.relative_to(ROOT)),
                               line=text[:match.start()].count('\n') + 1,
                               bytes=elements * (1 if element_type == 'char' else 4),
                               element_type=element_type, elements=elements, zero_filled=True,
                               uses=sites, generic_name=name in ('buf', 'name', 'tr', 'version')))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--compile-commands', type=Path)
    parser.add_argument('--json', type=Path, required=True)
    parser.add_argument('--check', action='store_true')
    options = parser.parse_args()
    if not options.binary.exists():
        print(f'SKIP retail import ABI check: missing private STABS input: {options.binary}')
        return
    files = {p: p.read_text(errors='replace') for p in sorted((ROOT / 'src').rglob('*'))
             if p.suffix in ('.c', '.h', '.cpp') and 'sdl2' not in p.parts}
    lexical = list(source_sites(files))
    storage = placeholders(files)
    active = None
    if options.compile_commands:
        entries = json.loads(options.compile_commands.read_text())
        entries = [e for e in entries if Path(e['file']).is_relative_to(ROOT / 'src')]
        active = []
        active_storage = []
        placeholder_names = [s['symbol'] for s in storage if not s['generic_name']]
        with ThreadPoolExecutor(max_workers=4) as pool:
            for sites, storage_sites in pool.map(lambda e: preprocess(e, placeholder_names), entries):
                active.extend(sites)
                active_storage.extend(storage_sites)
        unique = {tuple(site.items()): site for site in active}
        active = sorted(unique.values(), key=lambda s: (s['file'], s['line'], s['column']))
        unique_storage = {tuple(site.items()): site for site in active_storage}
        for placeholder in storage:
            placeholder['native_lexical_uses'] = None if placeholder['generic_name'] else [
                s for s in unique_storage.values() if s['symbol'] == placeholder['symbol']]
    ref = reference(options.binary)
    symbols = sorted({s['symbol'] for s in lexical + (active or [])})
    imports = {s: ref.get(s, dict(stabs_shapes=['function-or-unresolved'])) for s in symbols}
    checked = active if active is not None else lexical
    errors = []
    for site in checked:
        shape = imports[site['symbol']]['stabs_shapes']
        problem = incorrect_import_load(site, shape)
        if problem:
            errors.append(dict(site, problem=problem))
    report = dict(binary_sha256=hashlib.sha256(options.binary.read_bytes()).hexdigest(),
                  imports=imports, lexical_sites=lexical, native_sites=active,
                  placeholders=storage, errors=errors,
                  counts=dict(import_symbols=len(imports), lexical_sites=len(lexical),
                              native_sites=len(active) if active is not None else None,
                              lexical_accesses=dict(Counter(s['access'] for s in lexical)),
                              native_accesses=dict(Counter(s['access'] for s in active or [])),
                              placeholders=len(storage),
                              proven_extra_dereferences=sum(e['problem'] == 'extra-pointer-load' for e in errors),
                              proven_missing_dereferences=sum(e['problem'] == 'missing-pointer-load' for e in errors)))
    options.json.parent.mkdir(parents=True, exist_ok=True)
    options.json.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(json.dumps(report['counts'], sort_keys=True))
    for error in errors:
        print('%(file)s:%(line)s: %(problem)s of imp_%(symbol)s' % error, file=sys.stderr)
    return bool(options.check and errors)


if __name__ == '__main__':
    raise SystemExit(main())
