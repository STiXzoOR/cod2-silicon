#!/usr/bin/env python3
"""tools/check_cod2x_standin.py names every script that starts the engine bare."""
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('check_cod2x_standin', ROOT / 'tools/check_cod2x_standin.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)

BARE = "import subprocess\nsubprocess.run([binary, '+set', 'fs_basepath', data, '+map', 'mp_toujane'])\n"


class StandinCheck(unittest.TestCase):
    def tree(self, files, review=''):
        root = pathlib.Path(self.enterContext(tempfile.TemporaryDirectory()))
        for name, text in {checker.REVIEW: review, **files}.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        return root

    def test_a_script_that_starts_the_engine_bare_is_named(self):
        problems = checker.check(self.tree({'tests/x/run.py': BARE, 'tests/x/compile.py': "target = 'cod2_macos.dir'\n"}))
        self.assertEqual(len(problems), 1)
        self.assertIn('tests/x/run.py: starts the engine without the CoD2x stand-in', problems[0])

    def test_setting_the_variable_or_sourcing_the_file_passes(self):
        files = {'tests/x/run.py': "os.environ.setdefault('COD2X_TEST_SERVICE_URL', 'http://127.0.0.1:9')\n" + BARE,
                 'tools/y/run.sh': '. "$repo/tools/cod2x/standin_env.sh"\n"$binary" +set fs_basepath "$data"\n'}
        self.assertEqual(checker.check(self.tree(files)), [])

    def test_a_shell_script_is_checked_too(self):
        problems = checker.check(self.tree({'scripts/go.sh': '"$binary" +set dedicated 1 +exec server.cfg\n'}))
        self.assertEqual(len(problems), 1)

    def test_the_reviewed_list_adds_and_exempts(self):
        files = {'tests/p/first.py': "subprocess.run([app / 'Contents/MacOS/cod2_macos'])\n", 'tools/w/plist.py': "args = '+set r_mode 1'\n"}
        review = 'starts tests/p/first.py\nexempt tools/w/plist.py writes a plist\n'
        problems = checker.check(self.tree(files, review))
        self.assertEqual(len(problems), 1)
        self.assertIn('tests/p/first.py', problems[0])

    def test_a_stale_or_malformed_list_is_a_failure(self):
        files = {'tools/w/plain.py': 'print(1)\n'}
        review = 'starts tests/gone.py\nexempt tools/w/plain.py no longer builds one\nexempt tools/w/plain.py\n'
        problems = '\n'.join(checker.check(self.tree(files, review)))
        self.assertIn('tests/gone.py does not exist', problems)
        self.assertIn('is exempt but builds no engine command line', problems)
        self.assertIn('expected "starts <path>" or "exempt <path> <reason>"', problems)


if __name__ == '__main__':
    unittest.main()
