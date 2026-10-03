#!/usr/bin/env python3
"""Inspect compiler output for two concrete LP64 layout corrections."""
import argparse
import json
from pathlib import Path
import struct


class MachO64:
    def __init__(self, path):
        self.data = data = Path(path).read_bytes()
        magic, cpu, subtype, filetype, count, size, flags, reserved = struct.unpack_from('<8I', data)
        if (magic, cpu, filetype) != (0xfeedfacf, 0x100000c, 1):
            raise ValueError('expected an arm64 Mach-O object')
        offset, self.sections = 32, []
        for _ in range(count):
            cmd, length = struct.unpack_from('<II', data, offset)
            if cmd == 0x19:
                nsects = struct.unpack_from('<I', data, offset + 64)[0]
                for index in range(nsects):
                    section = struct.unpack_from('<16s16sQQIIIIIIII', data, offset + 72 + 80 * index)
                    self.sections.append(section)
            elif cmd == 2:
                symoff, nsym, stroff, strsize = struct.unpack_from('<4I', data, offset + 8)
            offset += length
        strings = data[stroff:stroff + strsize]
        self.symbols = {}
        for index in range(nsym):
            name, kind, section, desc, value = struct.unpack_from('<IBBHQ', data, symoff + index * 16)
            if name:
                name = strings[name:strings.index(0, name)].decode()
                self.symbols[name] = (section, value, index)

    def pointer_addend(self, name):
        index, value, _ = self.symbols[name]
        section = self.sections[index - 1]
        return struct.unpack_from('<Q', self.data, section[4] + value - section[2])[0]

    def first_relocation_offset(self, name, extent):
        index, value, _ = self.symbols[name]
        section = self.sections[index - 1]
        offsets = []
        for i in range(section[7]):
            address, bits = struct.unpack_from('<II', self.data, section[6] + 8 * i)
            if value - section[2] <= address < value - section[2] + extent:
                if (bits >> 25) & 3 != 3:
                    raise AssertionError('expected an 8-byte arm64 relocation')
                offsets.append(address - (value - section[2]))
        return min(offsets)


def check_recovered_values(directory, values_binary, records):
    from macho32 import MachO32
    reference = MachO32(values_binary)
    objects = {}
    for record in records:
        group, name = record['group'], record['name']
        if group not in objects:
            objects[group] = MachO64(directory / ('arm64-' + group + '.o'))
        obj = objects[group]
        index, address, _ = obj.symbols['_' + name]
        section = obj.sections[index - 1]
        offset = section[4] + address - section[2]
        truth, _ = reference.read_object(name, record['value_bytes'])
        if name == 'infoParms':
            # Its name pointer grows from 4 to 8 bytes; four int flags retain
            # their exact bits. The engine declares 54 entries with 24-byte
            # naturally aligned LP64 rows, including two zero sentinels.
            emitted = obj.data[offset:offset + 54 * 24]
            matches = all(emitted[i*24 + 8:i*24 + 24] == truth[i*20 + 4:i*20 + 20]
                          for i in range(54))
        else:
            matches = obj.data[offset:offset + len(truth)] == truth
        if not matches:
            raise ValueError(group + ':' + name + ': compiled LP64 scalar bytes differ from Steam')
    return len(records)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--values-binary', type=Path)
    args = parser.parse_args()
    old_data = MachO64(args.directory / 'arm64-reference-native-data.o')
    new_data = MachO64(args.directory / 'arm64-data_native.o')
    old_literal = MachO64(args.directory / 'arm64-reference-native-literals.o')
    new_literal = MachO64(args.directory / 'arm64-literals_native.o')
    assert old_data.pointer_addend('_keys') == 292
    assert new_data.pointer_addend('_keys') == 296
    assert old_literal.first_relocation_offset('___ZTV12CVertexArray', 128) == 8
    assert new_literal.first_relocation_offset('___ZTV12CVertexArray', 256) == 16
    print('arm64 keys -> playerKeys.keys: legacy addend 292, typed addend 296')
    print('arm64 vtable first function slot: legacy offset 8, typed offset 16')
    if args.values_binary:
        records = json.loads((args.directory / 'coverage.json').read_text())['recovered_values']
        count = check_recovered_values(args.directory, args.values_binary, records)
        print('%d recovered LP64 objects: compiled scalar bytes identical to Steam' % count)
