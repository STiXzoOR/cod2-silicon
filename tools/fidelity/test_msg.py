import sys
from pathlib import Path
import unittest
import os
import struct
import subprocess
import tempfile

sys.path.insert(0, str(Path(__file__).parent))
from spotcheck_msg import test_inputs
from tables import MachO64, read_native

ROOT = Path(__file__).resolve().parents[2]


class MessageProbeTests(unittest.TestCase):
    def test_compiled_native_masks_cover_exactly_the_wire_width(self):
        with tempfile.TemporaryDirectory() as tmp:
            obj = Path(tmp) / 'data.o'
            subprocess.run([os.environ.get('CLANG', 'clang'), '-target',
                            'arm64-apple-macos', '-ffreestanding', '-std=c11',
                            '-c', str(ROOT / 'build/lp64_gen/data_native.c'),
                            '-o', str(obj)], check=True, capture_output=True)
            masks = struct.unpack('<33I', read_native(MachO64(obj), '_kbitmask', 132))
            self.assertEqual(masks, tuple((1 << width) - 1 for width in range(33)))

    def test_probe_exposes_out_of_field_key_bit(self):
        reference = [(1 << bits) - 1 for bits in range(33)]
        native = list(reference)
        native[18] = reference[19]
        cases = list(test_inputs(reference, native))
        self.assertEqual(len(cases), 32)
        bits, key, wire, expected = cases[17]
        self.assertEqual((bits, key, wire, expected), (18, 1 << 18, 1, 1))
        self.assertNotEqual(wire ^ (key & native[bits]), expected)
        self.assertEqual(cases[3][3], 1)


if __name__ == '__main__':
    unittest.main()
