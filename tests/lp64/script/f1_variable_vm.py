#!/usr/bin/env python3
"""Reuse built full-script objects for nested-array compiler/VM regressions."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--fixture-dir', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
rows = json.loads(a.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/scr_variable.c') and 'cod2_macos.dir/' in str(r))
argv = row.get('arguments') or shlex.split(row['command'])
old_root = row['file'][:-len('/src/PC/script/scr_variable.c')]
flags = [s.replace(old_root, str(ROOT)) for s in argv[1:argv.index('-o')]
         if s not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(a.fixture_dir.resolve()),
          '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-ffunction-sections', '-fdata-sections']
objects = [f for f in sorted(a.fixture_dir.glob('scr_*.o')) if f.name != 'scr_variable.o']
for source in ['tests/lp64/script/f1_variable_vm.c', 'src/PC/script/scr_variable.c']:
    obj = a.output / (Path(source).stem + '.o')
    result = subprocess.run([argv[0], *flags, '-c', str(ROOT / source), '-o', str(obj)],
                            text=True, capture_output=True)
    (a.output / (obj.stem + '.compile.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stderr); raise SystemExit(result.returncode)
    objects.append(obj)
binary = a.output / 'test'
subprocess.run([argv[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                *map(str, objects), '-o', str(binary)], check=True)
result = subprocess.run([str(binary), 'nested_append'], text=True, capture_output=True, timeout=30)
(a.output / 'nested_append.log').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr, end='')
raise SystemExit(result.returncode)
