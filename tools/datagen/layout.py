"""Conservative C-layout reconstruction. Unsupported layouts are never guessed."""
import re
from stabs import Type, Unsupported


def align_up(n, align):
    return (n + align - 1) // align * align


def shape(db, ty, active=()):
    if ty.kind == 'ref':
        if ty.args[0] in active:
            raise Unsupported('recursive value type')
        active += (ty.args[0],)
    names = []
    named, visited = ty, set()
    while named.kind in ('ref', 'const', 'volatile', 'attribute'):
        if named.kind == 'ref':
            key = named.args[0]
            if key in visited:
                break
            visited.add(key)
            if key in db.names:
                names.append(db.names[key])
            named = db.types.get(key, Type('unknown'))
        else:
            named = named.args[-1]
    node = db.resolve(ty)
    kind = node.kind
    if kind == 'builtin':
        ctype, size = node.args
        if not size:
            raise Unsupported('void object')
        return dict(kind='scalar', ctype=ctype, size=size, align=min(size, 4))
    if kind == 'range':
        _, lo, hi = node.args
        if hi == 0 and lo in (4, 8, 12, 16):
            if lo not in (4, 8):
                raise Unsupported('extended precision float')
            return dict(kind='scalar', ctype='float' if lo == 4 else 'double', size=lo, align=4)
        names = []
        t, visited = ty, set()
        while t.kind in ('ref', 'const', 'volatile', 'attribute'):
            if t.kind == 'ref':
                k = t.args[0]
                if k in visited:
                    break
                visited.add(k); names.append(db.names.get(k, ''))
                t = db.types.get(k, node)
            else:
                t = t.args[-1]
        aliases = {'long int': ('long', 4), 'long unsigned int': ('unsigned long', 4),
                   'int': ('int', 4), 'unsigned int': ('unsigned int', 4),
                   'char': ('char', 1), 'signed char': ('signed char', 1),
                   'unsigned char': ('unsigned char', 1), 'short int': ('short', 2),
                   'short unsigned int': ('unsigned short', 2),
                   'long long int': ('long long', 8), 'long long unsigned int': ('unsigned long long', 8),
                   'bool': ('_Bool', 1)}
        known = next((aliases[n] for n in reversed(names) if n in aliases), None)
        if known:
            ctype, size = known
        else:
            signed = lo < 0
            bits = max(abs(lo).bit_length() if signed else 0, hi.bit_length() + int(signed))
            size = next((s for s in (1, 2, 4, 8) if bits <= s * 8), None)
            if size is None:
                raise Unsupported('integer range exceeds 64 bits')
            ctype = ('' if signed else 'unsigned ') + {1:'char', 2:'short', 4:'int', 8:'long long'}[size]
        return dict(kind='scalar', ctype=ctype, size=size, align=min(size, 4))
    if kind == 'enum':
        return dict(kind='scalar', ctype='int', size=4, align=4, enum=True)
    if kind in ('pointer', 'reference'):
        # A pointer to an unavailable C++ class remains an opaque pointer, not
        # a guessed class layout. Pointee metadata is retained in coverage.json.
        ctype = 'void *'
        try:
            target = db.resolve(node.args[0])
            if target.kind in ('function', 'method'):
                ctype = 'dg_function'
            elif target.kind in ('range', 'builtin'):
                ctype = shape(db, node.args[0], active)['ctype'] + ' *'
        except Unsupported:
            pass
        return dict(kind='pointer', ctype=ctype, size=4, align=4)
    if kind == 'array':
        elem, lo, hi = node.args
        if lo != 0 or hi < lo:
            raise Unsupported('nonzero/unknown array bounds')
        child = shape(db, elem, active)
        return dict(kind='array', child=child, count=hi + 1, size=child['size'] * (hi + 1), align=child['align'])
    if kind in ('struct', 'union'):
        size, members = node.args
        fields, offset, align = [], 0, 1
        for name, ty, bit, bits in members:
            child = shape(db, ty, active)
            if bit % 8 or bits != child['size'] * 8:
                raise Unsupported('bitfield or member size mismatch')
            if not re.fullmatch(r'[A-Za-z_]\w*', name):
                raise Unsupported('non-C member name')
            expected = align_up(offset, child['align']) if kind == 'struct' else 0
            if expected != bit // 8:
                raise Unsupported('non-native i386 member offset')
            fields.append((name, bit // 8, child))
            offset = expected + child['size'] if kind == 'struct' else max(offset, child['size'])
            align = max(align, child['align'])
        if not fields or align_up(offset, align) != size:
            raise Unsupported('non-native i386 aggregate size')
        result = dict(kind=kind, fields=fields, size=size, align=align)
        if names and re.fullmatch(r'[A-Za-z_]\w*', names[-1]):
            result['tag'] = kind + ' ' + names[-1]
        return result
    raise Unsupported('unsupported value type ' + kind)


def pointer_offsets(ty, offset=0):
    kind = ty['kind']
    if kind == 'pointer':
        return {offset}
    if kind == 'array':
        return set().union(*(pointer_offsets(ty['child'], offset + i * ty['child']['size']) for i in range(ty['count'])))
    if kind == 'struct':
        return set().union(*(pointer_offsets(child, offset + off) for _, off, child in ty['fields']))
    if kind == 'union':
        # A union needs an active-member choice from initializer bytes/relocs.
        return set().union(*(pointer_offsets(child, offset) for _, _, child in ty['fields']))
    return set()
