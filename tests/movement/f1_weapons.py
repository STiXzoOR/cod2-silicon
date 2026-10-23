#!/usr/bin/env python3
"""Exercise Mac 1.3 weapon state, field loading and pure angle expectations."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from policy_build import function

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--baseline')
p.add_argument('--compile-db', type=Path)
p.add_argument('--output', type=Path)
a = p.parse_args()
out = (a.output or a.build / ('f1-weapons-baseline' if a.baseline else 'f1-weapons')).resolve()
out.mkdir(parents=True, exist_ok=True)
rows = json.loads((a.compile_db or a.build / 'compile_commands.engine.json').read_text())
entry = next(r for r in rows if r['file'].endswith('/src/PC/bgame/bg_weapons.c') and 'cod2_macos.dir' in r['command'])
source_root = entry['file'][:-len('/src/PC/bgame/bg_weapons.c')]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
          '-I' + str(out), '-iquote', str(ROOT / 'src/PC/bgame')]
if a.baseline:
    flags += ['-DF1_BASELINE=1']
for name in ['weapons', 'mantle']:
    path = 'src/PC/bgame/bg_' + name + '.c'
    data = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
    (out / (name + '_source.h')).write_bytes(data)
path = 'src/PC/bgame/bg_weapons_load_obj.c'
load_source = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT, text=True) if a.baseline else (ROOT / path).read_text()
load_source = subprocess.run(['unifdef', '-DCOD2_X64=1'], input=load_source, text=True, stdout=subprocess.PIPE, check=False).stdout
(out / 'f1_load_source.h').write_text(function(load_source, 'BG_LoadWeaponDefInternal'))
subprocess.run([*flags, str(ROOT / 'tests/movement/f1_weapons.c'), str(ROOT / 'tests/movement/weapon_mantle.c'),
                '-Wl,-dead_strip', '-o', str(out / 'f1-weapons')], cwd=ROOT, check=True)
subprocess.run([str(out / 'f1-weapons')], check=True)
