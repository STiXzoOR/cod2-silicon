#!/usr/bin/env python3
"""Run synthetic production water/draw fixtures with an actual engine database."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def source(path, baseline):
    if baseline:
        return subprocess.check_output(['git', 'show', f'{baseline}:{path}'], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def function(text, name):
    match = re.search(r'^([^\n]*\b' + name + r'\([^;]*?\)\n\{)', text, re.M)
    if not match:
        raise ValueError(name)
    depth = 0
    for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()]
    raise ValueError(name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-commands', type=Path, required=True)
    parser.add_argument('--baseline')
    parser.add_argument('--test', choices=['draw', 'spectrum', 'mode', 'coords', 'vertices'], action='append')
    options = parser.parse_args()
    rows = json.loads(options.compile_commands.read_text())
    entry = next(row for row in rows if row['file'].endswith('/src/PC/gfx_d3d/rb_shade.c'))
    original_root = entry['file'].removesuffix('/src/PC/gfx_d3d/rb_shade.c')
    args = entry.get('arguments') or shlex.split(entry['command'])
    flags = []
    skip = False
    for arg in args:
        if skip:
            skip = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', entry['file'], '-DNDEBUG'):
            flags.append(arg.replace(original_root, str(ROOT)))
    with tempfile.TemporaryDirectory(prefix='ws75-water-') as temp:
        out = Path(temp)
        for path, name in [('src/PC/gfx_d3d/rb_shade.c', 'shade_source.h'),
                           ('src/PC/gfx_d3d/r_water.c', 'water_source.h')]:
            (out / name).write_text(source(path, options.baseline))
        state = source('src/PC/gfx_d3d/rb_state.c', options.baseline)
        utilities = re.search(r'^const byte s_filterTable\[16\] = .*?;', state, re.M)[0] + '\n'
        utilities += '\n'.join(function(state, name) for name in [
            'RB_SetTextureStageStateDx7', 'RB_SetTransformDx7', 'RB_ChangeGenTexCoords',
            'RB_SetTextureDx7', 'RB_SetSamplerStateDx7', 'RB_GetFilterTableValue',
            'RB_GetSamplerFilterModes', 'RB_SetSampler'])
        utilities += '\n' + function(source('src/PC/gfx_d3d/r_image.c', options.baseline),
                                      'R_DownsampleMipMapBilinear')
        utilities += '\n' + function(source('src/Mac/DirectX_9/MacOpenGLUtils.c', options.baseline),
                                      'MacOpenGLUtils_SquareRootLowPrecision')
        for name in ['CDirect3DVertexBuffer_Lock', 'CDirect3DVertexBuffer_Unlock']:
            utilities += '\n' + function(source('src/Mac/DirectX_9/CDirect3DVertexBuffer.c', options.baseline), name)
        for name in ['CColorConverter_ByteSwap32', 'StdConverterARGB_Convert']:
            utilities += '\n' + function(source('src/Mac/DirectX_9/CColorConverter.c', options.baseline), name)
        (out / 'water_helpers.h').write_text(utilities)
        fixture_flags = flags + ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
                                 '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
        objects = []
        for path in ['tests/lp64/renderer/water.c', 'src/PC/universal/fft.c',
                     'src/PC/universal/com_math.c']:
            obj = out / (Path(path).stem + '.o')
            subprocess.run([*fixture_flags, '-c', str(ROOT / path), '-o', str(obj)],
                           cwd=ROOT, check=True)
            objects.append(str(obj))
        binary = out / 'water'
        subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                        '-Wl,-dead_strip', *objects, '-o', str(binary)], cwd=ROOT, check=True)
        failures = []
        for name in options.test or ['draw', 'spectrum', 'mode', 'coords', 'vertices']:
            result = subprocess.run([str(binary), name], env={
                **os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
            if result.returncode:
                failures.append(name)
        if failures:
            raise SystemExit('Failed water fixtures: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
