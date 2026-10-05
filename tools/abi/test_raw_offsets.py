#!/usr/bin/env python3
"""Synthetic raw-offset gate tests; no licensed inputs."""
import tempfile
from pathlib import Path
import unittest

from raw_offsets import active_lines, candidates, check


class RawOffsetTests(unittest.TestCase):
    def test_raw_forms_and_ignored_comments(self):
        source = '''byte *entry;
*(const char **)((byte *)&global + 0x114c + i * 8);
*(int *)((char *)p + 113140);
*(int *)(entry + 0x13f4);
void *p = malloc(528);
#define CI_STRIDE 1208
ArchiveVec3(file, base, 0x248);
// *(int *)((byte *)p + 100)
char *text = "entry + 0x14";
'''
        self.assertEqual(set(candidates(source)), {2, 3, 4, 5, 6, 7})

    def test_negative_offset_and_duplicate_fail(self):
        text = 'int x = *(int *)((byte *)uiInfo + 0x388 - 4);'
        result = dict(sites=[dict(file='test.c', text=text)])
        self.assertTrue(candidates(text))
        self.assertTrue(check(result, []))
        manifest = [dict(file='test.c', text=text, count=1,
                         classification='safe', reason='synthetic test')]
        self.assertFalse(check(result, manifest))
        result['sites'] *= 2
        self.assertTrue(check(result, manifest))
        manifest[0]['classification'] = 'wrong on LP64'
        self.assertTrue(check(result, manifest))
        result['sites'] = result['sites'][:1]
        manifest[0]['classification'] = 'unreviewed'
        self.assertTrue(check(result, manifest))
        manifest[0]['classification'] = 'safe'
        manifest[0]['reason'] = ''
        self.assertTrue(check(result, manifest))

    def test_actual_preprocessor_gates_legacy_and_codx(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'source.c'
            path.write_text('''#if defined(COD2_X64)
#if defined(COD2_CODX)
int native_codx;
#else
int native_stock;
#endif
#else
int legacy;
#endif
''')
            entry = dict(file=str(path), directory=tmp,
                         arguments=['clang', '-DCOD2_X64=1', '-c', str(path), '-o', tmp + '/source.o'])
            _, lines = active_lines(entry)
            self.assertIn(5, lines)
            self.assertNotIn(3, lines)
            self.assertNotIn(8, lines)
            _, lines = active_lines(entry, path.read_text().replace('int native_stock;', 'int replacement;'))
            self.assertIn(5, lines)
            self.assertNotIn(3, lines)
            entry['arguments'].insert(1, '-DCOD2_CODX=1')
            _, lines = active_lines(entry)
            self.assertIn(3, lines)
            self.assertNotIn(5, lines)
            self.assertNotIn(8, lines)


if __name__ == '__main__':
    unittest.main()
