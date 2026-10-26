#!/usr/bin/env python3
"""Exercise production GL depth-surface transitions with synthetic pixels."""
import argparse
import json
import os
import re
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='read production source from a local commit')
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
source_dir = ROOT / 'src/Mac/DirectX_9'
for name in ['CDirect3DDevice.c', 'lp64_shader_draw.h']:
    path = 'src/Mac/DirectX_9/' + name
    source = subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=ROOT) if args.baseline else (ROOT / path).read_bytes()
    if name == 'CDirect3DDevice.c':
        # Direct calls exercise the unchanged production bodies, including Init.
        # The full exported dispatch table would retain every unrelated device method.
        source, count = re.subn(rb'^fnptr_t vtbl_CDirect3DDevice\[\] = .*?;\n',
                               b'fnptr_t vtbl_CDirect3DDevice[121];\n', source, flags=re.M)
        assert count == 1
    (output / name).write_bytes(source)
entry = next(row for row in json.loads(args.compile_db.read_text())
             if row['file'].endswith('/src/Mac/DirectX_9/CDirect3DDevice.c'))
source_root = entry['file'][:-len('/src/Mac/DirectX_9/CDirect3DDevice.c')]
command = entry.get('arguments') or shlex.split(entry['command'])
flags = [flag.replace(source_root, str(ROOT)) for flag in command[:command.index('-o')]
         if flag != '-DNDEBUG']
exe = output / 'depth_surface'
command = [*flags, '-UNDEBUG', '-DGL_SILENCE_DEPRECATION', '-I' + str(output),
           '-I' + str(source_dir), '-ffunction-sections', '-fdata-sections',
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wl,-dead_strip',
           '-framework', 'OpenGL', str(ROOT / 'tests/lp64/renderer/depth_surface.c'), '-o', str(exe)]
(output / 'compile-command.txt').write_text(shlex.join(command) + '\n')
with (output / 'compile.log').open('w') as log:
    subprocess.run(command, cwd=ROOT, stdout=log, stderr=log, check=True)
failed = []
for mode in ['arb', 'fallback']:
    for case in ['initial', 'detach', 'detached-draw', 'reattach', 'disabled', 'changed-detached', 'stencil-detached', 'clear-detached', 'clear-attached']:
        env = {**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1',
               'COD2_MAC_SHADER_CACHE': 'synthetic', 'D3D_PROG': '1' if mode == 'arb' else '0'}
        env.pop('COD2_MAC_SHADER_DIAGNOSTICS', None)
        run = subprocess.run([str(exe), mode, case], capture_output=True, text=True, env=env)
        name = mode + '-' + case
        (output / (name + '.log')).write_text(run.stdout + run.stderr)
        print(f'{name}: {run.returncode}', flush=True)
        if run.returncode:
            failed.append(name)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
