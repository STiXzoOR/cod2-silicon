#!/usr/bin/env python3
"""Exercise production snapshot transitions and corpse resets with synthetic state."""
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
path = 'src/PC/cgame_mp/cg_snapshot_mp.c'
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
for name in ['snapshot', 'playerstate']:
    source_path = f'src/PC/cgame_mp/cg_{name}_mp.c'
    source = subprocess.check_output(['git', 'show', f'{a.baseline}:{source_path}'], cwd=ROOT, text=True) if a.baseline else (ROOT / source_path).read_text()
    (a.output / (name + '_source.h')).write_text(source)
exe = a.output / 'snapshot'
subprocess.run([*flags, '-UNDEBUG', '-I' + str(a.output),
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                '-ffunction-sections', '-Wl,-dead_strip', str(ROOT / 'tests/camera/snapshot.c'),
                '-o', str(exe)], check=True, cwd=ROOT)
failed = []
for case in ['transition-high', 'transition-low', 'transition-ordinary', 'transition-demo',
             'transition-nopredict', 'transition-synchronous', 'transition-null-dvars',
             'corpse-high', 'corpse-low', 'corpse-ordinary', 'corpse-empty', 'corpse-mismatch',
             'process-normal-due', 'process-normal-future', 'process-normal-before',
             'process-cubemap-due', 'process-cubemap-future', 'process-cubemap-before',
             'process-cubemap-incoming-due', 'process-cubemap-incoming-future',
             'process-normal-incoming-inactive', 'process-normal-incoming-backward',
             'process-normal-incoming-restart',
             'process-cubemap-incoming-replace-restart',
             'process-cubemap-incoming-replace-backward']:
    result = subprocess.run([str(exe), case], capture_output=True, text=True,
                            env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (a.output / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}')
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
