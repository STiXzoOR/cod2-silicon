import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from spotcheck_msg import test_inputs


class MessageProbeTests(unittest.TestCase):
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
