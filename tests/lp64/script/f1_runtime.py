#!/usr/bin/env python3
"""Run recoverable GSC errors through the full production compiler and VM."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--vm-source', type=Path)
p.add_argument('cases', nargs='*')
a = p.parse_args()
# This existing harness generates host adapters and builds every script TU.
subprocess.run([sys.executable, str(ROOT / 'tests/cod2x/stop_gametype_vm.py'),
                '--compile-db', str(a.compile_db), '--output', str(a.output), '--ubsan',
                *(['--vm-source', str(a.vm_source)] if a.vm_source else [])], check=True)
row = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/scr_vm.c') and 'cod2_macos.dir/' in str(r))
cmd = row.get('arguments') or shlex.split(row['command'])
flags = [s for s in cmd[1:cmd.index('-o')] if s not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(a.output.resolve()), '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-ffunction-sections', '-fdata-sections']
obj = a.output / 'f1_runtime.o'
result = subprocess.run([cmd[0], *flags, '-c', str(ROOT / 'tests/lp64/script/f1_runtime.c'), '-o', str(obj)], capture_output=True, text=True)
(a.output / 'f1.compile.log').write_text(result.stdout + result.stderr)
if result.returncode:
    print(result.stderr[-4000:]); raise SystemExit(result.returncode)
objects = sorted(a.output.glob('scr_*.o'))
exe = a.output / 'f1_runtime'
subprocess.run([cmd[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip', str(obj), *map(str, objects), '-o', str(exe)], check=True)
failed = False
for case in a.cases or ['sentinel', 'self-endon', 'switch-packed', 'generic-error', 'too-many-params', 'negative-wait', 'inc-type', 'waittill-type', 'notify-type', 'endon-type', 'switch-range', 'external-overflow', 'function-pointer-type', 'notify-wait', 'match-wait', 'thread-churn', 'endon-churn', 'call-churn', 'nested-wait-churn', 'nested-thread-churn']:
    try:
        result = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=10)
        text = result.stdout + result.stderr
        failed |= bool(result.returncode)
        print(case, 'FAIL' if result.returncode else 'PASS')
    except subprocess.TimeoutExpired as e:
        text = str(e); failed = True; print(case, 'TIMEOUT')
    (a.output / (case + '.log')).write_text(text)
    if failed: print(text[-2000:])
raise SystemExit(failed)
