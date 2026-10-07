#!/usr/bin/env python3
"""Check private Mac 1.3 HUD constants/layouts without retaining extracted data."""
import argparse
import hashlib
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from macho32 import MachO32
from stabs import Database, Type

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
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


imports = {0x1acd84d: '_cg_drawTurretCrosshair', 0x1acd7cd: '_cg_paused',
           0x1acd5fd: '_cg', 0x1acd649: '_cg_entities', 0x1acd645: '_cg_weapons',
           0x1acd869: '_cg_crosshairAlpha', 0x1acd805: '_cg_descriptiveText'}
for address, name in imports.items():
    assert any(symbol[0] == name and symbol[4] == word(address) for symbol in reference.symbols)
for address, expected in [(0x3262f0, 0.01), (0x3261f4, -0.5), (0x3261e0, -2.0), (0x3265e8, 436.0)]:
    assert read(address, 4) == struct.pack('<f', expected)
# The instruction at this reviewed site loads the spectator scale immediate.
assert word(0x1cff00) == 0x3e555555
for address, expected in [(0x2d2ee8, 'KEY_UNBOUND'), (0x2d79dc, '+melee'),
    (0x2e2ea0, 'PLATFORM_FOLLOWSTOP'), (0x2d78e8, '+attack'),
    (0x2e2e64, 'PLATFORM_FOLLOWNEXTPLAYER'), (0x2d7a64, 'toggleads'),
    (0x2e2e80, 'PLATFORM_FOLLOWPREVIOUSPLAYER'), (0x2d79f0, '+melee_breath'), (0x2d7860, '+speed')]:
    assert read(address, len(expected) + 1) == expected.encode() + b'\0'

database = Database().read(args.binary)
cg_pointer = database.resolve(next(variable.type for variable in database.variables if variable.name == 'cg'))
cg_type = database.resolve(cg_pointer.args[0])


def member(record, name):
    field = next(field for field in record.args[1] if field[0] == name)
    assert field[2] % 8 == 0
    return database.resolve(field[1]), field[2] // 8


ps, predicted_offset = member(cg_type, 'predictedPlayerState')
assert predicted_offset == 0x25bc4
assert member(ps, 'pm_flags')[1] + predicted_offset == 0x25bd0
assert member(ps, 'viewlocked_entNum')[1] + predicted_offset == 0x26158
snapshot_pointer, _ = member(cg_type, 'nextSnap')
snapshot = database.resolve(snapshot_pointer.args[0])
snapshot_ps, snapshot_ps_offset = member(snapshot, 'ps')
assert snapshot_ps_offset + member(snapshot_ps, 'pm_flags')[1] == 0x18
entities_pointer = database.resolve(next(variable.type for variable in database.variables if variable.name == 'cg_entities'))
entity = database.resolve(entities_pointer.args[0])
state, state_offset = member(entity, 'nextState')
assert entity.args[0] == 548 and state_offset + member(state, 'weapon')[1] == 0x1b8
weapons_pointer = database.resolve(next(variable.type for variable in database.variables if variable.name == 'cg_weapons'))
weapon_info = database.resolve(weapons_pointer.args[0])
assert weapon_info.args[0] == 436 and member(weapon_info, 'hReticleCenter')[1] == 0x178
weapon_definition = database.resolve(next(Type('ref', (key,)) for key, name in database.names.items()
                                          if name == 'WeaponDef'))
assert member(weapon_definition, 'iReticleCenterSize')[1] == 0x120
print('PASS owned Mac 1.3 HUD import identities, literals and nested STABS field offsets')
