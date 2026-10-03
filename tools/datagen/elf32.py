"""Minimal ELF32/i386 object reader for byte and relocation round trips."""
from dataclasses import dataclass
from pathlib import Path
import struct


@dataclass
class Section:
    name: str
    type: int
    flags: int
    address: int
    offset: int
    size: int
    link: int
    info: int
    align: int
    entsize: int
    data: bytes = b''


@dataclass
class Symbol:
    name: str
    value: int
    size: int
    info: int
    other: int
    section: int


@dataclass
class Object:
    name: str
    offset: int
    data: bytes
    relocs: dict
    section: str
    align: int = 4
    aliases: tuple = ()
    global_symbol: bool = True
    origin: str = ''


class ELF:
    def __init__(self, path):
        self.path = str(path)
        data = Path(path).read_bytes()
        if data[:7] != b'\x7fELF\x01\x01\x01':
            raise ValueError('expected little-endian ELF32')
        h = struct.unpack_from('<HHIIIIIHHHHHH', data, 16)
        if h[0:2] != (1, 3):
            raise ValueError('expected an i386 relocatable object')
        shoff, shentsize, shnum, names = h[5], h[10], h[11], h[12]
        headers = [struct.unpack_from('<10I', data, shoff + i * shentsize) for i in range(shnum)]
        nh = headers[names]; strings = data[nh[4]:nh[4] + nh[5]]
        self.sections = []
        for sh in headers:
            section = Section(self.string(strings, sh[0]), *sh[1:])
            section.data = data[section.offset:section.offset + section.size] if section.type != 8 else bytes(section.size)
            self.sections.append(section)
        self.symbols = []
        for section in self.sections:
            if section.type == 2:
                strings = self.sections[section.link].data
                for off in range(0, section.size, section.entsize):
                    name, value, size, info, other, index = struct.unpack_from('<IIIBBH', section.data, off)
                    self.symbols.append(Symbol(self.string(strings, name), value, size, info, other, index))
        self.relocs = {}
        for section in self.sections:
            if section.type == 9:
                entries = self.relocs.setdefault(section.info, {})
                for off in range(0, section.size, section.entsize):
                    address, info = struct.unpack_from('<II', section.data, off)
                    if info & 255 != 1:
                        raise ValueError('expected R_386_32')
                    symbol = self.symbols[info >> 8]
                    addend = struct.unpack_from('<i', self.sections[section.info].data, address)[0]
                    target = self.sections[symbol.section].name if symbol.info & 15 == 3 else symbol.name
                    entries[address] = (target, addend)

    @staticmethod
    def string(data, offset):
        return data[offset:data.index(0, offset)].decode()

    def objects(self):
        result = []
        for index, section in enumerate(self.sections):
            if not section.flags & 2 or section.flags & 4 or not section.size:
                continue
            at_offset = {}
            for sym in self.symbols:
                if sym.section == index and sym.name and (sym.info & 15) in (0, 1):
                    at_offset.setdefault(sym.value, []).append(sym)
            offsets = sorted(at_offset)
            for i, offset in enumerate(offsets):
                symbols = at_offset[offset]
                sym = min(symbols, key=lambda s: (not bool(s.size), not s.name.startswith('imp_'), s.name))
                end = offsets[i + 1] if i + 1 < len(offsets) else section.size
                if end <= offset:
                    raise ValueError('empty blob symbol: ' + sym.name)
                relocs = {off - offset: r for off, r in self.relocs.get(index, {}).items() if offset <= off < end}
                aliases = tuple(sorted(s.name for s in symbols if s is not sym))
                result.append(Object(sym.name, offset, section.data[offset:end], relocs, section.name,
                                     aliases=aliases, global_symbol=bool(sym.info >> 4), origin=self.path))
        return result

    def image(self):
        """Include all allocated bytes and normalized relocations; ignore metadata."""
        result = {}
        for i, section in enumerate(self.sections):
            if section.flags & 2 and section.size:
                result[section.name] = (section.data, self.relocs.get(i, {}))
        return result
