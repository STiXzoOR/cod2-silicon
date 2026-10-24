"""Diagnostic profile tests using independently written synthetic source."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools/oracle/instrument_reference.py'
SPEC = importlib.util.spec_from_file_location('instrument_reference', SCRIPT)
ORACLE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ORACLE)

# Deliberately artificial bodies, not excerpts from the reference source.
SNAP = ('int untouched_before = 17;\n'
        'void Sys_SnapVector(float *v) { v[0] += 42.0f; }\n'
        'int untouched_after = 23;\n')
HOOK = ('#include "common_types.h"\n'
        '            cmd.angles[1] = 0;\n'
        '            cmd.buttons = 0;\n')


class OracleProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cod2-oracle-profile-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.destination = self.root / 'instrumented'
        self.patch = self.root / 'private.patch'
        fixtures = {
            'src/server/sv_client_mp.cpp': 'void SV_ClientThink(void *cl, void *cmd) {}',
            'src/server/sv_main_mp.cpp': ('void SV_BotUserMove() {}\n'
                'void SV_RunFrame() { G_RunFrame(17); }\n'
                'void SV_Frame() { Scr_SetLoading(0); }'),
            'src/qcommon/common.cpp': 'void Com_Frame() {}',
            'src/game/g_main_mp.cpp': 'void G_InitGame(int randomSeed) {}',
            'src/unix/unix_shared.cpp': SNAP,
            'Makefile': 'SRCS := \\\n\tsynthetic.cpp\n',
        }
        for relative, text in fixtures.items():
            target = self.source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text if relative == 'Makefile' else '#include "synthetic.h"\n' + text)

    def generate(self, **kwargs):
        with patch.object(ORACLE.subprocess, 'run', return_value=
                          subprocess.CompletedProcess([], 0, HOOK, '')):
            ORACLE.instrument(self.source, self.destination, self.patch, **kwargs)

    def test_default_preserves_reference_snap_bytes(self):
        self.generate()
        relative = 'src/unix/unix_shared.cpp'
        self.assertEqual((self.source / relative).read_bytes(),
                         (self.destination / relative).read_bytes())
        self.assertNotIn(relative, self.patch.read_text())
        metadata = json.loads((self.destination / 'oracle-profile.json').read_text())
        self.assertEqual(metadata['profile'], 'linux-stock')
        self.assertFalse(metadata['engine_behavior_modified'])
        self.assertEqual(metadata['changed_functions'], [])
        self.assertIsNone(metadata['executable_sha256'])

    def test_diagnostic_changes_only_snap_body_beyond_stock_instrumentation(self):
        self.generate()
        stock = self.destination
        self.destination = self.root / 'diagnostic'
        self.patch = self.root / 'diagnostic.patch'
        self.generate(profile='mac-snap-diagnostic')
        changed = [str(path.relative_to(stock)) for path in stock.rglob('*')
                   if path.is_file() and path.read_bytes() !=
                   (self.destination / path.relative_to(stock)).read_bytes()]
        self.assertEqual(sorted(changed), ['oracle-profile.json', 'src/unix/unix_shared.cpp'])
        text = (self.destination / 'src/unix/unix_shared.cpp').read_text()
        before = (self.source / 'src/unix/unix_shared.cpp').read_text()
        start, end = ORACLE.function_bounds(before, 'Sys_SnapVector')
        new_start, new_end = ORACLE.function_bounds(text, 'Sys_SnapVector')
        self.assertEqual(before[:start], text[:new_start])
        self.assertEqual(before[end:], text[new_end:])
        self.assertEqual(before, '#include "synthetic.h"\n' + SNAP)
        metadata = json.loads((self.destination / 'oracle-profile.json').read_text())
        self.assertEqual(metadata['profile'], 'mac-snap-diagnostic')
        self.assertTrue(metadata['engine_behavior_modified'])
        self.assertEqual(metadata['changed_functions'], ['Sys_SnapVector'])
        self.assertIsNone(metadata['executable_sha256'])

    def test_unknown_profile_rejected_before_copying(self):
        with self.assertRaisesRegex(ValueError, 'profile'):
            self.generate(profile='mac-ish')
        self.assertFalse(self.destination.exists())
        self.assertFalse(self.patch.exists())

    def test_unknown_cli_profile_rejected_before_copying(self):
        result = subprocess.run([sys.executable, str(SCRIPT), '--profile', 'mac-ish',
            '--source', str(self.source), '--destination', str(self.destination),
            '--patch', str(self.patch)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid choice', result.stderr)
        self.assertFalse(self.destination.exists())

    def test_missing_duplicate_and_unclosed_snap_rejected_before_copying(self):
        for text in ('void unrelated() {}', SNAP + SNAP, 'void Sys_SnapVector(float *v) {'):
            with self.subTest(text=text):
                (self.source / 'src/unix/unix_shared.cpp').write_text(text)
                with self.assertRaisesRegex(ValueError, 'Sys_SnapVector'):
                    self.generate(profile='mac-snap-diagnostic')
                self.assertFalse(self.destination.exists())
                self.assertFalse(self.patch.exists())

    def test_comments_and_literals_do_not_count_as_definitions(self):
        path = self.source / 'src/unix/unix_shared.cpp'
        text = ('// void Sys_SnapVector(float *v) { }\n'
                'const char *note = "Sys_SnapVector(float *v) { }";\n' + SNAP)
        path.write_text(text)
        self.generate(profile='mac-snap-diagnostic')
        result = (self.destination / 'src/unix/unix_shared.cpp').read_text()
        self.assertTrue(result.startswith(text[:text.index('int untouched_before')]))

    @unittest.skipUnless(shutil.which('clang++'), 'requires a host C++ compiler')
    def test_generated_body_matches_mac_conversion_results_without_undefined_casts(self):
        self.generate(profile='mac-snap-diagnostic')
        body = (self.destination / 'src/unix/unix_shared.cpp').read_text()
        fixture = self.root / 'snap.cpp'
        fixture.write_text(body.replace('#include "synthetic.h"\n', '') + r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
int main()
{
    const uint32_t cases[][2] = {
        {0x3ff33333, 0x3f800000}, {0xbff33333, 0xbf800000}, // +/-1.9 -> +/-1
        {0x3fc00000, 0x3f800000}, {0xbfc00000, 0xbf800000}, // +/-1.5 -> +/-1
        {0x40200000, 0x40000000}, {0xc0200000, 0xc0000000}, // +/-2.5 -> +/-2
        {0x3f000000, 0}, {0xbf000000, 0}, {0, 0}, {0x80000000, 0},
        {1, 0}, {0x80000001, 0}, // signed subnormals
        {0x4effffff, 0x4effffff}, {0xceffffff, 0xceffffff}, // int32 boundary neighbors
        {0x4f000000, 0xcf000000}, {0xcf000000, 0xcf000000},
        {0xcf000001, 0xcf000000}, {0x7f7fffff, 0xcf000000},
        {0x7f800000, 0xcf000000}, {0xff800000, 0xcf000000},
        {0x7fc12345, 0xcf000000}, {0xffc12345, 0xcf000000},
        {0x7f800001, 0xcf000000}, {0xff800001, 0xcf000000}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        float v[3];
        for (unsigned j = 0; j < 3; ++j) memcpy(&v[j], &cases[i][0], 4);
        Sys_SnapVector(v);
        for (unsigned j = 0; j < 3; ++j) {
            uint32_t actual;
            memcpy(&actual, &v[j], 4);
            if (actual != cases[i][1]) {
                fprintf(stderr, "case %u component %u: %08x != %08x\n",
                        i, j, actual, cases[i][1]);
                return 1;
            }
        }
    }
    return 0;
}
''')
        prefix = ['taskpolicy', '-b', 'nice', '-n', '19'] if sys.platform == 'darwin' else []
        executable = self.root / 'snap'
        subprocess.run([*prefix, 'clang++', '-std=c++98', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fsanitize=undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            str(fixture), '-o', str(executable)], check=True, capture_output=True, text=True)
        subprocess.run([str(executable)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
