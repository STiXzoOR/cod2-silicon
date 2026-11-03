#!/usr/bin/env python3
"""Check complete production item and math TUs under sanitizers."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CASES = ['health-quantity', 'health-overheal', 'ammo-offhand', 'weapon-offhand',
         'pool-on', 'pool-off', 'clip-field', 'clip-cap', 'no-slot-select',
         'random-clamp', 'bit31', 'denied-bit31', 'drop-fields', 'drop-failure', 'ammo-controls']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', help='Read the production item source from this local revision')
p.add_argument('--case', action='append', choices=CASES)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
rows = json.loads(a.compile_db.read_text())
item = 'src/PC/game_mp/g_items_mp.c'


def flags_for(source):
    entry = next(row for row in rows if row['file'].endswith('/' + source))
    command = entry.get('arguments') or shlex.split(entry['command'])
    old_root = entry['file'][:-len('/' + source)]
    flags = [arg.replace(old_root, str(ROOT)) for arg in command[:command.index('-o')]
             if arg not in ('-c', '-DNDEBUG')]
    return flags + ['-UNDEBUG', '-ffunction-sections', '-fdata-sections',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer']


item_source = out / 'g_items_mp.c'
item_source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + item], cwd=ROOT)
                          if a.baseline else (ROOT / item).read_bytes())
objects = []
sources = [(item_source, item), (ROOT / 'tests/lp64/game/item_pickup.c', item)]
for name in ['com_math']:
    path = 'src/PC/universal/' + name + '.c'
    sources.append((ROOT / path, path))
for path, source in sources:
    obj = out / (path.stem + '.o')
    with (out / (path.stem + '.compile.log')).open('w') as log:
        subprocess.run([*flags_for(source), '-c', str(path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'item_pickup'
subprocess.run([flags_for(item)[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
failed = []
for case in a.case or CASES:
    with (out / (case + '.log')).open('w') as log:
        result = subprocess.run([str(exe), case], stdout=log, stderr=log, cwd=out)
    print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed item-pickup cases: ' + ', '.join(failed))
