#!/usr/bin/env python3
"""Recover the Mac's compiled shader cache from a user-supplied i386 binary.

The output contains licensed game assets. Keep it outside the repository.
No shader programs or binary-derived payloads are distributed by this tool.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile

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


def verify_cache(output, expected_count=834):
    """Trust the cache only after checking every manifest entry's bytes."""
    try:
        manifest = json.loads((output / 'manifest.json').read_text())
        assets = manifest['assets']
        if not isinstance(assets, dict):
            return False
        if not re.fullmatch(r'[0-9a-f]{64}', manifest['binary_sha256']) or len(assets) != expected_count:
            return False
        for name, digest in assets.items():
            if not re.fullmatch(r'[A-Za-z_0-9]+\.(vsa|pse|vc|pc)', name):
                return False
            if not re.fullmatch(r'[0-9a-f]{64}', digest):
                return False
            if hashlib.sha256((output / name).read_bytes()).hexdigest() != digest:
                return False
            if name.endswith(('.vsa', '.pse')) and name[:-2] + 'c' not in assets:
                return False
        return True
    except (OSError, ValueError, KeyError, TypeError):
        return False


def write_cache(binary, output):
    print(f'CoD2x: extracting shaders from {binary}', flush=True)
    assets = extract(binary)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='shaders-', dir=output.parent) as directory:
        staging = Path(directory)
        for index, (name, data) in enumerate(sorted(assets.items()), 1):
            (staging / name).write_bytes(data)
            if index % 100 == 0 or index == len(assets):
                print(f'CoD2x: shader setup {index}/{len(assets)}', flush=True)
        manifest = dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                        assets={name: hashlib.sha256(data).hexdigest() for name, data in sorted(assets.items())})
        (staging / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        if not verify_cache(staging):
            raise ValueError('extracted shader manifest failed verification')
        output.mkdir(parents=True, exist_ok=True)
        for file in staging.iterdir():
            os.replace(file, output / file.name)
    print(f'CoD2x: verified {len(assets)} shader/constant files in {output}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--verify', action='store_true', help='verify existing cache without extraction')
    parser.add_argument('--setup', action='store_true', help='reuse a verified cache; extract once if absent or damaged')
    parser.add_argument('--fallback', type=Path, help='alternate licensed binary if the primary cannot be extracted')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.expanduser().resolve()
    if output == root or root in output.parents:
        parser.error('licensed shader cache must be outside the repository')
    if args.verify:
        return 0 if verify_cache(output) else 1
    output.parent.mkdir(parents=True, exist_ok=True)
    with (output.parent / '.shader-setup.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if args.setup and verify_cache(output):
            print(f'CoD2x: shader cache SHA-256 manifest verified: {output}', flush=True)
            return 0
        for binary in [args.binary, args.fallback]:
            if binary is None:
                continue
            try:
                write_cache(binary.expanduser(), output)
                return 0
            except (OSError, ValueError, StopIteration, struct.error) as error:
                print(f'CoD2x: shader extraction unavailable from {binary}: {error}', file=sys.stderr, flush=True)
        print('CoD2x: no verified shader cache; using approximation rendering.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
