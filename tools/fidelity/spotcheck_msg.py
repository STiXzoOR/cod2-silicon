#!/usr/bin/env python3
"""Exercise production keyed-field decoding against runtime reference mask data."""
import argparse
import json
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile

from spotcheck_reverb import extract_function
from tables import read_native

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/datagen'))
from arm64_test import MachO64
from macho32 import MachO32


def test_inputs(reference, native):
    """Choose the first differing mask bit; unchanged fields use an ordinary key."""
    for bits in range(1, 33):
        different = reference[bits] ^ native[bits]
        key = different & -different if different else 0x12345678
        # MSG_WriteBits writes only the low `bits` bits of (newValue ^ key).
        value = 1
        wire = (value ^ key) & ((1 << bits) - 1)
        yield bits, key, wire, wire ^ (key & reference[bits])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
    parser.add_argument('--binary', type=Path, default=Path.home() / 'Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect-difference', action='store_true')
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error('Keep observations outside git')
    reference_data, meta = MachO32(args.binary).read_object('kbitmask', 33 * 4)
    reference = struct.unpack('<33I', reference_data)
    rows = json.loads((args.build / 'compile_commands.engine.json').read_text())
    definitions = []
    for entry in rows:
        command = shlex.split(entry['command'])
        if 'cod2_macos.dir' not in entry['command'] or '-o' not in command:
            continue
        path = Path(entry['directory']) / command[command.index('-o') + 1]
        try:
            obj = MachO64(path)
        except (OSError, ValueError):
            continue
        if '_kbitmask' in obj.symbols and obj.symbols['_kbitmask'][0]:
            definitions.append(read_native(obj, '_kbitmask', 33 * 4))
    if len(definitions) != 1:
        raise ValueError('Expected exactly one built native mask table')
    native = struct.unpack('<33I', definitions[0])
    source_path = ROOT / 'src/PC/qcommon/msg_mp.c'
    entry = next(r for r in rows if Path(r['file']) == source_path and 'cod2_macos.dir' in r['command'])
    command = shlex.split(entry['command'])
    flags = command[:command.index('-o')]
    cases = list(test_inputs(reference, native))
    prelude = '''#include "common_types.h"
#include "imports.h"
#include <stdio.h>
static unsigned wire;
int MSG_ReadBit(msg_t *msg) { return 1; }
int MSG_ReadBits(msg_t *msg, int bits) { return (int)wire; }
'''
    table = 'unsigned int kbitmask[33] = {' + ','.join(map(str, native)) + '};\n'
    body = extract_function(source_path.read_text(), 'MSG_ReadDeltaKeyBits')
    driver = '\nint main(void) { msg_t msg = {0};\n' + '\n'.join(
        f'wire = {wire}u; printf("%d\\n", MSG_ReadDeltaKeyBits(&msg, (int){key}u, 0, {bits}));'
        for bits, key, wire, _ in cases) + '\nreturn 0; }\n'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as tmp:
        source, exe = Path(tmp) / 'probe.c', Path(tmp) / 'probe'
        source.write_text(prelude + table + body + driver)
        subprocess.run(flags + [str(source), '-o', str(exe)], cwd=entry['directory'], check=True, capture_output=True)
        observations = [int(n) & 0xffffffff for n in subprocess.check_output([str(exe)], text=True).splitlines()]
    result = dict(reference_table_address=hex(meta['address']), source=str(source_path.relative_to(ROOT)),
                  function='MSG_ReadDeltaKeyBits', reference_caller_address='0x1757bc',
                  total=len(cases), differences=sum(actual != case[3] for actual, case in zip(observations, cases)),
                  cases=[dict(bits=bits, key=key, wire=wire, reference=expected, actual=actual, different=actual != expected)
                         for (bits, key, wire, expected), actual in zip(cases, observations)])
    if len(observations) != len(cases):
        raise ValueError('Incomplete harness output')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'cases'}, indent=2))
    return 0 if bool(result['differences']) == args.expect_difference else 1


if __name__ == '__main__':
    raise SystemExit(main())
