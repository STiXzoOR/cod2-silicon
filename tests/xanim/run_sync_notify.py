#!/usr/bin/env python3
"""Verify synchronized script notifications through the real DObj time path."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = 'src/PC/xanim/xanim.c'
CASES = ('crossing', 'wrapping', 'ending', 'controls', 'unsynchronized', 'nested', 'rate')
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', help='read production source from a local commit')
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
source = subprocess.check_output(['git', 'show', f'{a.baseline}:{SOURCE}'], cwd=ROOT) if a.baseline else (ROOT / SOURCE).read_bytes()
(out / 'sync_xanim_source.h').write_bytes(source)
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/' + SOURCE))
source_root = entry['file'][:-len('/' + SOURCE)]
command = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in command[:command.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-ffunction-sections', '-fdata-sections',
          '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-Wl,-dead_strip']
exe = out / 'sync_notify'
with (out / 'compile.log').open('w') as log:
    subprocess.run([*flags, str(ROOT / 'tests/xanim/sync_notify.c'), '-o', str(exe)],
                   cwd=ROOT, stdout=log, stderr=log, check=True)
results = []
for case in CASES:
    result = subprocess.run([str(exe), case], capture_output=True, text=True,
                            env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / f'{case}.log').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='', flush=True)
    results.append({'case': case, 'returncode': result.returncode})
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(r['returncode'] for r in results))
