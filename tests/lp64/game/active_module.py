#!/usr/bin/env python3
"""Exercise production active-client and math translation units under sanitizers."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CASES = ['touch-item', 'touch-controls', 'stuck-inuse', 'stuck-maxclients',
         'stuck-flags', 'stuck-speed', 'stuck-controls', 'damage-yaw',
         'damage-pitch', 'damage-controls']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', help='Read active-client production source from this local revision')
p.add_argument('--case', action='append', choices=CASES)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
rows = json.loads(a.compile_db.read_text())
active = 'src/PC/game_mp/g_active_mp.c'
math = 'src/PC/universal/com_math.c'


def flags_for(source):
    entry = next(row for row in rows if row['file'].endswith('/' + source))
    command = entry.get('arguments') or shlex.split(entry['command'])
    old_root = entry['file'][:-len('/' + source)]
    flags = [arg.replace(old_root, str(ROOT)) for arg in command[:command.index('-o')]
             if arg not in ('-c', '-DNDEBUG')]
    return flags + ['-UNDEBUG', '-I' + str(out), '-ffunction-sections', '-fdata-sections',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer']


active_source = out / 'g_active_mp.c'
active_source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + active], cwd=ROOT)
                          if a.baseline else (ROOT / active).read_bytes())
objects = []
for path, source in [(active_source, active), (ROOT / math, math),
                     (ROOT / 'tests/lp64/game/active_module.c', active)]:
    obj = out / (path.stem + '.o')
    with (out / (path.stem + '.compile.log')).open('w') as log:
        subprocess.run([*flags_for(source), '-c', str(path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'active_module'
subprocess.run([flags_for(active)[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
failed = []
for case in a.case or CASES:
    with (out / (case + '.log')).open('w') as log:
        result = subprocess.run([str(exe), case], stdout=log, stderr=log, cwd=out)
    print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed active-module cases: ' + ', '.join(failed))
