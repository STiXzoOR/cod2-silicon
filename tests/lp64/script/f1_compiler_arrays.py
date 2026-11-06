#!/usr/bin/env python3
"""Run real GSC nested-array/local lifetime churn without game data."""
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
p.add_argument('cases', nargs='*')
a = p.parse_args()
subprocess.run([sys.executable, str(ROOT / 'tests/cod2x/stop_gametype_vm.py'),
                '--compile-db', str(a.compile_db), '--output', str(a.output), '--ubsan'], check=True)
row = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/scr_vm.c') and 'cod2_macos.dir/' in str(r))
cmd = row.get('arguments') or shlex.split(row['command'])
flags = [s for s in cmd[1:cmd.index('-o')] if s not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(a.output.resolve()), '-fsanitize=address,undefined',
          '-fno-sanitize-recover=all', '-ffunction-sections', '-fdata-sections']
obj = a.output / 'f1_compiler_arrays.o'
result = subprocess.run([cmd[0], *flags, '-c', str(ROOT / 'tests/lp64/script/f1_compiler_arrays.c'), '-o', str(obj)], capture_output=True, text=True)
(a.output / 'arrays.compile.log').write_text(result.stdout + result.stderr)
if result.returncode:
    print(result.stderr[-4000:])
    raise SystemExit(result.returncode)
exe = a.output / 'f1_compiler_arrays'
subprocess.run([cmd[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                str(obj), *map(str, sorted(a.output.glob('scr_*.o'))), '-o', str(exe)], check=True)
failed = False
for case in a.cases or ['append-delete', 'nested-append-delete', 'local-branches', 'array-copy', 'indexed-owner', 'implicit-string-owner', 'remove-loop', 'call-cleanup']:
    result = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=20)
    text = result.stdout + result.stderr
    (a.output / (case + '.log')).write_text(text)
    print(case, 'FAIL' if result.returncode else 'PASS')
    if result.returncode:
        print(text[-2500:])
    failed |= bool(result.returncode)
raise SystemExit(failed)
