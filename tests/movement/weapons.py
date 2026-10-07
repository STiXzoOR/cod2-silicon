#!/usr/bin/env python3
"""Exercise production weapon/mantle state transitions with synthetic services."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from policy_build import compile_policy, function

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--baseline')
p.add_argument('--compile-db', type=Path)
p.add_argument('--output', type=Path)
p.add_argument('--revision-matrix', action='store_true')
a = p.parse_args()
out = a.output.resolve() if a.output else a.build.resolve() / ('weapon-mantle-baseline' if a.baseline else 'weapon-mantle')
out.mkdir(parents=True, exist_ok=True)
rows = json.loads((a.compile_db or a.build / 'compile_commands.engine.json').read_text())
entry = next(r for r in rows if r['file'].endswith('/src/PC/bgame/bg_weapons.c') and 'cod2_macos.dir' in r['command'])
source_root = entry['file'][:-len('/src/PC/bgame/bg_weapons.c')]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
          '-I' + str(out), '-iquote', str(ROOT / 'src/PC/bgame')]
for name in ['weapons', 'mantle']:
    path = 'src/PC/bgame/bg_' + name + '.c'
    data = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
    (out / (name + '_source.h')).write_bytes(data)
objects = []
if a.revision_matrix:
    flags += ['-DGAMEPLAY_MATRIX=1']
    misc = (ROOT / 'src/PC/bgame/bg_misc.c').read_text()
    pmove = (ROOT / 'src/PC/bgame/bg_pmove.c').read_text()
    event = (ROOT / 'src/PC/cgame_mp/cg_event_mp.c').read_text()
    table = next(line for line in misc.splitlines() if line.startswith('const int singleClientEvents[7] ='))
    (out / 'weapon_events_source.h').write_text(table + '\n' +
        function(misc, 'BG_AddPredictableEventToPlayerstate') +
        function(pmove, 'PM_AddEvent') + function(pmove, 'PM_GetEffectiveStance') +
        function(misc, 'BG_PlayerStateToEntityState') + function(event, 'CG_CheckEvents'))
    objects = compile_policy(ROOT, out, flags)
    math = out / 'com_math.o'
    subprocess.run([*flags, '-c', str(ROOT / 'src/PC/universal/com_math.c'), '-o', str(math)], cwd=ROOT, check=True)
    objects.append(math)
subprocess.run([*flags, str(ROOT / 'tests/movement/weapons.c'), str(ROOT / 'tests/movement/weapon_mantle.c'), *map(str, objects), '-Wl,-dead_strip', '-o', str(out / 'weapons')], cwd=ROOT, check=True)
for revision in ([0, 3, 4, 6, -1] if a.revision_matrix else [None]):
    subprocess.run([str(out / 'weapons'), *([] if revision is None else [str(revision)])], check=True)
