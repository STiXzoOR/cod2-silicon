#!/usr/bin/env python3
"""Verify 1.3 facts from local licensed binaries; write no extracted binary data."""
import argparse
from collections import defaultdict
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from layout import shape
from stabs import Database, Unsupported

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=Path.home() / 'Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386')
parser.add_argument('--steam', type=Path, default=Path.home() / 'Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer')
args = parser.parse_args()
missing = [str(path) for path in [args.binary, args.steam] if not path.exists()]
if missing:
    print('SKIP 1.3 reference facts: missing private input: ' + ', '.join(missing))
    raise SystemExit(0)


class Image:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.db = Database().read(path)
        self.sections = []
        offset = 28
        for _ in range(struct.unpack_from('<I', self.data, 16)[0]):
            cmd, size = struct.unpack_from('<II', self.data, offset)
            if cmd == 1:
                for i in range(struct.unpack_from('<I', self.data, offset + 48)[0]):
                    section = struct.unpack_from('<16s16s9I', self.data, offset + 56 + i * 68)
                    self.sections.append((section[2], section[3], section[4]))
            offset += size

    def address(self, name):
        return self.db.symbols['_' + name]

    def at(self, address, length):
        for start, size, offset in self.sections:
            if start <= address and address + length <= start + size:
                return self.data[offset + address - start:offset + address - start + length]
        raise ValueError(hex(address))

    def pointer(self, address):
        return struct.unpack('<I', self.at(address, 4))[0]

    def string(self, address):
        return self.at(address, 256).split(b'\0', 1)[0]


retail, steam = Image(args.binary), Image(args.steam)
byname = defaultdict(list)
for variable in retail.db.variables:
    byname[variable.name].append(variable)


def sizes(name):
    result = set()
    for variable in byname[name]:
        try:
            result.add(shape(retail.db, variable.type)['size'])
        except (Unsupported, RecursionError):
            pass
    return result


for name, size in [('sys_packetReceived', 131072), ('g_largeLocalBuf', 1048576),
                   ('svs', 110844), ('cls', 2837904), ('ucmds', 104),
                   ('serverStatusDvars', 288), ('cg_shock_dvar_names', 116)]:
    assert sizes(name) == {size}, (name, sizes(name))
    print(f'Mac 1.3 {name}: {retail.address(name):#x}, typed bytes {size}')

# Audit only unambiguous byte-array types; ambiguous/unavailable STABS are reported.
source = (ROOT / 'src/blobs/bss.c').read_text()
source = re.sub(r'^\s*#\s*include[^\n]*', '', source, flags=re.M)
active = subprocess.run(['clang', '-E', '-P', '-x', 'c', '-', '-DCOD2_X64=1',
                         '-DCOD2_IS_PATCH_13=1', '-DMAX_MSGLEN=131072'],
                        input=source, text=True, capture_output=True, check=True).stdout
checked, skipped = 0, 0
for name, length in re.findall(r'^\s*(?:unsigned char|byte|char)\s+(\w+)\[(0x[0-9a-fA-F]+|\d+)\];', active, re.M):
    extents = sizes(name) or sizes(re.sub(r'_[0-9a-f]{8}$', '', name))
    if len(extents) != 1:
        skipped += 1
        continue
    expected = next(iter(extents))
    assert int(length, 0) >= expected, (name, length, expected)
    checked += 1
print(f'PASS active byte-array BSS audit: {checked} checked, {skipped} unavailable/ambiguous')

values = bytes([0xb5, 0xbf, 0xdf, 0xe0, 0xe1, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
                0xe9, 0xec, 0xf1, 0xf2, 0xf3, 0xf6, 0xf8, 0xf9, 0xfa, 0xfc])
for label, image in [('Mac 2006', retail), ('Steam 2013', steam)]:
    table = image.address('keynames_localized')
    keys = {}
    for i in range(100):
        pointer, key = struct.unpack('<II', image.at(table + i * 8, 8))
        if not pointer:
            break
        keys[key] = image.string(pointer)
    assert b''.join(keys[n] for n in range(0x80, 0x94)) == values
    french = image.address('frenchNumberKeysMap')
    for index, value in [(0, 0xe0), (2, 0xe9), (7, 0xe8), (9, 0xe7)]:
        assert image.string(image.pointer(french + index * 4)) == bytes([value])
    status = image.address('serverStatusDvars')
    assert image.string(image.pointer(status + 22 * 12)) == b'sv_punkbuster'
    assert image.pointer(status + 23 * 12) == 0
    print(f'PASS {label}: all 20 key bytes, French keys, 24-row status table')

commands = retail.address('ucmds')
assert retail.string(retail.pointer(commands + 9 * 8)) == b'wwwdl'
assert retail.pointer(commands + 12 * 8) == 0
shock = retail.address('cg_shock_dvar_names')
assert retail.string(retail.pointer(shock + 4 * 4)) == b'cg_shock_sound'
assert retail.string(retail.pointer(shock + 28 * 4)) == b'cg_shock_mouse_fadeTime'
print('PASS Mac 1.3: 13 command slots, 29 shellshock names')
