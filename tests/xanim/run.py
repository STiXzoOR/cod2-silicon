#!/usr/bin/env python3
"""Exercise Mac 1.3 animation and skeleton behavior with synthetic data."""
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
p.add_argument('--baseline', help='read production source from a local commit')
p.add_argument('--case', choices=['end', 'root', 'goals', 'time', 'notify', 'skeleton', 'surfaces', 'mapping', 'trace', 'angles', 'names'])
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
for name in ['xanim', 'dobj', 'xmodel']:
    path = f'src/PC/xanim/{name}.c'
    source = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
    (out / f'f1_{name}_source.h').write_bytes(source)
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/src/PC/xanim/xanim.c'))
source_root = entry['file'][:-len('/src/PC/xanim/xanim.c')]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-ffunction-sections', '-fdata-sections',
          '-fsanitize=address,undefined', '-fno-sanitize=shift', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-Wl,-dead_strip']
exe = out / 'xanim_fidelity'
with (out / 'compile.log').open('w') as log:
    subprocess.run([*flags, str(ROOT / 'tests/xanim/fidelity.c'), str(ROOT / 'tests/xanim/skeleton.c'),
                    str(ROOT / 'tests/xanim/model.c'), '-o', str(exe)],
                   cwd=ROOT, stdout=log, stderr=log, check=True)
run = subprocess.run([str(exe), *([a.case] if a.case else [])], capture_output=True, text=True,
                     env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
(out / 'run.log').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
run.check_returncode()
