#!/usr/bin/env python3
"""Validate reviewed ownerdraw facts against an external owned Mac 1.3 binary."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from macho32 import MachO32
from stabs import Database

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--data', type=Path, help='Optional owned install for menu caller inventory')
args = parser.parse_args()
reference = MachO32(args.binary)
assert hashlib.sha256(reference.data).hexdigest() == '15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12'


def read(address, size):
    section = next(section for section in reference.sections
                   if section.address <= address and address + size <= section.address + section.size)
    offset = section.offset + address - section.address
    return reference.data[offset:offset + size]


def word(address):
    return struct.unpack('<I', read(address, 4))[0]


for draw, address in [(33, 0x18e10c), (86, 0x18d2ff)]:
    assert word(0x33ba90 + (draw - 5) * 4) == address
for address, name in {0x1acd5fd: '_cg', 0x1acd641: '_cgs', 0x1acd649: '_cg_entities',
    0x1acd6bd: '_cg_hudCompassSize', 0x1acd6c1: '_cg_hudCompassMinRange',
    0x1acd6cd: '_cg_hudCompassMinRadius', 0x1acd6d1: '_cg_hudCompassMaxRange',
    0x1acd6dd: '_cg_hudObjectiveMaxRange', 0x1acd6e1: '_cg_hudObjectiveMinAlpha'}.items():
    assert any(symbol[0] == name and symbol[4] == word(address) for symbol in reference.symbols)
for address, value in [(0x326188, 1.0), (0x326190, 0.5), (0x326260, 16.0), (0x3261f4, -0.5)]:
    assert read(address, 4) == struct.pack('<f', value)
assert read(0x340c28, 8) == struct.pack('<d', 0.017453292519943295)

database = Database().read(args.binary)


def variable(name):
    return database.resolve(next(v.type for v in database.variables if v.name == name))


def member(record, name):
    field = next(field for field in record.args[1] if field[0] == name)
    assert field[2] % 8 == 0
    return database.resolve(field[1]), field[2] // 8


cg = database.resolve(variable('cg').args[0])
snapshot_pointer, next_snap = member(cg, 'nextSnap')
assert next_snap == 0x24
snapshot = database.resolve(snapshot_pointer.args[0])
ps, ps_offset = member(snapshot, 'ps')
assert ps_offset + member(ps, 'clientNum')[1] == 0xd8
objectives, objective_offset = member(ps, 'objective')
objective = database.resolve(objectives.args[0])
assert ps_offset + objective_offset == 0x5f0 and objective.args[0] == 28
for name, offset in [('state', 0), ('origin', 4), ('entNum', 16), ('icon', 24)]:
    assert member(objective, name)[1] == offset
refdef, refdef_offset = member(cg, 'refdef')
assert refdef_offset + member(refdef, 'vieworg')[1] == 0x28588
assert member(cg, 'compPointerYaw')[1] == 0x2c5b8
bgs, bgs_offset = member(cg, 'bgs')
client_array, client_offset = member(bgs, 'clientinfo')
client = database.resolve(client_array.args[0])
assert client.args[0] == 1208
assert bgs_offset + client_offset + member(client, 'infoValid')[1] == 0xe0914
assert member(client, 'location')[1] == 0x38
entity = database.resolve(variable('cg_entities').args[0])
assert entity.args[0] == 548 and member(entity, 'lerpOrigin')[1] == 0x1ec
cgs = database.resolve(variable('cgs').args[0])
media, media_offset = member(cgs, 'media')
assert media_offset + member(media, 'objectiveMaterials')[1] == 0x27de8
print('PASS Mac 1.3 ownerdraw dispatch, imports, constants and nested STABS fields')

if args.data:
    callers = {33: [], 86: []}
    for archive_path in sorted((args.data / 'main').glob('*.iwd')):
        with zipfile.ZipFile(archive_path) as archive:
            for name in archive.namelist():
                if not name.lower().endswith('.menu'):
                    continue
                text = archive.read(name).decode('latin1')
                for draw, label in [(33, 'LOCATION'), (86, 'COMPASS_POINTERS')]:
                    if re.search(r'\bownerdraw\s+(?:CG_PLAYER_' + label + '|' + str(draw) + r')\b', text, re.I):
                        callers[draw].append(archive_path.name + ':' + name)
    assert any(name.endswith(':ui_mp/hud.menu') for name in callers[86])
    print('Owned menu callers: ' + ', '.join(str(draw) + '=' + str(len(names)) for draw, names in callers.items()))
    for draw, names in callers.items():
        for name in names:
            print(str(draw) + ' ' + name)
