#!/usr/bin/env python3
"""Run repeated failed builtins through the production compiler and VM."""
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
p.add_argument('--reuse-objects', action='store_true')
p.add_argument('--vm-source', type=Path, help='Private baseline source for a red regression check')
p.add_argument('cases', nargs='*')
a = p.parse_args()
if not a.reuse_objects:
    subprocess.run([sys.executable, str(ROOT / 'tests/cod2x/stop_gametype_vm.py'),
                    '--compile-db', str(a.compile_db), '--output', str(a.output), '--ubsan',
                    *(['--vm-source', str(a.vm_source)] if a.vm_source else [])], check=True)
row = next(r for r in json.loads(a.compile_db.read_text())
           if r['file'].endswith('/scr_vm.c') and 'cod2_macos.dir/' in str(r))
cmd = row.get('arguments') or shlex.split(row['command'])
old_root = row['file'][:-len('/src/PC/script/scr_vm.c')]
flags = [s.replace(old_root, str(ROOT)) for s in cmd[1:cmd.index('-o')]
         if s not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(a.output.resolve()), '-fsanitize=address,undefined',
          '-fno-sanitize-recover=all', '-ffunction-sections', '-fdata-sections',
          '-iquote', str(ROOT / 'src/PC/script')]
sources = [('tests/lp64/script/f1_error_recovery.c', 'f1_error_recovery'),
           ('src/PC/game_mp/g_spawn_mp.c', 'g_spawn_mp')]
if a.reuse_objects:
    sources.insert(0, ('src/PC/script/scr_vm.c', 'scr_vm'))
for source, name in sources:
    source_path = a.vm_source.resolve() if name == 'scr_vm' and a.vm_source else ROOT / source
    result = subprocess.run([cmd[0], *flags, '-c', str(source_path),
                             '-o', str(a.output / (name + '.o'))], capture_output=True, text=True)
    (a.output / (name + '.compile.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stderr[-4000:]); raise SystemExit(result.returncode)
exe = a.output / 'f1_error_recovery'
subprocess.run([cmd[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                str(a.output / 'f1_error_recovery.o'), *map(str, sorted(a.output.glob('scr_*.o'))),
                str(a.output / 'g_spawn_mp.o'),
                '-o', str(exe)], check=True)
failed = False
for case in a.cases or ['builtin', 'builtin-many', 'method', 'entity-method', 'entity-method-many', 'nested', 'partial', 'partial-method', 'partial-nested', 'field-generic', 'field-self', 'field-compound', 'field-self-compound']:
    try:
        run = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=20)
        output = run.stdout + run.stderr
        failed |= bool(run.returncode)
        print(case, 'FAIL' if run.returncode else 'PASS')
    except subprocess.TimeoutExpired as error:
        output = str(error); failed = True; print(case, 'TIMEOUT')
    (a.output / (case + '.log')).write_text(output)
    if failed: print(output[-2000:])
raise SystemExit(failed)
