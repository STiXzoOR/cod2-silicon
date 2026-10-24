#!/usr/bin/env python3
"""Run the real animation/skeleton/skinning chain with synthetic bones only."""
import argparse
import json
import os
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
for name, path in [('xanim', 'src/PC/xanim/xanim.c'),
                   ('dobj', 'src/PC/xanim/dobj.c'),
                   ('model', 'src/PC/gfx_d3d/r_model.c')]:
    text = subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=ROOT, text=True) if args.baseline else (ROOT / path).read_text()
    (output / f'player_{name}_source.h').write_text(text)

entry = next(row for row in json.loads(args.compile_db.read_text())
             if row['file'].endswith('/src/PC/xanim/xanim.c'))
source_root = entry['file'][:-len('/src/PC/xanim/xanim.c')]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [flag.replace(source_root, str(ROOT)) for flag in flags[:flags.index('-o')]
         if flag != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(output), '-ffunction-sections', '-fdata-sections',
          '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wl,-dead_strip']
exe = output / 'player_skeleton'
with (output / 'compile.log').open('w') as log:
    subprocess.run([*flags, '-x', 'c', str(output / 'player_xanim_source.h'),
                    str(ROOT / 'src/PC/xanim/xmodel.c'),
                    str(ROOT / 'tests/lp64/renderer/player_skeleton.c'),
                    '-o', str(exe)], cwd=ROOT, stdout=log, stderr=log, check=True)
run = subprocess.run([str(exe)], capture_output=True, text=True, env={**os.environ,
                     'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
(output / 'run.log').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
run.check_returncode()
