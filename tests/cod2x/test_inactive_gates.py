#!/usr/bin/env python3
"""Exercise stock isolation with real compiler output and complete includes."""
import importlib.util
from pathlib import Path
import shlex
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('inactive', Path(__file__).with_name('check_inactive_gates.py'))
inactive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inactive)


class CompilerIsolation(unittest.TestCase):
    def compare(self, before, after, candidate_flags=''):
        with tempfile.TemporaryDirectory(prefix='cod2-inactive-test-') as temporary:
            root = Path(temporary)
            sides = []
            for name, header, extra in (('base', before, ''), ('candidate', after, candidate_flags)):
                tree = root / name
                tree.mkdir()
                source = tree / 'probe.c'
                source.write_text('#include "nested.inc"\nint probe(void) { return implementation(); }\n')
                (tree / 'nested.inc').write_text(header)
                entry = {'file': str(source), 'command': shlex.join([
                    'clang', '-O3', '-c', str(source), '-o', str(tree / 'probe.o')]) + ' ' + extra}
                sides.append((tree, tree / 'build', {'client:probe.c': entry}))
            return inactive.compare((0, 'client:probe.c'), sides, root)

    def test_extension_only_include_changes_disappear(self):
        before = '#if defined(COD2_CODX) && COD2_CODX\nint extension = 1;\n#endif\n'
        after = before.replace('1;', '9;')
        common = 'static int implementation(void) { return 7; }\n'
        row = self.compare(before + common, after + common)
        self.assertEqual(row['status'], 'pass')
        self.assertEqual(set(row['configurations']), {'undefined', 'zero'})

    def test_active_include_change_fails(self):
        row = self.compare('static int implementation(void) { return 7; }',
                           'static int implementation(void) { return 9; }')
        self.assertEqual(row['status'], 'changed')
        self.assertTrue(all(r['status'] == 'changed-assembly' for r in row['configurations'].values()))

    def test_spaces_inside_literals_are_significant(self):
        row = self.compare('static int implementation(void) { return "a b"[2]; }',
                           'static int implementation(void) { return "a  b"[2]; }')
        self.assertEqual(row['status'], 'changed')

    def test_unused_private_declaration_emits_identical_code(self):
        common = 'static int implementation(void) { return 7; }'
        row = self.compare('struct Private { int a; };\n' + common,
                           'struct Private { long a; double b; };\n' + common)
        self.assertEqual(row['status'], 'pass')
        self.assertTrue(all(r['status'] == 'identical-assembly' for r in row['configurations'].values()))

    def test_compile_error_is_not_a_pass(self):
        with self.assertRaises(RuntimeError):
            self.compare('static int implementation(void) { return 7; }', '#error missing provider')

    def test_compile_flag_change_fails_even_with_same_body(self):
        common = 'static int implementation(void) { return 7; }'
        self.assertEqual(self.compare(common, common, '-fwrapv')['status'], 'changed-flags')


if __name__ == '__main__':
    unittest.main()
