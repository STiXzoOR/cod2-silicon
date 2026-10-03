#!/usr/bin/env python3
"""Reject any arm64 pointer-sized data relocation that is not 8-byte aligned."""
import argparse
from pathlib import Path
import struct

from arm64_test import MachO64


def check_alignment(path):
    obj = MachO64(path)
    count, failures = 0, []
    for index, section in enumerate(obj.sections, 1):
        if section[1].split(b'\0')[0] == b'__DWARF' or section[8] & 0x02000000:
            continue  # Debug addresses are not runtime pointer fixups.
        for i in range(section[7]):
            offset, bits = struct.unpack_from('<II', obj.data, section[6] + 8 * i)
            length = (bits >> 25) & 3
            kind = bits >> 28
            if length != 3 or kind != 0:  # ARM64_RELOC_UNSIGNED, eight bytes
                continue
            count += 1
            if (section[2] + offset) % 8:
                symbols = [(value, name) for name, (sec, value, _) in obj.symbols.items()
                           if sec == index and value <= section[2] + offset]
                value, name = max(symbols, key=lambda s: (s[0], s[1].startswith('_')),
                                  default=(section[2], section[0].split(b'\0')[0].decode()))
                failures.append('%s+%#x' % (name, section[2] + offset - value))
    if failures:
        raise ValueError('%s: unaligned LP64 pointer relocations: %s' % (path, ', '.join(failures)))
    return count


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('objects', type=Path, nargs='+')
    args = parser.parse_args()
    for path in args.objects:
        print('%s: %d aligned LP64 pointer relocations' % (path, check_alignment(path)))
