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

    def test_repaired_current_cases_do_not_replace_positive_controls(self):
        def audit(score):
            return {'source_commit': 'synthetic', 'binary_sha256': 'reference',
                    'functions': [{'reference': {'name': name},
                                   'comparison': {'score': score}}
                                  for name in calibrate.KNOWN]}
        current, historical = audit(0), audit(8)
        current['source_commit'] = 'repaired'
        result = calibrate.evaluate(current, historical)
        self.assertTrue(all(r['flagged'] for r in result['known_bugs'].values()))
        self.assertFalse(any(r['flagged'] for r in result['current_known_cases'].values()))
        self.assertEqual(result['positive_control']['source_commit'], 'synthetic')
        self.assertFalse(all(r['flagged'] for r in calibrate.evaluate(current)['known_bugs'].values()))
        historical['functions'].pop()
        self.assertFalse(all(r['flagged'] for r in calibrate.evaluate(current, historical)['known_bugs'].values()))

    def test_positive_control_requires_matching_reference_and_provenance(self):
        current = {'source_commit': 'current', 'binary_sha256': 'reference', 'functions': []}
        for historical in ({'source_commit': 'old', 'binary_sha256': 'other'},
                           {'binary_sha256': 'reference'},
                           {'source_commit': 'old'}):
            with self.assertRaises(ValueError):
                calibrate.evaluate(current, historical)

if __name__ == '__main__': unittest.main()
