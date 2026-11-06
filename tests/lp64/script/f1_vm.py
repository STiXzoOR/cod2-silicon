#!/usr/bin/env python3
"""Check Mac 1.3 notify matching, packed stacks and VM operand failures."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('cases', nargs='*')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
rows = json.loads(a.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/scr_vm.c') and 'cod2_macos.dir/' in str(r))
cmd = row.get('arguments') or shlex.split(row['command'])
old = row['file'][:-len('/src/PC/script/scr_vm.c')]
flags = [s.replace(old, str(ROOT)) for s in cmd[1:cmd.index('-o')] if s not in ('-c', '-DNDEBUG')]
exe = a.output / 'test'
build = subprocess.run([cmd[0], *flags, '-UNDEBUG', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', '-Wl,-dead_strip',
                        str(ROOT / 'tests/lp64/script/f1_vm.c'), '-o', str(exe)], capture_output=True, text=True)
(a.output / 'compile.log').write_text(build.stdout + build.stderr)
if build.returncode:
    print(build.stderr[-4000:])
    raise SystemExit(build.returncode)
failed = False
for case in a.cases or ['match-short', 'archive-packed', 'append-packed', 'wait-overflow', 'vector-packed', 'unknown-object', 'field-object']:
    run = subprocess.run([str(exe), case], capture_output=True, text=True)
    (a.output / (case + '.log')).write_text(run.stdout + run.stderr)
    print(case, 'PASS' if run.returncode == 0 else 'FAIL')
    if run.returncode:
        print((run.stdout + run.stderr)[-1600:])
        failed = True
raise SystemExit(failed)
