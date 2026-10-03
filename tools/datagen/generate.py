#!/usr/bin/env python3
"""Generate locally from the repository blobs and a user-supplied STABS binary.

Never reads initializer bytes from the proprietary binary. It supplies types only.
The original assembler is the authority for bytes and symbolic relocations.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess

from bss import generate_bss
from c_headers import Headers
from elf32 import ELF
from layout import shape
from stabs import Database, Unsupported

ROOT = Path(__file__).resolve().parents[2]


def run(*args):
    subprocess.run(args, check=True)


def byte_init(data):
    return '{' + ','.join(str(b) for b in data) + '}' if any(data) else '{0}'


def source_vtable(obj):
    # literals.S documents the two header slots and function slots. The engine
    # uses the same two-pointer address point in FxPrimitives.c. Zero-only tail
    # slots preserve the assembly extent without keeping an ILP32 byte header.
    if not obj.name.startswith('__ZTV') or len(obj.data) < 8 or len(obj.data) % 4:
        return None
    if any(pos < 4 or pos % 4 for pos in obj.relocs):
        return None
    for pos in range(4, len(obj.data), 4):
        if pos not in obj.relocs and any(obj.data[pos:pos + 4]):
            return None
    pointer = dict(kind='pointer', ctype='dg_function', size=4, align=4)
    fields = [('offset_to_top', 0, dict(kind='scalar', ctype='__PTRDIFF_TYPE__', size=4, align=4)),
              ('type_info', 4, dict(kind='pointer', ctype='void *', size=4, align=4))]
    count = (len(obj.data) - 8) // 4
    if count:
        fields.append(('slots', 8, dict(kind='array', child=pointer, count=count, size=count * 4, align=4)))
    return dict(kind='struct', fields=fields, size=len(obj.data), align=4)


class Emitter:
    def __init__(self, objects, headers, db, byname):
        self.objects = objects
        self.headers = headers
        self.db, self.byname = db, byname
        self.types = []
        self.cache = {}
        names = sorted({o.name for o in objects} | {name for o in objects for name, _ in o.relocs.values() if not name.startswith('.')})
        self.refs = {name: 'dg_ref_' + str(i) for i, name in enumerate(names)}
        self.functions = {name for name in names if name in db.functions and re.fullmatch(r'[A-Za-z_]\w*', name)
                          and name not in {o.name for o in objects}}
        self.refs.update({name: name for name in self.functions})
        self.addends = {}
        self.address_types = {}
        self.translate_addresses = False
        self.trusted_objects = set()

    def address_type(self, name):
        if name in self.address_types:
            return self.address_types[name]
        candidates = self.byname.get(name, [])
        if not candidates and name.startswith('_'):
            candidates = self.byname.get(name[1:], [])
        choices = {}
        for var in candidates:
            try:
                ty = shape(self.db, var.type)
                choices[json.dumps(ty, sort_keys=True)] = ty
            except (Unsupported, RecursionError):
                pass
        ty = next(iter(choices.values())) if len(choices) == 1 else None
        self.address_types[name] = ty
        return ty

    def address_path(self, ty, offset, lvalue):
        if offset == 0:
            return lvalue, 0
        if ty['kind'] == 'array':
            index, rest = divmod(offset, ty['child']['size'])
            if index <= ty['count'] and (index < ty['count'] or rest == 0):
                return self.address_path(ty['child'], rest, '(' + lvalue + ')[%d]' % index)
        elif ty['kind'] == 'struct':
            for field, off, child in ty['fields']:
                if off <= offset < off + child['size']:
                    return self.address_path(child, offset - off, '(' + lvalue + ').' + field)
        elif ty['kind'] in ('scalar', 'pointer') and offset < ty['size']:
            return lvalue, offset
        raise Unsupported('address targets padding, an ambiguous union, or exceeds the debug type')

    def address(self, obj, symbol, addend, offset):
        if symbol.startswith('.'):
            targets = [o for o in self.objects if o.origin == obj.origin and o.section == symbol
                       and o.offset <= addend < o.offset + len(o.data)]
            if len(targets) != 1:
                raise Unsupported('unresolved section-relative address')
            target = targets[0]
            symbol, addend = target.name, addend - target.offset
        expr = self.refs[symbol]
        if not addend:
            return expr
        if not self.translate_addresses:
            return '(unsigned char *)' + expr + ' + (%d)' % addend
        key = (Path(obj.origin).stem, obj.name, offset)
        try:
            if id(obj) not in self.trusted_objects:
                raise Unsupported('owner has no lossless debug type; relocation may be spurious')
            target = self.address_type(symbol)
            if target is None:
                raise Unsupported('no unique target debug type')
            ctype = self.ctype(target)
            path, residual = self.address_path(target, addend, '(*(%s *)%s)' % (ctype, expr))
            expr = '(unsigned char *)&(' + path + ')'
            if residual:
                expr += ' + (%d)' % residual
            self.addends[key] = dict(object=obj.name, offset=offset, target=symbol, addend=addend,
                                     status='translated', expression=expr)
            return expr
        except Unsupported as error:
            self.addends[key] = dict(object=obj.name, offset=offset, target=symbol, addend=addend,
                                     status='unresolved', reason=str(error))
            return '(unsigned char *)' + expr + ' + (%d)' % addend

    def ctype(self, ty):
        if ty['kind'] in ('scalar', 'pointer'):
            return ty['ctype']
        reuse = self.headers.match(ty)
        if reuse:
            return reuse
        key = json.dumps(ty, sort_keys=True)
        if key in self.cache:
            return self.cache[key]
        name = 'dg_type_' + str(len(self.cache))
        self.cache[key] = name
        if ty['kind'] == 'array':
            self.types.append('typedef %s %s[%d];' % (self.ctype(ty['child']), name, ty['count']))
        else:
            fields = '\n'.join('    %s %s;' % (self.ctype(child), field) for field, _, child in ty['fields'])
            packed = ' __attribute__((packed))' if ty.get('fallback') else ''
            self.types.append('typedef %s%s {\n%s\n} %s;' % (ty['kind'], packed, fields, name))
        return name

    def initializer(self, obj, ty, offset=0, cast_type=None):
        kind, size = ty['kind'], ty['size']
        data = obj.data[offset:offset + size]
        if len(data) != size:
            raise Unsupported('type extends beyond blob')
        relocs = {p: r for p, r in obj.relocs.items() if offset <= p < offset + size}
        if kind == 'pointer':
            if offset in relocs and len(relocs) == 1:
                symbol, addend = relocs[offset]
                expr = self.address(obj, symbol, addend, offset)
                return '(%s)(%s)' % (cast_type or ty['ctype'], expr)
            if any(data):
                raise Unsupported('nonzero pointer without symbolic relocation')
            return '0'
        if kind == 'scalar':
            if relocs:
                raise Unsupported('symbolic relocation in non-pointer field')
            ctype = ty['ctype']
            if ctype in ('float', 'double'):
                value = struct.unpack('<f' if size == 4 else '<d', data)[0]
                if not math.isfinite(value):
                    raise Unsupported('non-finite float requires bit-exact fallback')
                return value.hex() + ('f' if size == 4 else '')
            value = int.from_bytes(data, 'little', signed=not ctype.startswith('unsigned') and ctype != '_Bool')
            if ctype == '_Bool' and value not in (0, 1):
                raise Unsupported('noncanonical bool')
            return str(value) + ('ULL' if ctype == 'unsigned long long' else 'LL' if ctype == 'long long' else '')
        if kind == 'array':
            if not relocs and not any(data):
                return '{0}'
            return '{' + ','.join(self.initializer(obj, ty['child'], offset + i * ty['child']['size']) for i in range(ty['count'])) + '}'
        if kind == 'union':
            for field, _, child in ty['fields']:
                try:
                    value = self.initializer(obj, child, offset)
                    if any(data[child['size']:]) or any(p >= offset + child['size'] for p in relocs):
                        continue
                    return '{.' + field + '=' + value + '}'
                except Unsupported:
                    pass
            raise Unsupported('no lossless union active member')
        values, occupied = [], set()
        for field, off, child in ty['fields']:
            field_type = '__typeof__(((%s *)0)->%s)' % (self.ctype(ty), field)
            values.append('.' + field + '=' + self.initializer(obj, child, offset + off, field_type))
            occupied.update(range(off, off + child['size']))
        if any(b and i not in occupied for i, b in enumerate(data)):
            raise Unsupported('nonzero aggregate padding')
        if any(p - offset not in occupied for p in relocs):
            raise Unsupported('relocation in aggregate padding')
        return '{' + ','.join(values) + '}'

    def fallback(self, obj):
        fields, offset = [], 0
        for pos in sorted(obj.relocs):
            if pos < offset or pos + 4 > len(obj.data):
                raise ValueError('overlapping/out-of-bounds relocation in ' + obj.name)
            if pos > offset:
                fields.append(('bytes_' + str(offset), offset, dict(kind='array', child=dict(kind='scalar', ctype='unsigned char', size=1, align=1), count=pos-offset, size=pos-offset, align=1)))
            fields.append(('pointer_' + str(pos), pos, dict(kind='pointer', ctype='void *', size=4, align=1)))
            offset = pos + 4
        if offset < len(obj.data):
            fields.append(('bytes_' + str(offset), offset, dict(kind='array', child=dict(kind='scalar', ctype='unsigned char', size=1, align=1), count=len(obj.data)-offset, size=len(obj.data)-offset, align=1)))
        return dict(kind='struct', fields=fields, size=len(obj.data), align=1, fallback=True)


def choose_type(db, obj, byname, emitter):
    name = re.sub(r'_[0-9a-f]{8}$', '', obj.name)
    candidates = byname.get(obj.name, []) or byname.get(name, [])
    if not candidates and name.startswith('_'):
        candidates = byname.get(name[1:], [])
    usable, reasons = {}, []
    for var in candidates:
        try:
            ty = shape(db, var.type)
            if ty['size'] > len(obj.data):
                raise Unsupported('type larger than blob')
            if any(pos >= ty['size'] for pos in obj.relocs):
                raise Unsupported('relocation beyond debug type')
            emitter.initializer(obj, ty)
            usable[json.dumps(ty, sort_keys=True)] = (ty, var)
        except (Unsupported, RecursionError) as error:
            reasons.append(str(error))
    if len(usable) == 1:
        ty, var = next(iter(usable.values()))
        return ty, dict(status='typed', source=Path(var.source).name, debug_name=var.name,
                       type_ref=repr(var.type), value_bytes=ty['size'], trailing_bytes=len(obj.data)-ty['size'])
    if len(usable) > 1:
        reasons = ['ambiguous static variable types']
    return emitter.fallback(obj), dict(status='fallback', reason='; '.join(sorted(set(reasons))) or 'no matching debug variable')


def generate(args):
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    db = Database().read(args.binary)
    byname = defaultdict(list)
    for var in db.variables:
        byname[var.name].append(var)
    groups = {}
    for name in ('data', 'literals', 'import_pointers'):
        original = out / ('original-' + name + '.o')
        run(args.clang, '-target', 'i386-unknown-linux-gnu', '-c', str(ROOT / 'src/blobs' / (name + '.S')), '-o', str(original))
        groups[name] = ELF(original).objects()
    # Preserve the current Stage 2 omissions, initializers, aliases, and stub
    # extents. These committed C artifacts are production's authority.
    native_sources = {'data': 'data32', 'literals': 'literals32',
                      'import_pointers': 'import_pointers_native'}
    for group, source in native_sources.items():
        text = (ROOT / 'build/native_gen' / (source + '.c')).read_text()
        # These files define only packed blob structs, bytes, and pointers. The
        # engine header provides uintptr_t and one omitted extern declaration;
        # removing it lets the i386 probe run without a Linux libc sysroot.
        text = text.replace('#include "common_types.h"',
                            'typedef __UINTPTR_TYPE__ uintptr_t;\nextern unsigned char scrMemTreeGlob[];')
        reference = out / ('reference-native-' + group + '.c')
        reference.write_text(text)
        objfile = reference.with_suffix('.o')
        run(args.clang, '-target', 'i386-unknown-linux-gnu', '-ffreestanding', '-std=c11',
            '-c', str(reference), '-o', str(objfile))
        groups[group + '_native'] = ELF(objfile).objects()
    objects = [o for group in groups.values() for o in group]
    headers = Headers(ROOT / 'src/headers')
    emitter = Emitter(objects, headers, db, byname)
    records, selected = [], {}
    group_by_id = {id(o): group for group, objs in groups.items() for o in objs}
    for obj in objects:
        ty, record = choose_type(db, obj, byname, emitter)
        vtable = source_vtable(obj)
        if vtable:
            ty = vtable
            record = dict(status='typed', source='documented vtable header and pointer slots')
        # Import wrapper storage has an explicit pointer contract in the source.
        if group_by_id[id(obj)].startswith('import_pointers') and len(obj.data) == 4 and set(obj.relocs) == {0}:
            ty = dict(kind='pointer', ctype='void *', size=4, align=4)
            record = dict(status='typed', source='import pointer wrapper')
        if obj.name.startswith('imp_') and len(obj.data) == 4 and set(obj.relocs) == {0}:
            ty = dict(kind='pointer', ctype='void *', size=4, align=4)
            record = dict(status='typed', source='import pointer wrapper')
        if obj.name.startswith('str_') and obj.section == '.rodata' and not obj.relocs:
            child = dict(kind='scalar', ctype='char', size=1, align=1)
            ty = dict(kind='array', child=child, count=len(obj.data), size=len(obj.data), align=1)
            record = dict(status='typed', source='string literal storage')
        if obj.name in ('sse_float_abs_mask', 'sse_float_sign_mask') and len(obj.data) == 16 and not obj.relocs:
            child = dict(kind='scalar', ctype='unsigned int', size=4, align=4)
            ty = dict(kind='array', child=child, count=4, size=16, align=4)
            record = dict(status='typed', source='documented four-word SSE bit mask')
        # Literal labels encode their floating-point width. Hexadecimal C
        # literals preserve finite IEEE values, including negative zero.
        if re.fullmatch(r'lit[48]_[0-9a-f]+', obj.name) and not obj.relocs:
            width = int(obj.name[3])
            if len(obj.data) >= width:
                ty = dict(kind='scalar', ctype='float' if width == 4 else 'double', size=width, align=4)
                try:
                    emitter.initializer(obj, ty)
                    record = dict(status='typed', source='floating-point literal', trailing_bytes=len(obj.data)-width)
                except Unsupported:
                    ty = emitter.fallback(obj)
                    record = dict(status='fallback', reason='non-finite floating-point literal needs exact payload bytes')
            elif group_by_id[id(obj)] == 'literals_native':
                record = dict(status='fallback', reason='upstream one-byte literal placeholder')
        ctype = emitter.ctype(ty)
        selected[id(obj)] = ty
        record.update(name=obj.name, group=group_by_id[id(obj)], aliases=obj.aliases,
                      section=obj.section, bytes=len(obj.data), relocations=len(obj.relocs), ctype=ctype)
        records.append(record)
    record_by_id = dict(zip((id(o) for o in objects), records))
    banner = '/* Generated by tools/datagen/generate.py. Do not edit or commit. */\n'
    common = banner + '''#ifndef COD2_DATAGEN_TYPES_H
#define COD2_DATAGEN_TYPES_H
#if defined(__APPLE__)
#define DG_ASM(name) "_" name
#define DG_DATA "__DATA,__data"
#define DG_RODATA "__TEXT,__const"
#define DG_BSS "__DATA,__data"
#define DG_RELRO "__DATA_CONST,__const"
#else
#define DG_ASM(name) name
#define DG_DATA ".data"
#define DG_RODATA ".rodata"
#define DG_BSS ".bss"
#define DG_RELRO ".data.rel.ro"
#endif
typedef void (*dg_function)(void);
'''
    native = {group: {o.name for o in groups[group + '_native']} for group in native_sources}
    omitted = [o.name for group in native_sources for o in groups[group] if o.name not in native[group]]
    emitter.translate_addresses = True
    emitter.trusted_objects = {id(o) for o in objects if record_by_id[id(o)]['status'] == 'typed'}
    for group, objs in groups.items():
        full = []
        for index, obj in enumerate(objs):
            ty, record = selected[id(obj)], record_by_id[id(obj)]
            ctype = emitter.ctype(ty)
            section = {'.rodata': 'DG_RODATA', '.data': 'DG_DATA', '.bss': 'DG_BSS',
                       '.data.rel.ro': 'DG_RELRO'}[obj.section]
            attr = '__attribute__((used, aligned(DG_ALIGN), section(%s)))' % section
            description = ('DATAGEN_FALLBACK: ' + record['reason']) if record['status'] == 'fallback' else record.get('source', 'debug type')
            text = '\n/* %s: %s */\n' % (obj.name, description)
            text += '%s%s dg_object_%d __asm__(DG_ASM("%s")) %s = %s;\n' % ('' if obj.global_symbol else 'static ', ctype, index, obj.name, attr, emitter.initializer(obj, ty))
            text += '#if __SIZEOF_POINTER__ == 4\n_Static_assert(sizeof(%s) == %d, "i386 size: %s");\n' % (ctype, ty['size'], obj.name)
            tail = obj.data[ty['size']:]
            if tail:
                text += 'static unsigned char dg_tail_%d[%d] %s = %s;\n' % (index, len(tail), attr, byte_init(tail))
            text += '#endif\n'
            for alias in obj.aliases:
                text += '__asm__(\".globl \" DG_ASM(\"%s\") \"\\n.set \" DG_ASM(\"%s\") \",\" DG_ASM(\"%s\"));\n' % (alias, alias, obj.name)
            full.append(text)
        prefix = banner + '#include "typed_types.h"\n#if __SIZEOF_POINTER__ == 4\n#define DG_ALIGN 1\n#else\n#define DG_ALIGN 16\n#endif\n'
        (out / (group + '.c')).write_text(prefix + ''.join(full))
    bss = generate_bss(ROOT, out, args.clang, db, byname, emitter)
    common += '#if defined(DG_ENGINE_HEADERS)\n#include \"common_types.h\"\n#else\n' + headers.render() + '#endif\n'
    common += '\n'.join(emitter.types) + '\n'
    common += '\n'.join(('extern void %s(void)' % ref if name in emitter.functions else 'extern unsigned char %s[]' % ref)
                        + ' __asm__(DG_ASM("%s"));' % name for name, ref in emitter.refs.items()) + '\n#endif\n'
    (out / 'typed_types.h').write_text(common)
    summary = {}
    for group, objs in groups.items():
        entries = [record_by_id[id(o)] for o in objs]
        summary[group] = dict(objects=len(entries), typed=sum(r['status']=='typed' for r in entries),
                              relocations=sum(r['relocations'] for r in entries),
                              typed_relocations=sum(r['relocations'] for r in entries if r['status']=='typed'))
    summary['bss'] = dict(objects=len(bss), upstream_typed=sum(r['status']=='upstream_typed' for r in bss),
                          typed=sum(r['status']=='typed' for r in bss),
                          fallback=sum(r['status']=='fallback' for r in bss))
    report = dict(format=1, bss=bss, binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  stabs=dict(variables=len(db.variables), types=len(db.types), parse_failures=len(db.errors),
                             failure_reasons=dict(sorted(Counter(reason for _, reason in db.errors).items()))),
                  summary=summary, objects=records, omitted_native=sorted(omitted),
                  native_names={key: sorted(val) for key,val in native.items()},
                  reused_engine_types=sorted(headers.used),
                  relocation_addends=[dict(origin=key[0], **value) for key, value in sorted(emitter.addends.items())])
    (out / 'coverage.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    (out / 'fallbacks.txt').write_text(''.join(r['group'] + ':' + r['name'] + ': ' + r['reason'] + '\n'
                                            for r in records if r['status']=='fallback') +
                                    ''.join('bss:' + r['name'] + ': ' + r['reason'] + '\n'
                                            for r in bss if r['status']=='fallback'))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/x64_gen')
    parser.add_argument('--clang', default='clang')
    generate(parser.parse_args())
