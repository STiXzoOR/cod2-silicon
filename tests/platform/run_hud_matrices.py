#!/usr/bin/env python3
"""Check actual RB_Set2D matrix copies with ASan/UBSan, without GL."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(path, name):
    source = (root / path).read_text()
    match = re.search(r'^(?:static )?(?:void|int) ' + name + r'\([^;]*?\)\n\{', source, re.M)
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


with tempfile.TemporaryDirectory(prefix='ws15-hud-matrix-') as tmp:
    out = Path(tmp)
    functions = [
        ('src/PC/universal/com_math.c', name)
        for name in ['MatrixIdentity44', 'MatrixMultiply44', 'MatrixTranspose44']
    ] + [
        ('src/Mac/DirectX_9/MacOpenGLUtils.c', 'MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL'),
        ('src/PC/gfx_d3d/rb_backend.c', 'RB_Set2D'),
    ]
    (out / 'hud_matrix_source.h').write_text('\n'.join(function(path, name) for path, name in functions))
    subprocess.run([
        'clang', '-arch', 'arm64', '-std=gnu99', '-g', '-O0', '-ffp-contract=off',
        '-DCOD2_X64=1', '-fsanitize=address,undefined',
        '-Wno-typedef-redefinition', '-Wno-duplicate-decl-specifier', '-Wno-ignored-attributes',
        '-Isrc', '-Isrc/headers', '-I' + tmp, 'tests/platform/macos_hud_matrices.c',
        '-o', str(out / 'hud_matrices')
    ], cwd=root, check=True)
    subprocess.run([str(out / 'hud_matrices')], check=True, env={
        **os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'
    })
