import tempfile
import unittest
from pathlib import Path
import os
import struct
import subprocess
from types import SimpleNamespace
from unittest.mock import patch
from stabs import Database, Grammar, Unsupported


class GrammarTests(unittest.TestCase):
    def setUp(self):
        self.db = Database()

    def parse(self, text, scope=1):
        return Grammar(self.db, scope, text).type()

    def test_recursive_struct_and_function_pointer(self):
        self.parse('(0,1)=s8next:(0,2)=*(0,1),0,32;call:(0,3)=*(0,4)=f(0,5)=r(0,5);-2147483648;2147483647;,32,32;;')
        node = self.db.resolve(self.parse('(0,1)'))
        self.assertEqual(node.kind, 'struct')
        self.assertEqual(node.args[0], 8)
        self.assertEqual(self.db.resolve(node.args[1][0][1]).kind, 'pointer')

    def test_array_union_enum_and_typedef(self):
        self.parse('(2,1)=eA:0,B:7,;')
        array = self.parse('(2,2)=ar(2,1);0;3;(2,1)')
        self.assertEqual(self.db.resolve(array).args[1:], (0, 3))
        self.parse('(2,3)=u4a:(2,1),0,32;b:(2,1),0,32;;')
        self.assertEqual(self.db.resolve(self.parse('(0,7)=(2,3)')).kind, 'union')

    def test_compilation_units_do_not_alias(self):
        a = self.parse('1=r1;-2147483648;2147483647;', 1)
        b = self.parse('1=r1;0;255;', 2)
        self.assertNotEqual(self.db.resolve(a), self.db.resolve(b))

    def test_cross_file_reference(self):
        self.db.files[2, 1] = (1, 1)
        self.parse('(1,2)=eTEST:1,;', 1)
        self.assertEqual(self.db.resolve(self.parse('(1,2)', 2)).kind, 'enum')

    def test_unknown_type_fails_explicitly(self):
        with self.assertRaises(Unsupported):
            self.parse('Z')

    def test_cpp_methods_do_not_hide_pod_storage(self):
        self.parse('(0,1)=r(0,1);-2147483648;2147483647;')
        node = self.db.resolve(self.parse('(0,2)=s4value:(0,1),0,32;operator=::(0,3)=#(0,2),(0,4)=*(0,2),(0,1);:_ZN4TestaSERKS_;2A.;;'))
        self.assertEqual(node.kind, 'struct')
        self.assertEqual([member[0] for member in node.args[1]], ['value'])

    def test_namespaced_cross_reference_consumes_full_name(self):
        parser = Grammar(self.db, 1, '(0,2)=xsalpha::beta::Widget:')
        node = parser.type()
        self.assertEqual(self.db.types[node.args[0]].args, ('s', 'alpha::beta::Widget'))
        self.assertEqual(parser.pos, len(parser.text))

    def test_namespaced_tags_resolve_in_their_compilation_unit(self):
        records = [
            ('first.cpp', 0x64, 0, 0, 0),
            ('alpha::Widget:Tt(0,1)=s4value:-1,0,32;;', 0x80, 0, 0, 0),
            ('Alias:t(0,2)=xsalpha::Widget:', 0x80, 0, 0, 0),
            ('second.cpp', 0x64, 0, 0, 0),
            ('alpha::Widget:Tt(0,1)=s8first:-1,0,32;second:-1,32,32;;', 0x80, 0, 0, 0),
            ('Alias:t(0,2)=xsalpha::Widget:', 0x80, 0, 0, 0),
            ('third.cpp', 0x64, 0, 0, 0),
            ('Alias:t(0,2)=xsalpha::Widget:', 0x80, 0, 0, 0),
        ]
        with patch('stabs.macho_symbols', return_value=iter(records)):
            self.db.read('synthetic')
        self.assertEqual(self.db.errors, [])
        self.assertEqual(self.db.resolve(self.parse('(0,2)', 1)).args[0], 4)
        self.assertEqual(self.db.resolve(self.parse('(0,2)', 2)).args[0], 8)
        self.assertEqual(self.db.resolve(self.parse('(0,2)', 1)).name, 'alpha::Widget')
        with self.assertRaisesRegex(Unsupported, 'unresolved tag alpha::Widget'):
            self.db.resolve(self.parse('(0,2)', 3))

    def test_nested_definitions_preserve_qualified_name_registration(self):
        for descriptor in ('t', 'T', 'tt', 'Tt'):
            with self.subTest(descriptor=descriptor):
                db = Database()
                records = [('one.cpp', 0x64, 0, 0, 0),
                           ('alpha::Outer:' + descriptor + '(0,1)=s4field:(0,2)=s4value:-1,0,32;;,0,32;;',
                            0x80, 0, 0, 0)]
                with patch('stabs.macho_symbols', return_value=iter(records)):
                    db.read('synthetic')
                outer = db.key(1, 0, 1)
                inner = db.key(1, 0, 2)
                self.assertEqual(db.errors, [])
                self.assertEqual(db.names, {outer: 'alpha::Outer'})
                self.assertEqual(db.types[outer].name, 'alpha::Outer')
                self.assertEqual(db.types[inner].name, '')
                self.assertEqual(db.tags[1, 'alpha::Outer'].args, (outer,))

    def test_cpp_access_preserves_storage_and_visibility(self):
        from layout import shape
        node = self.db.resolve(self.parse('(0,2)=s12priv:/0-1,0,32;prot:/1-1,32,32;pub:/2-1,64,32;;'))
        self.assertEqual(node.kind, 'struct')
        self.assertEqual(node.cpp.member_access, (('priv', 'private'), ('prot', 'protected'), ('pub', 'public')))
        self.assertEqual(shape(self.db, node)['size'], 12)

    def test_cpp_overloads_preserve_following_fields(self):
        from layout import shape
        text = ('(0,2)=s8value:-1,0,32;act::(0,3)=##-1;:first;2B.'
                '(0,4)=##-1;:second;1A.;tail:-1,32,32;;')
        parser = Grammar(self.db, 1, text)
        node = self.db.resolve(parser.type())
        self.assertEqual(parser.pos, len(text))
        self.assertEqual([(method.name, method.linkage, method.access, method.qualifier)
                          for method in node.cpp.methods],
                         [('act', 'first', 'public', 'const'), ('act', 'second', 'protected', '')])
        self.assertEqual([self.db.resolve(method.type).kind for method in node.cpp.methods], ['method', 'method'])
        self.assertEqual([field[0] for field in node.args[1]], ['value', 'tail'])
        self.assertEqual(shape(self.db, node)['size'], 8)

    def test_cpp_static_member_has_no_instance_storage(self):
        from layout import shape
        node = self.db.resolve(self.parse('(0,2)=s4count:/2-1:_ZN6Widget5countE;value:-1,0,32;;'))
        self.assertEqual([member[0] for member in node.args[1]], ['value'])
        static, = node.cpp.static_members
        self.assertEqual((static.name, static.linkage, static.access), ('count', '_ZN6Widget5countE', 'public'))
        self.assertEqual(self.db.resolve(static.type).args, ('int', 4))
        self.assertEqual(shape(self.db, node)['size'], 4)

    def test_cpp_static_crossrefs_preserve_type_and_linkage(self):
        from layout import shape
        for tag, name in (('e', 'State'), ('s', 'Widget'),
                          ('e', 'alpha::State'), ('s', 'alpha::Widget')):
            with self.subTest(tag=tag, name=name):
                self.db = Database()
                target = self.parse('(0,9)=' + ('eREADY:0,;' if tag == 'e' else 's4item:-1,0,32;;'))
                self.db.tags[1, name] = target
                text = 's4state:/1(0,2)=x' + tag + name + '::external_storage;value:-1,0,32;;'
                parser = Grammar(self.db, 1, text)
                node = parser.type()
                static, = node.cpp.static_members
                self.assertEqual(parser.pos, len(text))
                self.assertEqual(self.db.types[self.db.key(1, 0, 2)].args, (tag, name))
                self.assertEqual((static.name, static.linkage, static.access),
                                 ('state', 'external_storage', 'protected'))
                self.assertEqual(self.db.resolve(static.type).kind, 'enum' if tag == 'e' else 'struct')
                self.assertEqual(shape(self.db, node)['size'], 4)

    def test_cpp_static_crossrefs_propagate_through_terminal_type_wrappers(self):
        wrappers = [
            ('*(0,2)=xsalpha::Widget', 'pointer'),
            ('(0,3)=k(0,2)=xsalpha::Widget', 'const'),
            ('ar-1;0;1;(0,2)=xsalpha::Widget', 'array'),
            ('*f(0,2)=xsalpha::Widget', 'pointer'),
        ]
        for wrapped, kind in wrappers:
            with self.subTest(wrapped=wrapped):
                self.db = Database()
                parser = Grammar(self.db, 1, 's4state:' + wrapped + '::plain_storage;value:-1,0,32;;')
                node = parser.type()
                static, = node.cpp.static_members
                self.assertEqual(parser.pos, len(parser.text))
                self.assertEqual(static.type.kind, 'ref' if wrapped.startswith('(') else kind)
                self.assertEqual(static.linkage, 'plain_storage')
                self.assertEqual(self.db.types[self.db.key(1, 0, 2)].args, ('s', 'alpha::Widget'))

    def test_nonstatic_qualified_crossrefs_keep_their_terminator(self):
        for wrapped in ('(0,2)=xsalpha::Widget', '*(0,2)=xsalpha::Widget',
                        'ar-1;0;1;(0,2)=xsalpha::Widget'):
            with self.subTest(wrapped=wrapped):
                self.db = Database()
                parser = Grammar(self.db, 1, 's4value:' + wrapped + ':,0,32;;')
                node = parser.type()
                self.assertEqual(parser.pos, len(parser.text))
                self.assertEqual(node.args[1][0][0], 'value')
                self.assertIsNone(node.cpp)
                self.assertEqual(self.db.types[self.db.key(1, 0, 2)].args, ('s', 'alpha::Widget'))

    def test_database_static_crossrefs_keep_member_context(self):
        for tag, definition, kind in (('e', 'eREADY:0,;', 'enum'),
                                      ('s', 's4item:-1,0,32;;', 'struct')):
            with self.subTest(tag=tag):
                db = Database()
                records = [('one.cpp', 0x64, 0, 0, 0),
                           ('alpha::State:Tt(0,9)=' + definition, 0x80, 0, 0, 0),
                           ('local:(0,1)=s4state:/1(0,2)=x' + tag +
                            'alpha::State::plain_storage;value:-1,0,32;'
                            'make::(0,4)=fx' + tag + 'alpha::State::make;2A?;;', 0x80, 0, 0, 0),
                           ('StateAlias:t(0,3)=(0,2)', 0x80, 0, 0, 0)]
                with patch('stabs.macho_symbols', return_value=iter(records)):
                    db.read('synthetic')
                self.assertEqual(db.errors, [])
                self.assertEqual(db.types[db.key(1, 0, 2)].args, (tag, 'alpha::State'))
                static, = db.types[db.key(1, 0, 1)].cpp.static_members
                self.assertEqual((db.resolve(static.type).kind, db.resolve(static.type).name),
                                 (kind, 'alpha::State'))
                method, = db.types[db.key(1, 0, 1)].cpp.methods
                result = db.resolve(method.type).args[0]
                self.assertEqual((method.linkage, method.dispatch), ('make', 'static'))
                self.assertEqual(result.args, (tag, 'alpha::State'))
                self.assertEqual(db.names[db.key(1, 0, 3)], 'StateAlias')

    def test_failed_outer_definition_still_recovers_nested_types(self):
        records = [('one.cpp', 0x64, 0, 0, 0),
                   ('Broken:Tt(0,1)=s4unsupported:Z;valid:(0,2)=eREADY:0,;;', 0x80, 0, 0, 0),
                   ('Alias:t(0,3)=(0,2)', 0x80, 0, 0, 0)]
        with patch('stabs.macho_symbols', return_value=iter(records)):
            self.db.read('synthetic')
        self.assertEqual(self.db.errors, [(1, 'type descriptor Z')])
        self.assertEqual(self.db.types[self.db.key(1, 0, 2)].kind, 'enum')
        self.assertEqual(self.db.resolve(self.parse('(0,3)')).kind, 'enum')
        self.assertEqual(self.db.names[self.db.key(1, 0, 3)], 'Alias')

    def test_cpp_static_method_preserves_function_type(self):
        from layout import shape
        node = self.db.resolve(self.parse('(0,2)=s4value:-1,0,32;make::(0,3)=f-1:make;2A?;;'))
        method, = node.cpp.methods
        self.assertEqual(method.dispatch, 'static')
        self.assertEqual(self.db.resolve(method.type).kind, 'function')
        self.assertEqual(shape(self.db, node)['size'], 4)

    def test_cpp_static_method_crossref_return_preserves_linkage(self):
        for name in ('Widget', 'alpha::Widget'):
            with self.subTest(name=name):
                self.db = Database()
                parser = Grammar(self.db, 1, 's4value:-1,0,32;make::(0,3)=fxs' + name + '::make;2A?;;')
                node = parser.type()
                method, = node.cpp.methods
                self.assertEqual(parser.pos, len(parser.text))
                self.assertEqual((method.linkage, method.dispatch), ('make', 'static'))
                self.assertEqual(self.db.resolve(method.type).args[0].args, ('s', name))

    def test_cpp_semicolon_separated_overloads_remain_supported(self):
        node = self.db.resolve(self.parse('s4value:-1,0,32;act::(0,3)=##-1;:first;2A.;(0,4)=##-1;:second;2A.;;'))
        self.assertEqual([method.linkage for method in node.cpp.methods], ['first', 'second'])

    def test_cpp_base_storage_is_never_a_c_struct(self):
        from layout import shape
        self.parse('(0,1)=s4base:-1,0,32;;')
        node = self.db.resolve(self.parse('(0,2)=s8!1,020,(0,1);value:-1,32,32;;'))
        self.assertEqual(node.kind, 'cpp_class')
        base, = node.cpp.bases
        self.assertEqual((base.virtual, base.access, base.bit_offset), (False, 'public', 0))
        self.assertEqual(self.db.resolve(base.type).kind, 'struct')
        with self.assertRaisesRegex(Unsupported, 'unsupported value type cpp_class'):
            shape(self.db, node)

    def test_cpp_virtual_method_and_trailer_are_retained(self):
        from layout import shape
        text = '(0,2)=s4value:-1,0,32;call::(0,3)=##-1;:target;2B*0;(0,2);;;~%(0,2);'
        parser = Grammar(self.db, 1, text)
        node = self.db.resolve(parser.type())
        self.assertEqual(parser.pos, len(text))
        self.assertEqual(node.kind, 'cpp_class')
        method, = node.cpp.methods
        self.assertEqual((method.dispatch, method.virtual_index, method.qualifier), ('virtual', 0, 'const'))
        self.assertEqual(method.defining_class, self.parse('(0,2)'))
        self.assertEqual(node.cpp.vtable_base, self.parse('(0,2)'))
        with self.assertRaisesRegex(Unsupported, 'unsupported value type cpp_class'):
            shape(self.db, node)

    def test_cpp_virtual_base_and_pointer_remain_opaque(self):
        from layout import shape
        ref = self.parse('(0,2)=s4!2,100,(0,1);0132,(0,3);value:-1,0,32;;')
        node = self.db.resolve(ref)
        self.assertEqual([(base.virtual, base.access, base.bit_offset) for base in node.cpp.bases],
                         [(True, 'private', 0), (False, 'protected', 32)])
        self.assertEqual(shape(self.db, self.parse('*(0,2)')),
                         dict(kind='pointer', ctype='void *', size=4, align=4))
        with self.assertRaisesRegex(Unsupported, 'unsupported value type cpp_class'):
            shape(self.db, ref)

    def test_cpp_tags_keep_names_and_virtual_trailers_inside_nested_types(self):
        from layout import shape
        records = [('one.cpp', 0x64, 0, 0, 0),
                   ('alpha::Base:Tt(0,1)=s4value:-1,0,32;;~%(0,1);', 0x80, 0, 0, 0),
                   ('Alias:t(0,2)=xsalpha::Base:', 0x80, 0, 0, 0)]
        with patch('stabs.macho_symbols', return_value=iter(records)):
            self.db.read('synthetic')
        node = self.db.resolve(self.parse('(0,2)'))
        self.assertEqual((node.kind, node.name), ('cpp_class', 'alpha::Base'))
        parser = Grammar(self.db, 1, 's4nested:(0,3)=s4value:-1,0,32;;~%(0,1);,0,32;;')
        outer = parser.type()
        self.assertEqual(parser.pos, len(parser.text))
        with self.assertRaisesRegex(Unsupported, 'unsupported value type cpp_class'):
            shape(self.db, outer)

    def test_cpp_virtual_field_may_omit_width(self):
        node = self.db.resolve(self.parse('(0,2)=s8$vf2:*(0,2),0;value:-1,32,32;;'))
        self.assertEqual(node.kind, 'cpp_class')
        name, _, bit, bits = node.args[1][0]
        self.assertEqual((name, bit, bits), ('$vf2', 0, None))

    def test_malformed_cpp_records_fail_explicitly(self):
        cases = [
            's4value:/3-1,0,32;;',
            's4value:/9-1,0,32;;',
            's4!1,220,-1;;',
            's4!-1,;',
            's4count:-1:;;',
            's4value:-1,0;;',
            's4call::(0,3)=##-1;:target;2Z.;;',
            's4call::(0,3)=##-1;:target;2B*0;;;',
            's4call::(0,3)=##-1;:target;2B.trash;;',
            's4call::(0,3)=f-1:target;2A?trash;;',
            's4value:-1,0,32;;~%;',
            's4value:-1,0,32;;~%-1',
            's4!2,020,-1;;',
            's4count:-1:target',
            's4state:(0,2)=xealpha::State::external_storage',
            's4state:(0,2)=xealpha::State::external_storage;',
            's4state:(0,2)=xealpha::State,0,32;;',
            's4call::(0,3)=##-1;:target;2B*0;-1;;',
            'xsalpha::Widget',
            'xealpha::State::external_storage;',
            'xs:',
            'x',
        ]
        for text in cases:
            with self.subTest(text=text), self.assertRaises(Unsupported):
                self.parse(text)


class HeaderReuseTests(unittest.TestCase):
    def test_enum_typedef_has_complete_definition(self):
        from c_headers import Headers
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'src' / 'headers'
            root.mkdir(parents=True)
            (root / 'types.h').write_text('enum Color { RED = 1 };\ntypedef enum Color Color;\nstruct Paint { Color value; };\n')
            index = Headers(root)
            output = index.render({'struct Paint'})
            self.assertIn('enum Color { RED = 1 };', output)
            self.assertLess(output.index('enum Color { RED = 1 };'), output.index('struct Paint {'))

    def test_pointer_array_is_not_equivalent_to_byte_array(self):
        from c_headers import Headers, equivalent
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'types.h').write_text('struct A { char *p[2]; };\nstruct B { unsigned char p[8]; };\n')
            index = Headers(root)
            self.assertFalse(equivalent(index.aggregate('struct A'), index.aggregate('struct B')))


def synthetic_macho(path, values, symbols):
    """Small original fixture, unrelated to any proprietary file."""
    data_offset = 28 + 124 + 24
    sym_offset = data_offset + len(values)
    strings, nlist = b'\0', b''
    for name, address in symbols:
        nlist += struct.pack('<IBBHI', len(strings), 0xf, 1, 0, 0x2000 + address)
        strings += name.encode() + b'\0'
    header = struct.pack('<7I', 0xfeedface, 7, 3, 2, 2, 148, 0)
    segment = struct.pack('<II16s8I', 1, 124, b'__DATA', 0x2000, len(values),
                          data_offset, len(values), 3, 3, 1, 0)
    section = struct.pack('<16s16s9I', b'__data', b'__DATA', 0x2000, len(values),
                          data_offset, 2, 0, 0, 0, 0, 0)
    symtab = struct.pack('<6I', 2, 24, sym_offset, len(symbols), sym_offset + len(nlist), len(strings))
    path.write_bytes(header + segment + section + symtab + values + nlist + strings)


class RecoveryTests(unittest.TestCase):
    def test_real_pointer_survives_and_scalar_relocation_is_removed(self):
        from elf32 import Object
        from macho32 import MachO32
        from recover import recover_object
        from stabs import Type
        integer = Type('builtin', ('unsigned int', 4))
        debug = Type('struct', (8, (('pointer', Type('pointer', (integer,)), 0, 32),
                                    ('flags', integer, 32, 32))))
        source = Object('value', 0, bytes(8), {0: ('real_symbol', 0), 4: ('false_symbol', 17)}, '.data')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'fixture'
            synthetic_macho(path, struct.pack('<2I', 0x8040, 0x1234), [('__ZL5value', 0)])
            fixed, ty, report = recover_object(source, Database(),
                                               {'value': [SimpleNamespace(type=debug)]}, MachO32(path))
            self.assertEqual(fixed.relocs, {0: ('real_symbol', 0)})
            self.assertEqual(fixed.data, struct.pack('<2I', 0, 0x1234))
            self.assertEqual(report['false_relocations'], [4])

    def test_nonzero_nonrelocated_mismatch_fails(self):
        from elf32 import Object
        from macho32 import MachO32
        from recover import recover_object
        from stabs import Type
        debug = Type('array', (Type('builtin', ('unsigned char', 1)), 0, 7))
        source = Object('value', 0, b'\x01' + bytes(7), {4: ('false', 0)}, '.data')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'fixture'
            synthetic_macho(path, b'\x02' + bytes(7), [('_value', 0)])
            with self.assertRaisesRegex(ValueError, 'nonzero non-relocated bytes disagree'):
                recover_object(source, Database(), {'value': [SimpleNamespace(type=debug)]}, MachO32(path))

    def test_function_static_name_and_next_symbol_bounds(self):
        from macho32 import MachO32
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'fixture'
            synthetic_macho(path, bytes(16), [('__ZZL4TestvE5value', 0), ('_next', 8)])
            obj = MachO32(path)
            self.assertEqual(obj.read_object('value', 8)[1]['next_symbol_extent'], 8)
            with self.assertRaisesRegex(ValueError, 'exceeds next-symbol extent'):
                obj.read_object('value', 12)

    def test_compiled_unaligned_pointer_is_rejected(self):
        from check_alignment import check_alignment
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'fixture.c'
            path.write_text('extern char target[];\nstruct __attribute__((packed)) S { char b[4]; void *p; };\n'
                            'struct S broken = {{0}, target};\nstruct { char b[8]; void *p; } valid = {{0}, target};\n')
            obj = path.with_suffix('.o')
            subprocess.run([os.environ.get('CLANG', 'clang'), '-target', 'arm64-apple-macos',
                            '-ffreestanding', '-g', '-c', str(path), '-o', str(obj)], check=True)
            with self.assertRaisesRegex(ValueError, r'_broken\+0x4'):
                check_alignment(obj)
            path.write_text('extern char target[];\nstruct { char b[8]; void *p; } valid = {{0}, target};\n')
            subprocess.run([os.environ.get('CLANG', 'clang'), '-target', 'arm64-apple-macos',
                            '-ffreestanding', '-g', '-c', str(path), '-o', str(obj)], check=True)
            self.assertEqual(check_alignment(obj), 1)


if __name__ == '__main__':
    unittest.main()
