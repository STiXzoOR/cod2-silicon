#!/usr/bin/env python3
"""Exercise production weapon/mantle state transitions with synthetic services."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline')
a = p.parse_args()
out = a.build.resolve() / ('weapon-mantle-baseline' if a.baseline else 'weapon-mantle')
out.mkdir(parents=True, exist_ok=True)
rows = json.loads((a.build / 'compile_commands.engine.json').read_text())
entry = next(r for r in rows if r['file'].endswith('/src/PC/bgame/bg_weapons.c') and 'cod2_macos.dir' in r['command'])
flags = shlex.split(entry['command'])
flags = [f for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
          '-I' + str(out), '-iquote', str(ROOT / 'src/PC/bgame')]
for name in ['weapons', 'mantle']:
    path = 'src/PC/bgame/bg_' + name + '.c'
    data = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
    (out / (name + '_source.h')).write_bytes(data)
subprocess.run([*flags, str(ROOT / 'tests/movement/weapons.c'), str(ROOT / 'tests/movement/weapon_mantle.c'), '-Wl,-dead_strip', '-o', str(out / 'weapons')], cwd=entry['directory'], check=True)
subprocess.run([str(out / 'weapons')], check=True)
