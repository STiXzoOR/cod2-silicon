#!/usr/bin/env python3
"""Exercise native FX object creation, update, draw, bolting and destruction."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]

def function(source, name):
    match = re.search(r'^(?:const FxBoltFramePtr |Bool |void )' + name + r'\([^;]*?\)\n\{', source, re.M)
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

with tempfile.TemporaryDirectory(prefix='ws15-fx-') as tmp:
    out = Path(tmp)
    orientation = (root / 'src/PC/universal/q_shared.c').read_text()
    (out / 'fx_orientation_source.h').write_text('\n'.join(function(orientation, name) for name in [
        'OrientationPosToWorldPos', 'OrientationDirToWorldDir', 'OrientationPosFromWorldPos', 'OrientationDirFromWorldDir']))
    scheduler = (root / 'src/PC/EffectsCore/FxScheduler.c').read_text()
    (out / 'fx_creation_source.h').write_text('\n'.join(function(scheduler, name) for name in ['FX_GetBoltingFrame', 'FxScheduler_CreateEffect']))
    command = ['clang', '-arch', 'arm64', '-std=gnu99', '-g', '-O0', '-fcommon', '-ffp-contract=off',
               '-DCOD2_X64=1', '-fsanitize=address,undefined', '-Wl,-dead_strip',
               '-Wno-typedef-redefinition', '-Wno-duplicate-decl-specifier',
               '-Isrc', '-Isrc/headers', '-I' + tmp,
               'tests/platform/macos_fx_primitives.c',
               'src/PC/EffectsCore/FxPrimitives.c', 'src/PC/EffectsCore/FxUtil.c',
               'src/PC/EffectsCore/FxCurve.c', 'src/PC/qcommon/q_math.c',
               'src/PC/universal/com_math.c', '-o', str(out / 'fx_primitives')]
    subprocess.run(command, cwd=root, check=True)
    subprocess.run([str(out / 'fx_primitives')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
