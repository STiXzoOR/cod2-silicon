#!/usr/bin/env python3
"""Exercise the production damage and trigger paths using rebased engine compiler flags."""
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
CASES = ['pain', 'death', 'notify-kills', 'notify-revives', 'notify-clamps',
         'use8', 'use256', 'filters', 'thresholds']
p.add_argument('--case', action='append', choices=CASES)
p.add_argument('--baseline', help='Read the production combat source from this local revision')
a = p.parse_args()
os.environ['UBSAN_OPTIONS'] = 'halt_on_error=1'
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
source = 'src/PC/game_mp/g_combat_mp.c'
rows = json.loads(a.compile_db.read_text())
entry = next(row for row in rows if row['file'].endswith('/' + source) and
             'cod2_macos.dir' in (row.get('command') or ' '.join(row['arguments'])))
source_root = entry['file'][:-len('/' + source)]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-ffunction-sections', '-fdata-sections',
          '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
objects = []
for path in ['tests/lp64/game/f1_damage.c', source, 'src/PC/game_mp/g_trigger_mp.c', 'src/PC/universal/com_math.c']:
    obj = out / (Path(path).stem + '.o')
    input_path = ROOT / path
    if path == source:
        input_path = out / 'g_combat_mp.c'
        input_path.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT)
                               if a.baseline else (ROOT / source).read_bytes())
    with (out / (Path(path).stem + '.compile.log')).open('w') as log:
        subprocess.run([*flags, '-c', str(input_path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'f1_damage'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
failed = []
for case in a.case or CASES:
    with (out / (case + '.log')).open('w') as log:
        result = subprocess.run([str(exe), case], cwd=ROOT, stdout=log, stderr=log)
    print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed damage cases: ' + ', '.join(failed))
