"""Index the simple C declarations in engine headers for verified type reuse.

This deliberately accepts a small C subset. Macro-dependent declarations,
bitfields, and anonymous aggregates stay on the STABS/fallback path.
"""
from pathlib import Path
import re
from layout import align_up
from stabs import Unsupported

SCALARS = {
    'char': ('char', 1), 'signed char': ('signed char', 1),
    'unsigned char': ('unsigned char', 1), 'short': ('short', 2),
    'short int': ('short', 2), 'short unsigned int': ('unsigned short', 2),
    'unsigned short': ('unsigned short', 2), 'unsigned short int': ('unsigned short', 2),
    'int': ('int', 4), 'unsigned int': ('unsigned int', 4), 'unsigned': ('unsigned int', 4),
    'long': ('long', 4), 'long int': ('long', 4), 'long unsigned int': ('unsigned long', 4),
    'unsigned long': ('unsigned long', 4), 'unsigned long int': ('unsigned long', 4),
    'long long': ('long long', 8), 'long long int': ('long long', 8),
    'unsigned long long': ('unsigned long long', 8), 'long long unsigned int': ('unsigned long long', 8),
    'float': ('float', 4), 'double': ('double', 8), '_Bool': ('_Bool', 1), 'bool': ('_Bool', 1),
}


def equivalent(a, b):
    if (a['kind'], a['size'], a['align']) != (b['kind'], b['size'], b['align']):
        return False
    if a['kind'] == 'pointer':
        return True
    if a['kind'] == 'scalar':
        return a['ctype'] == b['ctype'] or (a.get('enum') and b.get('enum'))
    if a['kind'] == 'array':
        return a['count'] == b['count'] and equivalent(a['child'], b['child'])
    return len(a['fields']) == len(b['fields']) and all(
        x[:2] == y[:2] and equivalent(x[2], y[2]) for x, y in zip(a['fields'], b['fields']))


class Headers:
    def __init__(self, root):
        self.root = root
        self.aliases, self.tags, self.used = {}, {}, set()
        self.resolving = set()
        # The legacy common_types.h contains a disabled duplicate of the types.
        # Subsystem headers win over the older central cod2_defs.h definitions.
        paths = sorted(root.rglob('*.h'), key=lambda p: (p.name == 'cod2_defs.h', str(p)))
        for path in paths:
            if path.name in ('common_types.h', 'generated_syms.h'):
                continue
            text = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
            for m in re.finditer(r'\b(struct|union|enum)\s+(\w+)\s*\{', text):
                pos, depth = m.end(), 1
                while pos < len(text) and depth:
                    depth += (text[pos] == '{') - (text[pos] == '}')
                    pos += 1
                if depth or text[pos:pos+1] != ';':
                    continue
                snippet = text[m.start():pos+1]
                if '#' in snippet or '{' in snippet[snippet.index('{')+1:snippet.rindex('}')]:
                    continue
                key = m[1] + ' ' + m[2]
                self.tags.setdefault(key, (snippet, path))
            for m in re.finditer(r'^\s*typedef\s+([^;{}#]+);', text, re.M):
                declaration = m[1].strip()
                function = re.search(r'\(\s*\*\s*(\w+)\s*\)', declaration)
                plain = re.search(r'\b(\w+)\s*(?:\[[^]]+\])*\s*$', declaration)
                match = function or plain
                if match:
                    self.aliases.setdefault(match[1], ('typedef ' + declaration + ';', declaration, path))

    def base(self, name):
        name = re.sub(r'\b(const|volatile)\b', '', name)
        name = ' '.join(name.split())
        if name in SCALARS:
            ctype, size = SCALARS[name]
            return dict(kind='scalar', ctype=ctype, size=size, align=min(size, 4)), []
        if name in self.aliases:
            if name in self.resolving:
                raise Unsupported('cyclic header typedef')
            self.resolving.add(name)
            try:
                _, decl, _ = self.aliases[name]
                ty, _, deps = self.declaration(decl)
                return ty, [('alias', name, False)] + [dep for dep in deps if dep[0] == 'tag' and dep[2]]
            finally:
                self.resolving.remove(name)
        if name in self.tags:
            return self.aggregate(name), [('tag', name, True)]
        raise Unsupported('unsupported header type ' + name)

    def declaration(self, text):
        if any(c in text for c in ':{}#') or '__attribute' in text:
            raise Unsupported('unsupported C declaration')
        function = re.fullmatch(r'(.*?)\(\s*\*\s*(\w+)\s*\)\s*\((.*)\)', text, re.S)
        if function:
            deps = [('alias', word, False) for word in re.findall(r'\b\w+\b', function[1] + ' ' + function[3]) if word in self.aliases]
            return dict(kind='pointer', ctype='dg_function', size=4, align=4), function[2], deps
        m = re.fullmatch(r'(.*?)\b(\w+)\s*((?:\[\s*(?:0x[0-9a-fA-F]+|[0-9]+)\s*\])*)', text.strip(), re.S)
        if not m:
            raise Unsupported('unsupported header declarator')
        base, name, dimensions = m.groups()
        if '*' in base:
            target = ' '.join(re.sub(r'\b(const|volatile)\b|\*', '', base).split())
            deps = [('alias', target, False)] if target in self.aliases else [('tag', target, False)] if target in self.tags else []
            ty = dict(kind='pointer', ctype='void *', size=4, align=4)
        else:
            ty, deps = self.base(base)
        for bound in reversed(re.findall(r'\[\s*([^]]+)\s*\]', dimensions)):
            count = int(bound.strip(), 0)
            ty = dict(kind='array', child=ty, count=count, size=count*ty['size'], align=ty['align'])
        return ty, name, deps

    def aggregate(self, name):
        if name in self.resolving:
            raise Unsupported('recursive header value type')
        self.resolving.add(name)
        try:
            snippet, path = self.tags[name]
            kind = name.split()[0]
            if kind == 'enum':
                return dict(kind='scalar', ctype='int', enum=True, size=4, align=4)
            body = snippet[snippet.index('{')+1:snippet.rindex('}')]
            fields, offset, alignment = [], 0, 1
            for decl in body.split(';'):
                if not decl.strip():
                    continue
                ty, field, _ = self.declaration(decl.strip())
                off = align_up(offset, ty['align']) if kind == 'struct' else 0
                fields.append((field, off, ty))
                offset = off + ty['size'] if kind == 'struct' else max(offset, ty['size'])
                alignment = max(alignment, ty['align'])
            return dict(kind=kind, fields=fields, size=align_up(offset, alignment), align=alignment)
        finally:
            self.resolving.remove(name)

    def match(self, ty):
        name = ty.get('tag')
        if not name or name not in self.tags:
            return None
        try:
            if equivalent(ty, self.aggregate(name)):
                # Rendering checks all syntactic dependencies, including function
                # argument typedefs whose sizes do not contribute to the struct.
                self.render({name})
                self.used.add(name)
                return name
        except (Unsupported, RecursionError, ValueError):
            pass
        return None

    def render(self, names=None):
        ordered, visiting, done, forwards = [], set(), set(), set()

        def visit(kind, name, complete=True):
            key = (kind, name)
            if kind == 'tag':
                forwards.add(name + ';')
                if not complete:
                    return
            if key in done:
                return
            if key in visiting:
                raise Unsupported('header declaration dependency cycle')
            visiting.add(key)
            if kind == 'alias':
                snippet, decl, path = self.aliases[name]
                simple_tag = re.fullmatch(r'(struct|union|enum)\s+(\w+)\s+\w+', decl)
                if simple_tag:
                    deps = [('tag', simple_tag[1] + ' ' + simple_tag[2], simple_tag[1] == 'enum')]
                else:
                    _, _, deps = self.declaration(decl)
            else:
                snippet, path = self.tags[name]
                if name.startswith('enum '):
                    deps = []
                else:
                    body = snippet[snippet.index('{')+1:snippet.rindex('}')]
                    deps = []
                    for decl in body.split(';'):
                        if decl.strip():
                            _, _, dep = self.declaration(decl.strip()); deps.extend(dep)
            for dep in deps:
                visit(*dep)
            visiting.remove(key); done.add(key)
            ordered.append('/* ' + str(path.relative_to(self.root.parent.parent)) + ' */\n' + snippet)
        for name in sorted(self.used if names is None else names):
            visit('tag', name)
        return '\n'.join(sorted(forwards)) + '\n' + '\n'.join(ordered) + '\n'
