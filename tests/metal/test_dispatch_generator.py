"""Check typed dispatch generation and reject malformed existing hooks."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FILES = ('CDirect3D CDirect3DDevice CDirect3DSwapChain CDirect3DSurface '
         'CDirect3DTexture CDirect3DCubeTexture CDirect3DVolumeTexture '
         'CDirect3DVertexBuffer CDirect3DIndexBuffer CDirect3DVertexDeclaration '
         'CDirect3DVertexShader CDirect3DPixelShader CFence CMemoryBuffer '
         'CVAOPacket CVertexArrays COpenGLVAO COpenGL d3d9_real').split()
SOURCE = r"""
int CDirect3DDevice_Copy(void *object, float depth)
{
    if (object &&
        CDirect3DSurface_GetGLBlitInfo(object, &depth) &&
        depth > 0) {
        return 0;
    }
    return 1;
}
void *CDirect3DDevice_Create(void *object, void (*callback)(float, int))
{
    callback(1.0f, 2);
    return object;
}
void CDirect3DDevice_Clear(void *object)
{
    (void)object;
}
int CDirect3DDevice_Count(void)
{
    return 1;
}
void *Direct3DCreate9(unsigned int version)
{
    return 0;
}
"""

class DispatchGeneratorTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        source_dir = self.root / 'src/Mac/DirectX_9'
        source_dir.mkdir(parents=True)
        (self.root / 'src/Mac/Metal').mkdir(parents=True)
        (self.root / 'tools/metal').mkdir(parents=True)
        self.script = self.root / 'tools/metal/add_dispatch.py'
        shutil.copyfile(ROOT / 'tools/metal/add_dispatch.py', self.script)
        for name in FILES:
            (source_dir / (name + '.c')).write_text('')
        self.source = source_dir / 'CDirect3DDevice.c'
        self.source.write_text(SOURCE)

    def run_generator(self, *args):
        return subprocess.run([sys.executable, str(self.script), *args],
                              capture_output=True, text=True)

    def generate(self):
        generated = self.run_generator()
        self.assertEqual(generated.returncode, 0, generated.stderr)

    def snapshot(self):
        paths = [self.root / 'src/Mac/DirectX_9' / (name + '.c') for name in FILES]
        paths += [self.root / 'tools/metal/shim_methods.json',
                  self.root / 'src/Mac/Metal/metal_hresult.inc']
        return {path: path.read_bytes() for path in paths}

    def test_nested_call_is_not_an_entry_point(self):
        self.assertNotEqual(self.run_generator('--check').returncode, 0)
        self.generate()
        entries = json.loads((self.root / 'tools/metal/shim_methods.json').read_text())
        self.assertEqual([e['method'] for e in entries],
                         ['CDirect3DDevice_Copy', 'CDirect3DDevice_Create',
                          'CDirect3DDevice_Clear', 'CDirect3DDevice_Count',
                          'Direct3DCreate9'])
        self.assertEqual(entries[0]['arguments'], ['object', 'depth'])
        self.assertEqual(entries[1]['arguments'], ['object', 'callback'])
        self.assertEqual(entries[2]['result'], 'void')
        self.assertEqual(entries[3]['arguments'], [])
        first = self.snapshot()
        self.generate()
        self.assertEqual(self.snapshot(), first)
        checked = self.run_generator('--check')
        self.assertEqual(checked.returncode, 0, checked.stderr)
        self.assertEqual(self.snapshot(), first)

    def test_check_rejects_malformed_hooks(self):
        self.generate()
        valid = self.source.read_text()
        mutations = [
            ('method', 'MacShim_Dispatch("CDirect3DDevice_Copy",',
             'MacShim_Dispatch("CDirect3DDevice_Other",'),
            ('receiver', '"CDirect3DDevice_Copy", object,',
             '"CDirect3DDevice_Copy", NULL,'),
            ('receiver argument address', '{ &object, &depth }', '{ object, &depth }'),
            ('float argument address', '{ &object, &depth }', '{ &object, depth }'),
            ('argument order', '{ &object, &depth }', '{ &depth, &object }'),
            ('callback argument address', '{ &object, &callback }',
             '{ &object, callback }'),
            ('result type', 'void * macResult = { 0 };', 'int macResult = { 0 };'),
            ('result address', '"CDirect3DDevice_Create", object, macArguments, &macResult,',
             '"CDirect3DDevice_Create", object, macArguments, NULL,'),
            ('result size', '&macResult, sizeof(macResult)', '&macResult, 0'),
            ('result return', 'return macResult;', 'return 0;'),
            ('void result', '"CDirect3DDevice_Clear", object, macArguments, NULL, 0',
             '"CDirect3DDevice_Clear", object, macArguments, &object, sizeof(object)'),
            ('void return', '        return;\n', '        return 0;\n'),
            ('factory receiver', '"Direct3DCreate9", NULL,',
             '"Direct3DCreate9", version,'),
            ('empty arguments', 'macArguments[] = { NULL }', 'macArguments[] = { 0, 0 }'),
            ('guard expression', '#if MAC_METAL_NATIVE\n', '#if MAC_METAL_NATIVE && 0\n'),
            ('missing endif', '#endif\n', ''),
        ]
        for label, before, after in mutations:
            with self.subTest(mutation=label):
                self.assertIn(before, valid)
                self.source.write_text(valid.replace(before, after, 1))
                first = self.snapshot()
                checked = self.run_generator('--check')
                self.assertNotEqual(checked.returncode, 0, checked.stdout)
                self.assertIn('malformed native dispatch:', checked.stderr)
                self.assertEqual(self.snapshot(), first)

    def test_empty_native_block_is_rejected_without_rewriting(self):
        self.generate()
        valid = self.source.read_text()
        start = valid.index('\n#if MAC_METAL_NATIVE')
        end = valid.index('#endif\n', start) + len('#endif\n')
        self.source.write_text(valid[:start] + '\n#if MAC_METAL_NATIVE\n#endif\n' + valid[end:])
        first = self.snapshot()
        for args in [('--check',), ()]:
            with self.subTest(arguments=args):
                checked = self.run_generator(*args)
                self.assertNotEqual(checked.returncode, 0, checked.stdout)
                self.assertIn('CDirect3DDevice_Copy', checked.stderr)
                self.assertEqual(self.snapshot(), first)

if __name__ == '__main__':
    unittest.main()
