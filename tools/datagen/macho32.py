"""Read named data from a user-supplied i386 Mach-O without address guessing."""
from dataclasses import dataclass
from pathlib import Path
import struct

from stabs import macho_symbols


@dataclass
class Section:
    segment: str
    name: str
    address: int
    size: int
    offset: int
    flags: int


class MachO32:
    def __init__(self, path):
        self.data = data = Path(path).read_bytes()
        # macho_symbols validates the header and LC_SYMTAB before using nlist.
        self.symbols = [s for s in macho_symbols(path)
                        if not s[1] & 0xe0 and s[1] & 0xe == 0xe]
        self.sections = []
        offset = 28
        for _ in range(struct.unpack_from('<I', data, 16)[0]):
            command, size = struct.unpack_from('<II', data, offset)
            if command == 1:  # LC_SEGMENT, followed by section records
                count = struct.unpack_from('<I', data, offset + 48)[0]
                if size < 56 + count * 68:
                    raise ValueError('truncated Mach-O sections')
                for index in range(count):
                    s = struct.unpack_from('<16s16s9I', data, offset + 56 + index * 68)
                    self.sections.append(Section(s[1].split(b'\0')[0].decode(),
                                                 s[0].split(b'\0')[0].decode(),
                                                 s[2], s[3], s[4], s[8]))
            offset += size

    def named_object(self, name):
        # Mach-O's platform underscore is distinct from Itanium's underscore.
        # Match globals, file-local statics, and function-local static suffixes.
        exact = '_' + name
        local = '__ZL' + str(len(name)) + name
        suffix = 'E' + str(len(name)) + name
        matches = [s for s in self.symbols if s[0] in (exact, local)
                   or (s[0].startswith('__ZZ') and s[0].endswith(suffix))]
        identities = {(s[2], s[4]) for s in matches}
        if len(identities) != 1:
            raise ValueError('%s: expected one named reference object, found %d' % (name, len(identities)))
        symbol, _, index, _, address = min(matches)
        if not 0 < index <= len(self.sections):
            raise ValueError(name + ': invalid reference section')
        section = self.sections[index - 1]
        if section.segment not in ('__DATA', '__TEXT') or section.name == '__text':
            raise ValueError(name + ': expected data in __DATA or __TEXT')
        end = min((s[4] for s in self.symbols if s[2] == index and s[4] > address),
                  default=section.address + section.size)
        if not section.address <= address < end <= section.address + section.size:
            raise ValueError(name + ': reference object exceeds section')
        return symbol, section, address, end - address

    def read_object(self, name, size):
        symbol, section, address, extent = self.named_object(name)
        if size > extent:
            raise ValueError('%s: debug size %d exceeds next-symbol extent %d' % (name, size, extent))
        if section.flags & 0xff in (1, 12):  # S_ZEROFILL / S_GB_ZEROFILL
            data = bytes(size)
        else:
            offset = section.offset + address - section.address
            data = self.data[offset:offset + size]
            if len(data) != size:
                raise ValueError(name + ': truncated reference data')
        return data, dict(symbol=symbol, segment=section.segment, section=section.name,
                          address=address, next_symbol_extent=extent, value_bytes=size)
