#!/usr/bin/env python3
"""Recover the Mac's compiled shader cache from a user-supplied i386 binary.

The output contains licensed game assets. Keep it outside the repository.
No shader programs or binary-derived payloads are distributed by this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'datagen'))
from macho32 import MachO32


def constant_table(text):
    tokens = text.split()
    count = int(tokens[0])
    if not 0 <= count <= 256 or len(tokens) != 1 + count * 6:
        raise ValueError('invalid Mac constant metadata')
    data = bytearray(struct.pack('<7I', 28, 0, 0, count, 28, 0, 0))
    data.extend(bytes(count * 36))  # 20-byte constant + 16-byte type records
    for i in range(count):
        name = tokens[1 + i * 6]
        register, size, cls, ty, rows = map(int, tokens[2 + i * 6:7 + i * 6])
        if not re.fullmatch(rb'[A-Za-z_][A-Za-z_0-9]*', name):
            raise ValueError('invalid constant name')
        if not all(0 <= n <= 65535 for n in (register, size, cls, ty, rows)):
            raise ValueError('invalid constant field')
        type_offset = 28 + count * 20 + i * 16
        struct.pack_into('<I4H2I', data, 28 + i * 20, len(data), 0, register, size, 0, type_offset, 0)
        # Matches CD3DXConstantTable's serialized CTAB construction, not HLSL reflection.
        struct.pack_into('<6HI', data, type_offset, cls, ty, 0, 0, rows, 0, 0)
        data.extend(name + b'\0')
    return bytes(data)


def extract(binary):
    macho = MachO32(binary)
    address = next(s[4] for s in macho.symbols if s[0] == '_D3DXCompileShader')
    section = next(s for s in macho.sections if s.name == '__text' and s.address <= address < s.address + s.size)
    end = min(s[4] for s in macho.symbols if s[2] == macho.sections.index(section) + 1 and s[4] > address)
    offset = section.offset + address - section.address
    code = macho.data[offset:offset + end - address]
    cstrings = next(s for s in macho.sections if s.name == '__cstring')

    def string(pointer):
        if not cstrings.address <= pointer < cstrings.address + cstrings.size:
            return b''
        start = cstrings.offset + pointer - cstrings.address
        stop = macho.data.index(b'\0', start, cstrings.offset + cstrings.size)
        return macho.data[start:stop]

    # Each map insertion constructs the value, then its filename. Verify both
    # strings, rather than assuming neighboring __cstring contents belong together.
    refs = [struct.unpack('<I', m.group(1))[0]
            for m in re.finditer(rb'\xc7\x44\x24\x04(.{4})', code, re.DOTALL)]
    assets = {}
    for previous, current in zip(refs, refs[1:]):
        name = string(current)
        if not re.fullmatch(rb'[A-Za-z_0-9]+\.(vsa|pse|vc|pc)', name):
            continue
        payload = string(previous)
        if name.endswith(b'.vsa') and not payload.startswith(b'!!ARBvp1.0'):
            raise ValueError('invalid vertex program: ' + name.decode())
        if name.endswith(b'.pse') and not payload.startswith(b'!!ARBfp1.0'):
            raise ValueError('invalid fragment program: ' + name.decode())
        if name.endswith((b'.vc', b'.pc')):
            payload = constant_table(payload)
        key = name.decode()
        if key in assets and assets[key] != payload:
            raise ValueError('duplicate shader: ' + key)
        assets[key] = payload
    if len(assets) < 800:
        raise ValueError('incomplete shader map; expected the symbolized Mac 1.3 binary')
    for name in assets:
        if name.endswith(('.vsa', '.pse')) and name[:-2] + 'c' not in assets:
            raise ValueError('missing constant table: ' + name)
    return assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.expanduser().resolve()
    if output == root or root in output.parents:
        parser.error('licensed shader cache must be outside the repository')
    assets = extract(args.binary)
    output.mkdir(parents=True, exist_ok=True)
    for name, data in sorted(assets.items()):
        (output / name).write_bytes(data)
    manifest = dict(binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                    assets={name: hashlib.sha256(data).hexdigest() for name, data in sorted(assets.items())})
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Extracted {len(assets)} shader/constant files to {output}')


if __name__ == '__main__':
    main()
