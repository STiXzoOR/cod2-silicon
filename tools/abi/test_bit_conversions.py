#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

CHECK = Path(__file__).with_name('astdump_strict.py')
class BitConversions(unittest.TestCase):
    def audit(self, code):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'input.c'
            source.write_text(code)
            db = root / 'compile_commands.json'
            db.write_text(json.dumps([dict(file=str(source), directory=directory, arguments=['clang', '-c', str(source), '-o', 'input.o'])]))
            return subprocess.run(['python3', str(CHECK), str(db)], capture_output=True, text=True)
    def test_rejects_feeder_bits_converted_by_value(self):
        result = self.audit('int count(float); int f(float x) { return count(*(int *)&x); }')
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('1 violations', result.stdout)
    def test_rejects_reverse_conversion(self):
        self.assertEqual(self.audit('int f(int x) { return *(float *)&x; }').returncode, 1)
    def test_accepts_numeric_and_bit_reinterpretation(self):
        self.assertEqual(self.audit('float f(int x) { return x; } float g(int x) { return *(float *)&x; }').returncode, 0)
    def test_compile_error_is_failure(self):
        self.assertNotEqual(self.audit('invalid C source').returncode, 0)
if __name__ == '__main__': unittest.main()
