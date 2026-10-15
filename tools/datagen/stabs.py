"""Read i386 Mach-O STABS without nm, SDK headers, or third party modules."""
from dataclasses import dataclass
from pathlib import Path
import re
import struct


@dataclass
class Type:
    kind: str
    args: tuple = ()
    name: str = ''
    cpp: 'CppInfo' = None


@dataclass(frozen=True)
class CppBase:
    type: Type
    virtual: bool
    access: str
    bit_offset: int


@dataclass(frozen=True)
class CppMethod:
    name: str
    type: Type
    linkage: str
    access: str
    qualifier: str
    dispatch: str
    virtual_index: int = None
    defining_class: Type = None


@dataclass(frozen=True)
class CppStaticMember:
    name: str
    type: Type
    linkage: str
    access: str


@dataclass(frozen=True)
class CppInfo:
    aggregate_kind: str
    bases: tuple = ()
    methods: tuple = ()
    static_members: tuple = ()
    member_access: tuple = ()
    vtable_base: Type = None


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

    def choice(self, allowed, label):
        char = self.text[self.pos:self.pos + 1]
        if not char or char not in allowed:
            raise Unsupported('unsupported ' + label)
        self.pos += 1
        return char

    def access(self):
        return {'0': 'private', '1': 'protected', '2': 'public'}[self.choice('012', 'C++ access')]

    def starts_type_number(self):
        char = self.text[self.pos:self.pos + 1]
        return bool(char) and (char in '(-' or char.isdigit())

    def methods(self, name):
        methods = []
        while True:
            ty = self.type(linkage_context=True); self.take(':')
            linkage = self.until(';')
            access = self.access()
            qualifier = {'A': '', 'B': 'const', 'C': 'volatile', 'D': 'const volatile'}[
                self.choice('ABCD', 'C++ method qualifier')]
            dispatch = self.choice('.*?', 'C++ method dispatch')
            index, defining_class = None, None
            if dispatch == '*':
                index = self.number(); self.take(';')
                defining_class = self.type(); self.take(';')
            methods.append(CppMethod(name, ty, linkage, access, qualifier,
                                     {'.': 'nonvirtual', '*': 'virtual', '?': 'static'}[dispatch], index, defining_class))
            # Overloads may follow the dispatch marker directly. Some
            # producers also place a semicolon between overloads.
            if self.text[self.pos:self.pos + 1] == ';':
                self.pos += 1
                if not self.starts_type_number():
                    return methods
            elif not self.starts_type_number():
                raise Unsupported('expected C++ method terminator')

    def aggregate(self, descriptor):
        size = self.number()
        kind = 'struct' if descriptor == 's' else 'union'
        members, bases, methods, static_members, member_access = [], [], [], [], []
        cpp_storage = False
        if self.text[self.pos:self.pos + 1] == '!':
            self.pos += 1
            count = self.number(); self.take(',')
            if count < 0:
                raise Unsupported('negative C++ base count')
            for _ in range(count):
                virtual = self.choice('01', 'C++ base virtual flag') == '1'
                access = self.access()
                bit = self.number(); self.take(',')
                ty = self.type(); self.take(';')
                bases.append(CppBase(ty, virtual, access, bit))
            cpp_storage = bool(bases)
        while self.text[self.pos:self.pos + 1] != ';':
            name = self.until(':')
            if self.text[self.pos:self.pos + 1] == ':':
                self.pos += 1
                methods.extend(self.methods(name))
                continue
            access = 'public'
            if self.text[self.pos:self.pos + 1] == '/':
                self.pos += 1
                access = self.access()
                member_access.append((name, access))
            ty = self.type(linkage_context=True)
            if self.text[self.pos:self.pos + 1] == ':':
                self.pos += 1
                linkage = self.until(';')
                if not linkage:
                    raise Unsupported('missing C++ static member linkage')
                static_members.append(CppStaticMember(name, ty, linkage, access))
                continue
            virtual_field = name.startswith(('$vf', '$vb', '_vptr$', '_vptr.'))
            self.take(','); bit = self.number()
            bits = None
            if self.text[self.pos:self.pos + 1] == ',':
                self.pos += 1
                bits = self.number()
            elif not virtual_field:
                raise Unsupported('missing member bit size')
            self.take(';')
            members.append((name, ty, bit, bits))
            cpp_storage |= virtual_field
        self.take(';')
        vtable_base = None
        if self.text.startswith('~%', self.pos):
            self.pos += 2
            vtable_base = self.type(); self.take(';')
        cpp_storage |= any(method.dispatch == 'virtual' for method in methods) or vtable_base is not None
        info = None
        if bases or methods or static_members or member_access or cpp_storage:
            info = CppInfo(kind, tuple(bases), tuple(methods), tuple(static_members), tuple(member_access), vtable_base)
        # Base subobjects and virtual storage require a C++ ABI model. Keep
        # their metadata while making the C layout consumer reject the value.
        return Type('cpp_class' if cpp_storage else kind, (size, tuple(members)), cpp=info)

    def type(self, linkage_context=False):
        if self.pos >= len(self.text):
            raise Unsupported('missing type')
        c = self.text[self.pos]
        if self.starts_type_number():
            if c == '(':
                self.pos += 1
                file = self.number(); self.take(','); num = self.number(); self.take(')')
            else:
                file, num = 0, self.number()
            key = self.db.key(self.scope, file, num)
            if self.text[self.pos:self.pos + 1] == '=':
                self.pos += 1
                node = self.type(linkage_context=linkage_context)
                self.db.types[key] = node
            return Type('ref', (key,))
        self.pos += 1
        if c in '*&kB':
            return Type({'*': 'pointer', '&': 'reference', 'k': 'const', 'B': 'volatile'}[c], (self.type(linkage_context=linkage_context),))
        if c == 'f':
            return Type('function', (self.type(linkage_context=linkage_context),))
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
            return Type('attribute', (attr, self.type(linkage_context=linkage_context)))
        if c == 'r':
            base = self.type(); self.take(';')
            lo = self.until(';'); hi = self.until(';')
            return Type('range', (base, int(lo, 0) if lo.startswith('0x') else int(lo), int(hi)))
        if c == 'a':
            self.take('r'); index = self.type(); self.take(';')
            lo = self.number(); self.take(';'); hi = self.number(); self.take(';')
            return Type('array', (self.type(linkage_context=linkage_context), lo, hi))
        if c in 'su':
            return self.aggregate(c)
        if c == 'e':
            values = []
            while self.text[self.pos:self.pos + 1] != ';':
                name = self.until(':'); val = self.number(); self.take(',')
                values.append((name, val))
            self.take(';')
            return Type('enum', tuple(values))
        if c == 'x':
            tag = self.choice('sue', 'cross-reference tag')
            start = self.pos
            static_boundary = -1
            if linkage_context:
                end = self.text.find(';', start)
                if end >= 0:
                    colon = self.text.rfind(':', start, end)
                    # Before member linkage the final double colon can be the
                    # crossref terminator followed by the linkage separator.
                    # Earlier double colons still qualify the type's name.
                    if colon > start and self.text[colon - 1] == ':' and ',' not in self.text[colon + 1:end]:
                        static_boundary = colon - 1
            while self.pos < len(self.text):
                if self.pos != static_boundary and self.text.startswith('::', self.pos):
                    self.pos += 2
                elif self.text[self.pos] == ':':
                    name = self.text[start:self.pos]
                    self.pos += 1
                    if not name:
                        raise Unsupported('missing cross-reference name')
                    return Type('crossref', (tag, name))
                else:
                    self.pos += 1
            raise Unsupported('unterminated cross-reference name')
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
        self.named_symbols = []
        self.functions = set()
        self.stab_counts = {}

    def key(self, scope, file, num):
        return (self.files.get((scope, file), (scope, file)), num)

    def read(self, path):
        scope, source, file_no, pending = 0, '', 0, ''
        includes = {}
        for text, kind, section, desc, value in macho_symbols(path):
            if not kind & 0xe0:
                self.symbols[text] = value
                self.named_symbols.append((text, kind, section, value))
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
            if kind == 0x24 and re.match(r'.*:[Ff]', text):
                self.functions.add(text.split(':', 1)[0])
            # A successful parse has already read its nested definitions in
            # context. Recover them independently only outside successful spans,
            # so an unsupported outer type cannot hide valid referenced types.
            parsed_until = 0
            for m in re.finditer(r'(\(-?\d+,-?\d+\)|(?<![\w])\d+)=', text):
                if m.start() < parsed_until:
                    continue
                parser = Grammar(self, scope, text[m.start():])
                try:
                    ref = parser.type()
                    parsed_until = m.start() + parser.pos
                    prefix = text[:m.start()]
                    named = None
                    # Nested definitions usually follow field/type syntax,
                    # so avoid scanning their potentially long name prefixes.
                    if prefix.endswith((':t', ':T', ':tt', ':Tt')):
                        named = re.search(r'((?:[^:]|::)+):[tT]t?$', prefix)
                    if named:
                        self.names[ref.args[0]] = named[1]
                        node = self.types.get(ref.args[0])
                        if node and node.kind in ('struct', 'union', 'enum', 'cpp_class'):
                            self.tags.setdefault((scope, named[1]), ref)
                except (Unsupported, ValueError, RecursionError) as error:
                    self.errors.append((scope, str(error)))
            named = re.match(r'((?:[^:]|::)+):[tT]t?(.*)', text)
            if named:
                try:
                    ref = Grammar(self, scope, named[2]).type()
                    if ref.kind == 'ref':
                        self.names[ref.args[0]] = named[1]
                except (Unsupported, ValueError, RecursionError):
                    pass
            if kind in (0x20, 0x26, 0x28):
                m = re.match(r'(.*):[GSV](.*)', text)
                if m:
                    try:
                        ty = Grammar(self, scope, m[2]).type()
                        self.variables.append(Variable(m[1], ty, value, source, kind))
                    except (Unsupported, ValueError) as error:
                        self.errors.append((scope, str(error)))
        for key, name in self.names.items():
            try:
                node = self.resolve(Type('ref', (key,)))
                if node.kind in ('struct', 'union', 'enum', 'cpp_class'):
                    self.tags.setdefault((key[0][0], name), Type('ref', (key,)))
                    if not node.name:
                        node.name = name
            except Unsupported:
                pass
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
