#!/usr/bin/env python3
"""Exercise the production corpse landing path using rebased engine compiler flags."""
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
a = p.parse_args()
os.environ['UBSAN_OPTIONS'] = 'halt_on_error=1'
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
source = 'src/PC/game_mp/g_items_mp.c'
rows = json.loads(a.compile_db.read_text())
entry = next(row for row in rows if row['file'].endswith('/' + source) and
             'cod2_macos.dir' in (row.get('command') or ' '.join(row['arguments'])))
source_root = entry['file'][:-len('/' + source)]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-ffunction-sections', '-fdata-sections',
          '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
objects = []
for path in ['tests/lp64/game/f1_items.c', source, 'src/PC/universal/com_math.c']:
    obj = out / (Path(path).stem + '.o')
    with (out / (Path(path).stem + '.compile.log')).open('w') as log:
        subprocess.run([*flags, '-c', str(ROOT / path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'f1_items'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
subprocess.run([str(exe)], cwd=ROOT, check=True)
