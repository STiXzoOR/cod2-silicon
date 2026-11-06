#!/usr/bin/env python3
"""Exercise production renderer utility behavior with native layouts and recording callbacks."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/src/PC/gfx_d3d/r_utils.c'))
old_root = entry['file'].removesuffix('/src/PC/gfx_d3d/r_utils.c')
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
path = 'src/PC/gfx_d3d/r_utils.c'
source = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT, text=True) if a.baseline else (ROOT / path).read_text()
(a.output / 'utils_source.h').write_text(source)
exe = a.output / 'utils'
subprocess.run([*flags, '-UNDEBUG', '-I' + str(a.output), '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-ffunction-sections', '-Wl,-dead_strip', str(ROOT / 'tests/lp64/renderer/utils.c'), '-o', str(exe)], check=True, cwd=ROOT)
failed = []
for case in ['dispatch', 'default', 'startsolid', 'fresnel']:
    result = subprocess.run([str(exe), case], capture_output=True, text=True, env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (a.output / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}')
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
