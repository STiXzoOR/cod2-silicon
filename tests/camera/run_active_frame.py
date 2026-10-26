#!/usr/bin/env python3
"""Exercise production active-frame, recoil producer and aim consumer against Mac 1.3 expectations."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='Use historical cg_view_mp.c with current fixture and dependencies')
parser.add_argument('--case', action='append')
args = parser.parse_args()
output = args.output.resolve()
if output == ROOT or ROOT in output.parents:
    parser.error('--output must be outside the checkout')
output.mkdir(parents=True, exist_ok=True)
rows = json.loads(args.compile_commands.read_text())


def compile_entry(relative):
    row = next(row for row in rows if row['file'].endswith('/' + relative)
               and 'cod2_macos.dir' in (row.get('command') or ' '.join(row['arguments'])))
    old_root = row['file'][:-len(relative)].rstrip('/')
    command = row.get('arguments') or shlex.split(row['command'])
    command = [part.replace(old_root, str(ROOT)) for part in command]
    flags, skip = [], False
    for part in command:
        if skip:
            skip = False
        elif part in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif part not in ('-c', '-MD', '-MMD', str(ROOT / relative)):
            flags.append(part)
    return flags


camera_path = 'src/PC/cgame_mp/cg_view_mp.c'
camera = (subprocess.check_output(['git', 'show', args.baseline + ':' + camera_path], cwd=ROOT)
          if args.baseline else (ROOT / camera_path).read_bytes())
(output / 'active_frame_source.h').write_bytes(camera)
camera_flags = compile_entry(camera_path)
# Compile the unchanged real translation-unit command before assertion-enabled
# source inclusion; no flags or outputs in the caller's build are modified.
subprocess.run([*camera_flags, '-c', str(ROOT / camera_path), '-o', str(output / 'camera-production.o')],
               cwd=ROOT, check=True)
sanitize = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
camera_flags = [flag for flag in camera_flags if flag != '-DNDEBUG']
fixture = output / 'camera.o'
math_object = output / 'math.o'
subprocess.run([*camera_flags, *sanitize, '-I' + str(output),
                '-iquote', str(ROOT / 'src/PC/cgame_mp'), '-c', str(ROOT / 'tests/camera/active_frame.c'),
                '-o', str(fixture)], cwd=ROOT, check=True)
math_flags = [flag for flag in compile_entry('src/PC/universal/com_math.c') if flag != '-DNDEBUG']
subprocess.run([*math_flags, *sanitize, '-c', str(ROOT / 'src/PC/universal/com_math.c'),
                '-o', str(math_object)], cwd=ROOT, check=True)
# Rename only unused neighboring functions that overlap fixture service stubs.
# BG_WeaponFireRecoil and CG_DrawActive retain their production implementations.
extra_objects = []
for relative, renames in [
    ('src/PC/bgame/bg_weapons.c', ['BG_GetWeaponDef', 'BG_GetNumWeapons', 'BG_GetViewmodelWeaponIndex', 'BG_CalculateViewAngles', 'BG_GetBobCycle', 'BG_GetHorizontalBobFactor', 'BG_GetVerticalBobFactor', 'BG_IsAimDownSightWeapon']),
    ('src/PC/cgame_mp/cg_draw_mp.c', ['CG_DrawPlayerSprites', 'CG_Draw3dHudElems', 'CG_Draw2D', 'CG_AddLagometerFrameInfo', 'CG_ShakeCamera']),
]:
    flags = [flag for flag in compile_entry(relative) if flag != '-DNDEBUG']
    obj = output / (Path(relative).stem + '.o')
    subprocess.run([*flags, *sanitize, *['-D' + name + '=Unused_' + name for name in renames],
                    '-c', str(ROOT / relative), '-o', str(obj)], cwd=ROOT, check=True)
    extra_objects.append(str(obj))
executable = output / 'active_frame'
subprocess.run([camera_flags[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                str(fixture), str(math_object), *extra_objects, '-o', str(executable)], cwd=ROOT, check=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
failed = []
for case in args.case or ['recoil', 'limits', 'entities', 'producer', 'gates']:
    result = subprocess.run([str(executable), case], cwd=output, capture_output=True, text=True)
    (output / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}')
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
