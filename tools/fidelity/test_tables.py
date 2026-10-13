import importlib.util
from pathlib import Path
import sys
import unittest

SPEC = importlib.util.spec_from_file_location('fidelity_tables', Path(__file__).with_name('tables.py'))
tables = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = tables
if Path(SPEC.origin).exists(): SPEC.loader.exec_module(tables)


class TableTests(unittest.TestCase):
    def test_long_array_uses_native_stride(self):
        ty = dict(kind='array', count=2, size=8, align=4,
                  child=dict(kind='scalar', ctype='unsigned long', size=4, align=4))
        native = tables.native_layout(ty)
        self.assertEqual(native['size'], 16)
        self.assertEqual(list(tables.scalar_fields(ty, native)),
                         [(0, 0, 'unsigned long', 4, 8), (4, 8, 'unsigned long', 4, 8)])

    def test_integer_widening_compares_values_and_float_compares_bits(self):
        ty = dict(kind='scalar', ctype='long', size=4, align=4)
        self.assertEqual(tables.mismatch_count(b'\xff'*4, b'\xff'*8, ty), 0)
        self.assertEqual(tables.mismatch_count(b'\x01\0\0\0', b'\x02'+b'\0'*7, ty), 1)
        ty = dict(kind='scalar', ctype='float', size=4, align=4)
        self.assertEqual(tables.mismatch_count(b'\0'*4, b'\0'*3+b'\x80', ty), 1)

    def test_pointer_bearing_table_is_not_guessed(self):
        ty = dict(kind='pointer', size=4, align=4, ctype='void *')
        with self.assertRaises(ValueError): tables.mismatch_count(b'\0'*4, b'\0'*8, ty)

if __name__ == '__main__': unittest.main()
