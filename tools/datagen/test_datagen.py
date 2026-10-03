import tempfile
import unittest
from pathlib import Path
import os
import struct
import subprocess
from types import SimpleNamespace
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
