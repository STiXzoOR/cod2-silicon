"""Read i386 Mach-O STABS without nm, SDK headers, or third party modules."""
from dataclasses import dataclass, field
from pathlib import Path
import re
import struct


@dataclass
class Type:
    kind: str
    args: tuple = ()
    name: str = ''


@dataclass
class Variable:
    name: str
    type: Type
    address: int
    source: str
    stab: int


class Unsupported(ValueError):
    pass


def macho_symbols(path):
    data = Path(path).read_bytes()
    if len(data) < 28 or struct.unpack_from('<I', data)[0] != 0xfeedface:
        raise ValueError('expected a thin, little-endian 32-bit Mach-O')
    if struct.unpack_from('<I', data, 4)[0] != 7:
        raise ValueError('expected the i386 slice')
    offset = 28
    symtab = None
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        cmd, size = struct.unpack_from('<II', data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError('invalid Mach-O load command')
        if cmd == 2:
            symtab = struct.unpack_from('<4I', data, offset + 8)
        offset += size
    if symtab is None:
        raise ValueError('Mach-O has no LC_SYMTAB')
    symoff, count, stroff, strsize = symtab
    if symoff + count * 12 > len(data) or stroff + strsize > len(data):
        raise ValueError('truncated Mach-O symbol table')
    strings = data[stroff:stroff + strsize]
    for i in range(count):
        index, kind, section, desc, value = struct.unpack_from('<IBBHI', data, symoff + i * 12)
        if index >= len(strings):
            raise ValueError('invalid Mach-O string index')
        end = strings.find(b'\0', index)
        if end < 0:
            raise ValueError('unterminated Mach-O symbol')
        yield strings[index:end].decode('utf-8', 'replace'), kind, section, desc, value


class Grammar:
    def __init__(self, db, scope, text):
        self.db, self.scope, self.text, self.pos = db, scope, text, 0

    def take(self, token):
        if not self.text.startswith(token, self.pos):
            raise Unsupported('expected ' + token)
        self.pos += len(token)

    def number(self):
        m = re.match(r'-?\d+', self.text[self.pos:])
        if not m:
            raise Unsupported('expected integer')
        self.pos += len(m[0])
        return int(m[0])

    def until(self, token):
        end = self.text.find(token, self.pos)
        if end < 0:
            raise Unsupported('unterminated ' + token)
        result = self.text[self.pos:end]
        self.pos = end + len(token)
        return result

    def type(self):
        if self.pos >= len(self.text):
            raise Unsupported('missing type')
        c = self.text[self.pos]
        if c == '(' or c.isdigit() or c == '-':
            if c == '(':
                self.pos += 1
                file = self.number(); self.take(','); num = self.number(); self.take(')')
            else:
                file, num = 0, self.number()
            key = self.db.key(self.scope, file, num)
            if self.text[self.pos:self.pos + 1] == '=':
                self.pos += 1
                node = self.type()
                self.db.types[key] = node
            return Type('ref', (key,))
        self.pos += 1
        if c in '*&kB':
            return Type({'*': 'pointer', '&': 'reference', 'k': 'const', 'B': 'volatile'}[c], (self.type(),))
        if c == 'f':
            return Type('function', (self.type(),))
        if c == '#':
            # C++ method: class, result, argument types, terminated by semicolon.
            result = []
            if self.text[self.pos:self.pos + 1] == '#':
                self.pos += 1
                result.append(self.type()); self.take(';')
            else:
                result.append(self.type())
                while self.text[self.pos:self.pos + 1] == ',':
                    self.pos += 1; result.append(self.type())
                self.take(';')
            return Type('method', tuple(result))
        if c == '@':
            attr = self.until(';')
            return Type('attribute', (attr, self.type()))
        if c == 'r':
            base = self.type(); self.take(';')
            lo = self.until(';'); hi = self.until(';')
            return Type('range', (base, int(lo, 0) if lo.startswith('0x') else int(lo), int(hi)))
        if c == 'a':
            self.take('r'); index = self.type(); self.take(';')
            lo = self.number(); self.take(';'); hi = self.number(); self.take(';')
            return Type('array', (self.type(), lo, hi))
        if c in 'su':
            size = self.number()
            members = []
            # C++ inheritance, methods, and bitfields are retained as unsupported
            # rather than guessed into an ABI-compatible C struct.
            while self.text[self.pos:self.pos + 1] != ';':
                name = self.until(':')
                if self.text[self.pos:self.pos + 1] in ':/':
                    raise Unsupported('C++ member metadata')
                ty = self.type(); self.take(','); bit = self.number(); self.take(',')
                bits = self.number(); self.take(';')
                members.append((name, ty, bit, bits))
            self.take(';')
            return Type('struct' if c == 's' else 'union', (size, tuple(members)))
        if c == 'e':
            values = []
            while self.text[self.pos:self.pos + 1] != ';':
                name = self.until(':'); val = self.number(); self.take(',')
                values.append((name, val))
            self.take(';')
            return Type('enum', tuple(values))
        if c == 'x':
            tag = self.text[self.pos]; self.pos += 1
            return Type('crossref', (tag, self.until(':')))
        raise Unsupported('type descriptor ' + c)


class Database:
    def __init__(self):
        self.types = {}
        self.names = {}
        self.variables = []
        self.errors = []
        self.files = {}
        self.tags = {}
        self.symbols = {}
        self.stab_counts = {}

    def key(self, scope, file, num):
        return (self.files.get((scope, file), (scope, file)), num)

    def read(self, path):
        scope, source, file_no, pending = 0, '', 0, ''
        includes = {}
        for text, kind, section, desc, value in macho_symbols(path):
            if not kind & 0xe0:
                self.symbols[text] = value
                continue
            self.stab_counts[kind] = self.stab_counts.get(kind, 0) + 1
            if kind == 0x64 and text and not text.endswith('/'):
                scope += 1; source = text; file_no = 0
            if kind in (0x82, 0xc2):  # N_BINCL / N_EXCL include-file identity
                file_no += 1
                identity = (text, value)
                if kind == 0x82:
                    includes[identity] = (scope, file_no)
                self.files[scope, file_no] = includes.get(identity, (scope, file_no))
            if text.endswith('\\'):
                pending += text[:-1]
                continue
            text = pending + text; pending = ''
            # Parse every nested definition independently. An unsupported C++
            # class must not hide valid referenced types later in its record.
            for m in re.finditer(r'(\(-?\d+,-?\d+\)|(?<![\w])\d+)=', text):
                parser = Grammar(self, scope, text[m.start():])
                try:
                    ref = parser.type()
                    prefix = text[:m.start()]
                    named = re.search(r'([^:]+):[tT]t?$', prefix)
                    if named:
                        self.names[ref.args[0]] = named[1]
                        node = self.types.get(ref.args[0])
                        if node and node.kind in ('struct', 'union', 'enum'):
                            self.tags.setdefault((scope, named[1]), ref)
                except (Unsupported, ValueError, RecursionError) as error:
                    self.errors.append((scope, str(error)))
            if kind in (0x20, 0x26, 0x28):
                m = re.match(r'(.*):[GSV](.*)', text)
                if m:
                    try:
                        ty = Grammar(self, scope, m[2]).type()
                        self.variables.append(Variable(m[1], ty, value, source, kind))
                    except (Unsupported, ValueError) as error:
                        self.errors.append((scope, str(error)))
        return self

    def resolve(self, ty, seen=None):
        seen = set() if seen is None else seen
        while ty.kind in ('ref', 'const', 'volatile', 'attribute', 'crossref'):
            if ty.kind == 'ref':
                key = ty.args[0]
                if key in seen:
                    raise Unsupported('cyclic/void type')
                seen.add(key)
                ty = self.types.get(key)
                if ty is None and key[1] < 0:
                    builtins = {-1: ('int', 4), -2: ('char', 1), -3: ('short', 2),
                                -4: ('long', 4), -5: ('unsigned char', 1),
                                -6: ('signed char', 1), -7: ('unsigned short', 2),
                                -8: ('unsigned int', 4), -9: ('unsigned long', 4),
                                -10: ('void', 0), -11: ('float', 4), -12: ('double', 8),
                                -13: ('long double', 16), -14: ('long long', 8),
                                -15: ('unsigned long long', 8), -16: ('_Bool', 1)}
                    if key[1] in builtins:
                        ty = Type('builtin', builtins[key[1]])
                if ty is None:
                    raise Unsupported('unresolved type ' + repr(key))
            elif ty.kind == 'crossref':
                local = key[0][0] if seen else None
                matches = [v for (scope, name), v in self.tags.items()
                           if name == ty.args[1] and scope == local]
                if not matches:
                    raise Unsupported('unresolved tag ' + ty.args[1])
                ty = matches[0]
            else:
                ty = ty.args[-1]
        return ty
