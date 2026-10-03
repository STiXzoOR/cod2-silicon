#!/usr/bin/env python3
"""Exercise the native cloud buffers and indexed draw packet without GL."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(path, name):
    source = (root / path).read_text()
    match = re.search(r'^(?:static )?void ' + name + r'\([^;]*?\)\n\{', source, re.M)
    assert match, name
    depth = 0
    for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]', source[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return source[match.start():match.start() + token.end()]
    raise ValueError(name)


with tempfile.TemporaryDirectory(prefix='ws15-fx-cloud-') as tmp:
    out = Path(tmp)
    init = (root / 'src/PC/gfx_d3d/r_init.c').read_text()
    guarded = re.search(r'(?ms)^#if defined\(COD2_X64\)\nstatic vec2_t cornerTexCoords.*?^#endif', init)
    constants = guarded[0] if guarded else re.search(
        r'(?ms)^static vec2_t cornerTexCoords.*?^static const r_index_t quadIndices[^;]*;', init)[0]
    (out / 'fx_cloud_source.h').write_text(constants + '\n' + '\n'.join(function(path, name) for path, name in [
        ('src/PC/gfx_d3d/r_init.c', 'R_CreateParticleCloudBuffer'),
        ('src/PC/gfx_d3d/rb_shade.c', 'RB_DrawIndexedPrim'),
        ('src/PC/gfx_d3d/rb_tess.c', 'RB_TessParticleCloud'),
    ]))
    subprocess.run([
        'clang', '-arch', 'arm64', '-std=gnu99', '-g', '-O0', '-ffp-contract=off',
        '-DCOD2_X64=1', '-fsanitize=address,undefined',
        '-Wno-typedef-redefinition', '-Wno-duplicate-decl-specifier',
        '-Isrc', '-Isrc/headers', '-I' + tmp, 'tests/platform/macos_fx_cloud.c',
        'src/PC/universal/com_math.c', '-Wl,-dead_strip', '-o', str(out / 'fx_cloud')
    ], cwd=root, check=True)
    subprocess.run([str(out / 'fx_cloud')], check=True, env={
        **os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'
    })
