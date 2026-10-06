#!/usr/bin/env python3
"""Test production movement with synthetic services and Mac 1.3 expectations."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--baseline')
p.add_argument('--test', action='append')
a = p.parse_args()
out = a.build.resolve() / 'movement-tests'
out.mkdir(parents=True, exist_ok=True)
source = 'src/PC/bgame/bg_pmove.c'
data = subprocess.check_output(['git', 'show', f'{a.baseline}:{source}'], cwd=ROOT, text=True) if a.baseline else (ROOT / source).read_text()
(out / 'pmove_source.h').write_text(data)
viewpath = 'src/PC/cgame_mp/cg_view_mp.c'
view = subprocess.check_output(['git', 'show', f'{a.baseline}:{viewpath}'], cwd=ROOT, text=True) if a.baseline else (ROOT / viewpath).read_text()
match = re.search(r'        (float bobMax = [^;]+;)', view)
(out / 'bob_max_source.h').write_text(match[1] + '\n')
defaults = {'friction': '5.5f', 'stopspeed': '100.0f', 'inertiaMax': '10000.0f',
            'player_footstepsThreshhold': '190.0f', 'player_moveThreshhold': '10.0f',
            'player_strafeSpeedScale': '0.8f', 'player_backSpeedScale': '0.7f',
            'player_spectateSpeedScale': '2.0f', 'player_dmgtimer_maxTime': '750.0f',
            'bg_fallDamageMinHeight': '128.0f', 'bg_fallDamageMaxHeight': '300.0f'}
ints = {'player_dmgtimer_stumbleTime': 500, 'player_dmgtimer_flinchTime': 500}
names = set(re.findall(r'extern const dvar_t \*(\w+);', data))
globals_ = []
for n in sorted(names):
    member, value = ('integer', str(ints[n])) if n in ints else ('value', defaults.get(n, '0.0f'))
    globals_.append(f'static dvar_t fixture_{n} = {{ .current.{member} = {value} }};\nconst dvar_t *{n} = &fixture_{n};\n')
(out / 'dvars.h').write_text(''.join(globals_))
rows = json.loads((a.build / 'compile_commands.json').read_text())
entry = next(x for x in rows if x['file'].endswith('/' + source))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [x for x in flags if x != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-ffunction-sections', '-fdata-sections',
          '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
obj = out / 'movement.o'
subprocess.run([*flags, '-c', str(ROOT / 'tests/movement/movement.c'), '-o', str(obj)], check=True)
objects = [obj]
for source in ['com_math', 'q_shared']:
    extra = out / (source + '.o')
    subprocess.run([*flags, '-c', str(ROOT / ('src/PC/universal/' + source + '.c')), '-o', str(extra)], check=True)
    objects.append(extra)
exe = out / 'movement'
subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip', *map(str, objects), '-o', str(exe)], check=True)
for name in a.test or ['bob', 'scale', 'walk', 'lean', 'timers', 'direction', 'view']:
    subprocess.run([str(exe), name], check=True)
