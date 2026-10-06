import importlib.util
from pathlib import Path
import sys
import unittest

SPEC = importlib.util.spec_from_file_location('spotcheck', Path(__file__).with_name('spotcheck_reverb.py'))
spotcheck = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = spotcheck
if Path(SPEC.origin).exists(): SPEC.loader.exec_module(spotcheck)

class SpotcheckTests(unittest.TestCase):
    def test_extraction_preserves_nested_body_and_ignores_prototype(self):
        text = 'void Tick(int x);\nvoid Tick(int x)\n{ if(x) { Log("}"); } }\nvoid Next(void) {}'
        body = spotcheck.extract_function(text, 'Tick')
        self.assertIn('Log("}")', body)
        self.assertNotIn('Next', body)
        self.assertTrue(body.endswith('}'))

    def test_extracts_static_integer_message_reader(self):
        body = spotcheck.extract_function('static int Read(int key) { return key; }', 'Read')
        self.assertTrue(body.startswith('static int Read'))

if __name__ == '__main__': unittest.main()
