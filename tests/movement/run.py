#!/usr/bin/env python3
"""Test production movement with synthetic services and Mac 1.3 expectations."""
import argparse
import json
import os
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
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
out = a.build.resolve() / 'movement-tests'
out.mkdir(parents=True, exist_ok=True)
source = 'src/PC/bgame/bg_pmove.c'
data = subprocess.check_output(['git', 'show', f'{a.baseline}:{source}'], cwd=ROOT, text=True) if a.baseline else (ROOT / source).read_text()
(out / 'pmove_source.h').write_text(data)
for name in ['slidemove', 'mantle', 'jump']:
    path = 'src/PC/bgame/bg_' + name + '.c'
    content = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT, text=True) if a.baseline else (ROOT / path).read_text()
    (out / (name + '_source.h')).write_text(content)
viewpath = 'src/PC/cgame_mp/cg_view_mp.c'
view = subprocess.check_output(['git', 'show', f'{a.baseline}:{viewpath}'], cwd=ROOT, text=True) if a.baseline else (ROOT / viewpath).read_text()
match = re.search(r'        (float bobMax = [^;]+;)', view)
(out / 'bob_max_source.h').write_text(match[1] + '\n')
transitionpath = 'src/PC/cgame_mp/cg_playerstate_mp.c'
transition = subprocess.check_output(['git', 'show', f'{a.baseline}:{transitionpath}'], cwd=ROOT, text=True) if a.baseline else (ROOT / transitionpath).read_text()
start = transition.index('void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops)\n{')
# This is the final definition in the production translation unit.
(out / 'transition_source.h').write_text(transition[start:])
weaponpath = 'src/PC/bgame/bg_weapons.c'
weapon = subprocess.check_output(['git', 'show', f'{a.baseline}:{weaponpath}'], cwd=ROOT, text=True) if a.baseline else (ROOT / weaponpath).read_text()
start = weapon.index('void BG_CalculateWeaponPosition_Sway(playerState_t *ps, vec_t *swayViewAngles, vec_t *swayOffset, vec_t *swayAngles, float ssSwayScale, int frametime)\n{')
end = weapon.index('\nint BG_GetFirstAvailableOffhand(', start)
(out / 'sway_source.h').write_text(weapon[start:end])
initpath = 'src/PC/game_mp/g_main_mp.c'
server = subprocess.check_output(['git', 'show', f'{a.baseline}:{initpath}'], cwd=ROOT, text=True) if a.baseline else (ROOT / initpath).read_text()
start = server.index('    Scr_EndLoadScripts();') + len('    Scr_EndLoadScripts();')
end = server.index('    *(void **)imp_bgs = (void *)&level_bgs;', start)
(out / 'server_anim_init_source.h').write_text(server[start:end])
defaults = {'friction': '5.5f', 'stopspeed': '100.0f', 'inertiaMax': '10000.0f',
            'player_footstepsThreshhold': '190.0f', 'player_moveThreshhold': '10.0f',
            'player_strafeSpeedScale': '0.8f', 'player_backSpeedScale': '0.7f',
            'player_spectateSpeedScale': '2.0f', 'player_dmgtimer_maxTime': '750.0f',
            'bg_fallDamageMinHeight': '128.0f', 'bg_fallDamageMaxHeight': '300.0f',
            'bg_foliagesnd_minspeed': '40.0f', 'bg_foliagesnd_maxspeed': '180.0f'}
ints = {'player_dmgtimer_stumbleTime': 500, 'player_dmgtimer_flinchTime': 500,
        'bg_foliagesnd_resetinterval': 500, 'bg_foliagesnd_fastinterval': 500,
        'bg_foliagesnd_slowinterval': 1500}
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
for name in a.test or ['bob', 'scale', 'walk', 'lean', 'timers', 'direction', 'view', 'ladder', 'prone', 'dispatch']:
    if name in ('collision', 'events', 'sway'):
        continue
    subprocess.run([str(exe), name], check=True)
if not a.test or 'collision' in a.test:
    subprocess.run([*flags, '-c', str(ROOT / 'tests/movement/collision.c'), '-o', str(obj)], check=True)
    collision = out / 'collision'
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip', *map(str, objects), '-o', str(collision)], check=True)
    subprocess.run([str(collision)], check=True)
if not a.test or 'events' in a.test:
    subprocess.run([*flags, '-c', str(ROOT / 'tests/movement/events.c'), '-o', str(obj)], check=True)
    events = out / 'events'
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip', str(obj), '-o', str(events)], check=True)
    subprocess.run([str(events)], check=True)
if not a.test or 'sway' in a.test:
    subprocess.run([*flags, '-c', str(ROOT / 'tests/movement/sway.c'), '-o', str(obj)], check=True)
    sway = out / 'sway'
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip', *map(str, objects), '-o', str(sway)], check=True)
    subprocess.run([str(sway)], check=True)
