#!/usr/bin/env python3
"""Check native game/server layouts and network-field offsets against local retail STABS."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from stabs import Database, Type, Unsupported
from layout import align_up, shape

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=Path.home() / 'Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386')
parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
args = parser.parse_args()
missing = [str(path) for path in [args.binary] if not path.is_file()]
if missing:
    print('SKIP retail-derived LP64 layouts: missing private input: ' + ', '.join(missing))
    raise SystemExit(0)
db = Database().read(args.binary)


def named(name):
    candidates = []
    for key, value in db.names.items():
        if value == name:
            try:
                node = db.resolve(Type('ref', (key,)))
                if node.kind in ('struct', 'union'):
                    candidates.append(node)
            except Unsupported:
                pass
    # Some TUs declare only the shared prefix; select the complete retail record.
    return max(candidates, key=lambda node: (node.args[0], len(node.args[1])))


# Existing upstream game handles encode indices/arena offsets in four-byte slots.
HANDLES = {'parent', 'chain', 'tagInfo', 'tagChildren', 'nextFree'}


def native(node, owner='', member=''):
    names = {node.name}
    current, seen = node, set()
    while current.kind == 'ref' and current.args[0] not in seen:
        key = current.args[0]
        seen.add(key)
        names.add(db.names.get(key, ''))
        current = db.types.get(key, current)
    node = db.resolve(node)
    names.add(node.name)
    if owner in ('gentity_s', 'gentity_t') and member in HANDLES:
        return 4, 4
    if node.kind in ('pointer', 'reference'):
        return 8, 8
    if node.kind == 'array':
        element, first, last = node.args
        size, alignment = native(element)
        assert first == 0
        return size * (last + 1), alignment
    if node.kind in ('struct', 'union'):
        # Engine netadr_t deliberately retains its IPX bytes: 20 bytes, versus
        # the Mac retail IPv4-only 12. Account for that established ABI seam.
        if names & {'netadr_t', 'netadr_s'} or (node.args[0] == 12 and
                [field[0] for field in node.args[1]] == ['type', 'ip', 'port']):
            return 20, 4
        if 'VoicePacket_t' in names or (node.args[0] == 261 and
                [field[0] for field in node.args[1]] == ['talker', 'data', 'dataSize']):
            # Retail and the current header explicitly pack this pointer-free wire record.
            return 261, 1
        fields, size, alignment = native_fields(node)
        return size, alignment
    scalar = shape(db, node)
    return scalar['size'], min(scalar['size'], 8)


def native_fields(node):
    size, alignment, fields = 0, 1, []
    for member, ty, retail_bit, retail_bits in node.args[1]:
        width, align = native(ty, node.name, member)
        offset = align_up(size, align) if node.kind == 'struct' else 0
        fields.append((member, offset, width))
        size = offset + width if node.kind == 'struct' else max(size, width)
        alignment = max(alignment, align)
    return fields, align_up(size, alignment), alignment


def retail_field(node, expression):
    offset = 0
    for component in expression.split('.'):
        match = re.fullmatch(r'(\w+)(?:\[(\d+)\])?', component)
        assert match, expression
        field, index = match.groups()
        node = db.resolve(node)
        matches = [(n, t, bit, bits) for n, t, bit, bits in node.args[1] if n == field]
        if not matches:
            # The engine uses anonymous unions in entityState_t.
            for n, ty, bit, bits in node.args[1]:
                if not n:
                    try:
                        child_offset, child = retail_field(ty, component)
                        matches = [(field, child, bit + child_offset * 8, 0)]
                        break
                    except (AssertionError, ValueError):
                        pass
        assert len(matches) == 1, expression
        _, node, bit, bits = matches[0]
        offset += bit // 8
        if index is not None:
            array = db.resolve(node)
            assert array.kind == 'array'
            node = array.args[0]
            offset += int(index) * shape(db, node)['size']
    return offset, node


checks = ['#include "common_types.h"']
roots = {'gentity_t': 'gentity_s', 'gclient_t': 'gclient_s', 'client_t': 'client_s',
         'serverStatic_t': 'serverStatic_t', 'server_t': 'server_t',
         'entityState_t': 'entityState_s', 'playerState_t': 'playerState_s',
         'clientSnapshot_t': 'clientSnapshot_t', 'cachedClient_t': 'cachedClient_s',
         'archivedEntity_t': 'archivedEntity_s', 'msg_t': 'msg_t'}
count = 0
for ctype, retail_name in roots.items():
    node = named(retail_name)
    fields, size, alignment = native_fields(node)
    checks.append(f'_Static_assert(sizeof({ctype}) == {size}, "{ctype} size");')
    for member, offset, width in fields:
        if not member:  # unnamed unions are checked through the wire fields below
            continue
        checks.append(f'_Static_assert(offsetof({ctype}, {member}) == {offset}, "{ctype}.{member} offset");')
        checks.append(f'_Static_assert(sizeof((({ctype} *)0)->{member}) == {width}, "{ctype}.{member} width");')
        count += 1
    print(f'{ctype}: retail i386 {node.args[0]}, native {size}, tag {node.name}')

wire = {'PSF': ('playerState_t', named('playerState_s')),
        'CSF': ('clientState_t', named('clientState_s')),
        'AEF': ('archivedEntity_t', named('archivedEntity_s')),
        'ESF': ('entityState_t', named('entityState_s'))}
wire_count = 0
source = (ROOT / 'src/PC/qcommon/msg_mp.c').read_text()
for macro, field in re.findall(r'\{\s*(PSF|CSF|AEF|ESF)\(([^)]+)\)', source):
    ctype, node = wire[macro]
    offset, ty = retail_field(node, field)
    checks.append(f'_Static_assert(offsetof({ctype}, {field}) == {offset}, "wire {ctype}.{field}");')
    width = shape(db, ty)['size']
    checks.append(f'_Static_assert(sizeof((({ctype} *)0)->{field}) == {width}, "wire member width");')
    # name[] and weaponslots[] are intentionally serialized in four-byte chunks.
    checks.append(f'_Static_assert({offset} + 4 <= sizeof({ctype}), "wire chunk bounds");')
    wire_count += 1

# HUD/objective tables still use numeric offsets upstream; these records carry no
# native pointers. Pin those values to both the retail debug layout and the header.
labels = dict(re.findall(r'const char (\w+)\[\] = "([^"]*)";', source))
for table_name, ctype, retail_name in [('hudElemFields', 'hudelem_t', 'hudelem_s'),
                                        ('objectiveFields', 'objective_t', 'objective_t')]:
    table_body = re.search(r'NetField ' + table_name + r'\[\d+\] = \{(.*?)\n\};', source, re.S)[1]
    for label, offset in re.findall(r'\{ \(char \*\)&(\w+), (0x\w+),', table_body):
        field = labels[label]
        retail_offset, ty = retail_field(named(retail_name), field)
        assert retail_offset == int(offset, 16), (table_name, field, retail_offset, offset)
        checks.append(f'_Static_assert(offsetof({ctype}, {field}) == {retail_offset}, "wire {ctype}.{field}");')
        wire_count += 1

# Check every existing typed weapon-table offset against the untouched i386 row.
table = (ROOT / 'src/PC/bgame/bg_weapons_load_obj_weaponDefFields.inc').read_text()
native_table, old_table = re.split(r'^#else$', table, maxsplit=1, flags=re.M)
fields = re.findall(r'offsetof\(WeaponDef, ([^)]+)\)', native_table)
old_rows = re.findall(r'\(const char \*\)str_\w+, \(const char \*\)(0x\w+), \(const char \*\)(0x\w+)', old_table)
assert len(fields) == len(old_rows) == 366
for field, (offset, kind) in zip(fields, old_rows):
    actual, ty = retail_field(named('WeaponDef'), field)
    assert actual == int(offset, 16), (field, actual, offset)

output = args.build.resolve() / 'ws6-tests'
output.mkdir(parents=True, exist_ok=True)
probe = output / 'layouts.c'
probe.write_text('\n'.join(checks) + '\n')
entry = next(e for e in json.loads((args.build / 'compile_commands.json').read_text())
             if e['file'].endswith('/src/PC/server_mp/sv_init_mp.c'))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
result = subprocess.run([*flags, '-fsyntax-only', str(probe)], capture_output=True, text=True)
(output / 'layouts.log').write_text(result.stderr)
if result.returncode:
    print(result.stderr)
    raise SystemExit(result.returncode)
print(f'PASS: {count} native fields; {wire_count} retail network offsets/widths; 366 weapon field mappings')
