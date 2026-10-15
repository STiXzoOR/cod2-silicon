"""Synthetic tests for the original-code math oracle's acceptance gate."""
import copy
import math
import json
import tempfile
from pathlib import Path
from unittest.mock import patch
import unittest

import math_reference as oracle


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.cases = [
            dict(case=0, function='AngleSubtract', arguments=['00000000', '00000000']),
            dict(case=1, function='LerpAngle', arguments=['00000000', '3f800000', '3f000000']),
        ]
        self.native = [dict(case=0, bits='00000000'), dict(case=1, bits='3f000000')]

    def test_signed_zero_and_one_ulp_differences_both_fail(self):
        original = [dict(case=0, bits='80000000'), dict(case=1, bits='3f000001')]
        result = oracle.compare_observations(self.cases, self.native, original)
        self.assertFalse(result['passed'])
        self.assertEqual(result['mismatches'], 2)
        self.assertEqual(len(result['observations']), 2)
        self.assertEqual([row['original_bits'] for row in result['observations']],
                         ['80000000', '3f000001'])

    def test_equal_complete_results_pass(self):
        result = oracle.compare_observations(self.cases, self.native, copy.deepcopy(self.native))
        self.assertTrue(result['passed'])
        self.assertEqual(result['compared'], 2)

    def test_missing_duplicate_unexpected_and_nonfinite_results_are_rejected(self):
        invalid = [self.native[:1], self.native + self.native[:1],
                   self.native + [dict(case=2, bits='00000000')],
                   [dict(case=0, bits='7f800000'), self.native[1]],
                   [dict(case=0, bits='not bits'), self.native[1]]]
        for rows in invalid:
            for native, original in [(rows, self.native), (self.native, rows)]:
                with self.subTest(native=native, original=original):
                    with self.assertRaises(ValueError):
                        oracle.compare_observations(self.cases, native, original)

    def test_empty_run_cannot_pass(self):
        with self.assertRaises(ValueError):
            oracle.compare_observations([], [], [])


class RunnerTests(unittest.TestCase):
    def run_mocked_reference(self, introduce_import):
        class Reference:
            imports = {'self_test_model': 3}

            def call(self, *args, **kwargs):
                if introduce_import:
                    self.imports['math_model'] = 1
                return 0.0

        reference = Reference()
        cases = [dict(case=0, function='AngleSubtract', arguments=['00000000', '00000000'])]
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'result.json'
            with patch.object(oracle, 'sha256', return_value='synthetic'), \
                 patch.object(oracle, 'build_cases', return_value=cases), \
                 patch.object(oracle, 'load_reference', return_value=(reference, reference.imports.copy())), \
                 patch.object(oracle, 'compile_native', return_value=Path(directory) / 'probe'), \
                 patch.object(oracle, 'observe_native', return_value=[dict(case=0, bits='00000000')]):
                status = oracle.main(['--compile-db', directory, '--mac-binary', directory,
                                      '--output', str(output)])
            return status, json.loads(output.read_text())

    def test_matching_outputs_cannot_pass_after_modeled_import(self):
        status, report = self.run_mocked_reference(True)
        self.assertEqual(status, 1)
        self.assertFalse(report['complete'])
        self.assertFalse(report['passed'])
        self.assertEqual(report['original_oracle']['math_imports'], {'math_model': 1})
        self.assertIn('modeled import', report['error']['message'])

    def test_unchanged_nonzero_self_test_baseline_can_pass(self):
        status, report = self.run_mocked_reference(False)
        self.assertEqual(status, 0)
        self.assertTrue(report['complete'])
        self.assertTrue(report['passed'])
        self.assertEqual(report['original_oracle']['math_imports'], {})


class CaseTests(unittest.TestCase):
    def test_endpoint_rounding_regression_is_explicitly_sampled(self):
        cases = [case for case in oracle.build_cases() if case.get('scenario')]
        self.assertEqual(len(cases), 1)
        self.assertEqual(cases[0]['function'], 'LerpAngle')
        self.assertEqual(cases[0]['arguments'], ['c3340000', '4333ffff', '3f400000'])

    def test_cases_include_finite_bounded_edges_and_seeded_samples(self):
        cases = oracle.build_cases()
        self.assertEqual(cases, oracle.build_cases())
        self.assertEqual({case['function'] for case in cases},
                         {'AngleNormalize360', 'AngleNormalize180', 'AngleSubtract', 'LerpAngle'})
        self.assertGreater(len(cases), 1000)
        self.assertEqual([case['case'] for case in cases], list(range(len(cases))))
        for case in cases:
            values = [oracle.float_from_bits(value) for value in case['arguments']]
            self.assertTrue(all(math.isfinite(value) for value in values))
            self.assertTrue(all(abs(value) <= 1000000 for value in values))
            if case['function'] == 'LerpAngle':
                self.assertLessEqual(abs(values[2]), 2)
        arguments = {value for case in cases for value in case['arguments']}
        self.assertTrue({'00000000', '80000000', '00000001', '80000001'} <= arguments)


if __name__ == '__main__':
    unittest.main()
