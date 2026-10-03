import tempfile
import unittest
from pathlib import Path
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


if __name__ == '__main__':
    unittest.main()
