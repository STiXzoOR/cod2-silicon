#!/usr/bin/env python3
"""Exercise browser packet handlers with production message and info-string readers."""
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
path = 'src/PC/client_mp/cl_main_pc_mp.c'
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/' + path)
             and 'cod2_macos.dir' in r.get('command', str(r.get('arguments', []))))
old_root = entry['file'].removesuffix('/' + path)
flags = []
skip = False
for arg in entry.get('arguments') or shlex.split(entry['command']):
    if skip:
        skip = False
    elif arg in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif arg not in ('-c', '-MD', '-MMD', '-DNDEBUG', entry['file']):
        flags.append(arg.replace(old_root, str(ROOT)))
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
source = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
(a.output / 'browser_source.h').write_bytes(source)
objects = []
for dependency in ['src/PC/qcommon/msg_mp.c', 'src/PC/universal/q_shared.c',
                   'src/PC/qcommon/cod2x_protocol.c']:
    obj = a.output / (Path(dependency).stem + '.o')
    subprocess.run([*flags, '-c', str(ROOT / dependency), '-o', str(obj)], cwd=ROOT, check=True)
    objects.append(str(obj))
exe = a.output / 'browser_packets'
subprocess.run([*flags, '-I' + str(a.output), '-iquote', str((ROOT / path).parent),
                '-Wl,-dead_strip', str(ROOT / 'tests/online/browser_packets.c'), *objects,
                '-o', str(exe)], cwd=ROOT, check=True)
failed = []
for case in ['lan-source', 'lan-reset', 'announcement-line', 'announcement-short',
             'announcement-limit', 'ping', 'protocol', 'status', 'status-key', 'status-limit']:
    result = subprocess.run([str(exe), case], capture_output=True, text=True,
                            env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (a.output / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}', flush=True)
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
