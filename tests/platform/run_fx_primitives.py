#!/usr/bin/env python3
"""Exercise native FX object creation, update, draw, bolting and destruction."""
import argparse
import contextlib
import json
import shlex
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path)
p.add_argument('--output', type=Path)
a = p.parse_args()

def function(source, name):
    match = re.search(r'^(?:static dvar_t \*|const FxBoltFramePtr |Bool |void )' + name + r'\([^;]*?\)\n\{', source, re.M)
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

with (contextlib.nullcontext(str(a.output.resolve())) if a.output else tempfile.TemporaryDirectory(prefix='ws15-fx-')) as tmp:
    out = Path(tmp)
    out.mkdir(parents=True, exist_ok=True)
    orientation = (root / 'src/PC/universal/q_shared.c').read_text()
    (out / 'fx_orientation_source.h').write_text('\n'.join(function(orientation, name) for name in [
        'OrientationPosToWorldPos', 'OrientationDirToWorldDir', 'OrientationPosFromWorldPos', 'OrientationDirFromWorldDir']))
    scheduler = (root / 'src/PC/EffectsCore/FxScheduler.c').read_text()
    native_scheduler = subprocess.run(['unifdef', '-DCOD2_X64=1'], input=scheduler,
                                      capture_output=True, text=True)
    if native_scheduler.returncode not in (0, 1):
        native_scheduler.check_returncode()
    scheduler = native_scheduler.stdout
    (out / 'fx_creation_source.h').write_text('\n'.join(function(scheduler, name) for name in ['FX_GetBoltingFrame', 'FxScheduler_CreateEffect']))
    (out / 'fx_play_source.h').write_text(
        function(scheduler, 'FxScheduler_GetDvar') + '\n' +
        '#define FxScheduler_PlayEffect FxScheduler_PlayEffectUnderTest\n' +
        function(scheduler, 'FxScheduler_PlayEffect') + '\n#undef FxScheduler_PlayEffect\n')
    sources = ['tests/platform/macos_fx_primitives.c',
               'src/PC/EffectsCore/FxPrimitives.c', 'src/PC/EffectsCore/FxUtil.c',
               'src/PC/EffectsCore/FxCurve.c', 'src/PC/qcommon/q_math.c',
               'src/PC/universal/com_math.c']
    rows = json.loads(a.compile_db.read_text()) if a.compile_db else []
    objects = []
    for source in sources:
        if rows:
            engine_source = source if source.startswith('src/') else 'src/PC/EffectsCore/FxPrimitives.c'
            row = next(r for r in rows if r['file'].endswith('/' + engine_source) and
                       'cod2_macos.dir' in (r.get('command') or ' '.join(r['arguments'])))
            source_root = row['file'][:-len('/' + engine_source)]
            command = row.get('arguments') or shlex.split(row['command'])
            flags = [f.replace(source_root, str(root)) for f in command[:command.index('-o')] if f != '-DNDEBUG']
        else:
            flags = ['clang', '-arch', 'arm64', '-std=gnu99', '-g', '-O0', '-fcommon',
                     '-ffp-contract=off', '-DCOD2_X64=1', '-Wno-typedef-redefinition',
                     '-Wno-duplicate-decl-specifier', '-Isrc', '-Isrc/headers']
        flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I' + tmp]
        obj = out / (Path(source).stem + '.o')
        with (out / (Path(source).stem + '.compile.log')).open('w') as log:
            subprocess.run([*flags, '-c', str(root / source), '-o', str(obj)], cwd=root,
                           stdout=log, stderr=log, check=True)
        objects.append(str(obj))
    subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                    '-Wl,-dead_strip', *objects, '-o', str(out / 'fx_primitives')], check=True)
    subprocess.run([str(out / 'fx_primitives')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
