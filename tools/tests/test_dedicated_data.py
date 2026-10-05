import importlib.util
from pathlib import Path
import platform
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('dedicated_data', Path(__file__).resolve().parents[1] / 'server/dedicated_data.py')
dedicated_data = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dedicated_data)
dedicated_view = dedicated_data.dedicated_view


class DedicatedDataTest(unittest.TestCase):
    def test_only_retention_attribute_changes(self):
        source = ('int used = 7;\n'
                  'void *slot __asm__(DG_ASM("slot")) '
                  '__attribute__((used, aligned(DG_ALIGN), section(DG_RELRO))) = owner;\n'
                  '__asm__(".globl alias\\n.set alias,slot");\n')
        result = dedicated_view(source)
        self.assertEqual(result, source.replace('used, aligned', 'aligned'))
        self.assertEqual(dedicated_view(result), result)

    @unittest.skipUnless(platform.system() == 'Darwin', 'ld64 retention semantics')
    def test_link_discards_client_import_but_requires_live_server_owner(self):
        source = ('#define DG_ALIGN 16\n#define DG_RELRO "__DATA_CONST,__const"\n'
                  'extern int client_only;\n'
                  'void *client_slot __attribute__((used, aligned(DG_ALIGN), section(DG_RELRO))) = &client_only;\n'
                  'extern int server_owner;\n'
                  'void *server_slot __attribute__((used, aligned(DG_ALIGN), section(DG_RELRO))) = &server_owner;\n'
                  'int main(void) { return *(int *)server_slot != 20; }\n')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            unit = root / 'data.c'
            owner = root / 'owner.c'
            owner.write_text('int server_owner = 20;\n')
            command = ['clang', '-Wl,-dead_strip', str(unit), str(owner), '-o', str(root / 'server')]
            unit.write_text(source)
            failure = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(failure.returncode, 0)
            self.assertIn('_client_only', failure.stderr)
            unit.write_text(dedicated_view(source))
            subprocess.run(command, check=True, capture_output=True)
            subprocess.run([str(root / 'server')], check=True)
            failure = subprocess.run(command[:3] + command[4:], capture_output=True, text=True)
            self.assertNotEqual(failure.returncode, 0)
            self.assertIn('_server_owner', failure.stderr)


if __name__ == '__main__':
    unittest.main()
