import importlib.util
from pathlib import Path
import sys
import unittest

SPEC = importlib.util.spec_from_file_location('calibrate', Path(__file__).with_name('calibrate.py'))
calibrate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = calibrate
if Path(SPEC.origin).exists(): SPEC.loader.exec_module(calibrate)


class CalibrationTests(unittest.TestCase):
    def test_unreviewed_samples_do_not_inflate_precision(self):
        result = calibrate.precision([{'verdict': 'difference'}, {'verdict': 'equivalent'}, {'verdict': 'unresolved'}])
        self.assertEqual(result['precision_lower'], 1/3)
        self.assertEqual(result['precision_upper'], 2/3)
        self.assertEqual(result['resolved_precision'], 0.5)

    def test_calibration_checks_direct_and_enclosing_evidence(self):
        audit = {'functions': [{'reference': {'name': 'Direct'}, 'comparison': {'score': 8}}],
                 'native_only': [{'name': 'Extracted', 'ancestor_signals': [{'score': 16}]}]}
        result = calibrate.coverage(audit, ['Direct', 'Extracted', 'Missing'])
        self.assertTrue(result['Direct']['flagged'])
        self.assertEqual(result['Extracted']['basis'], 'caller-context')
        self.assertFalse(result['Missing']['flagged'])

if __name__ == '__main__': unittest.main()
