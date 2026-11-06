#!/usr/bin/env python3
"""Exercise production player prediction with synthetic movement and ADS inputs."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
a = p.parse_args()
a.output = a.output.resolve()
if a.output == ROOT or ROOT in a.output.parents:
    p.error('--output must be outside the checkout')
a.output.mkdir(parents=True, exist_ok=True)
path = 'src/PC/cgame_mp/cg_predict_mp.c'
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/' + path))
old_root = entry['file'].removesuffix('/' + path)
args = entry.get('arguments') or shlex.split(entry['command'])
flags = []
skip = False
for arg in args:
    if skip:
        skip = False
    elif arg in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif arg not in ('-c', '-MD', '-MMD', '-DNDEBUG', entry['file']):
        flags.append(arg.replace(old_root, str(ROOT)))
source = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT, text=True) if a.baseline else (ROOT / path).read_text()
(a.output / 'prediction_source.h').write_text(source)
exe = a.output / 'prediction'
# Pattern initialization exposes the baseline's uninitialized moved flag on the
# no-command path; this affects fixture compilation only, never engine builds.
subprocess.run([*flags, '-UNDEBUG', '-I' + str(a.output), '-ftrivial-auto-var-init=pattern',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                '-ffunction-sections', '-Wl,-dead_strip', str(ROOT / 'tests/camera/prediction.c'),
                '-o', str(exe)], check=True, cwd=ROOT)
failed = []
for case in ['mover', 'no-mover', 'no-command', 'acknowledged-ahead', 'missing-command', 'zoom',
             'ads-reset', 'ads-reentry', 'ads-flag', 'ads-zero-max']:
    result = subprocess.run([str(exe), case], capture_output=True, text=True,
                            env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (a.output / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}')
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
