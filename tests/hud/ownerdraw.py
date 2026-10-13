#!/usr/bin/env python3
"""Execute production objective/location ownerdraw paths with synthetic state."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = ['objectives', 'distance', 'entity-origin', 'materials', 'fade', 'client-gates',
         'placement', 'codx-policy', 'location', 'location-fallback']
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='Read ownerdraw source from a Git revision')
parser.add_argument('--case', action='append', choices=CASES)
args = parser.parse_args()
output = args.output.resolve()
if output == ROOT or ROOT in output.parents:
    parser.error('--output must be outside the checkout')
output.mkdir(parents=True, exist_ok=True)
rows = json.loads(args.compile_commands.read_text())


def compile_flags(relative):
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


def source(relative):
    return (subprocess.check_output(['git', 'show', args.baseline + ':' + relative], cwd=ROOT).decode()
            if args.baseline else (ROOT / relative).read_text())


def function(text, name):
    match = re.search(r'^[^\n]*\b' + name + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError('Missing production function ' + name)
    start = match.start()
    if match[0].lstrip().startswith(name + '('):
        start = text.rfind('\n', 0, start - 1) + 1
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]',
                             text[start:], re.S):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[start:start + token.end()] + '\n'
    raise ValueError('Unterminated production function ' + name)


path = 'src/PC/cgame_mp/cg_newDraw_mp.c'
owner = source(path)
flags = compile_flags(path)
# Parse the complete translation unit using the actual engine build flags.
if not args.baseline:
    subprocess.run([*flags, '-c', str(ROOT / path), '-o', str(output / 'ownerdraw-production.o')],
                   cwd=ROOT, check=True)

# Keep dispatch intact. Unrelated cases abort if reached; the tested ownerdraw,
# fading, localization, material validation, orientation and geometry are real.
code = owner[:owner.index('static qboolean CG_DrawHealthEnabled')]
unrelated = ['CG_DrawPlayerAmmoValue', 'CG_DrawPlayerAmmoBackdrop', 'CG_DrawPlayerStance',
    'CG_DrawScore', 'CG_GameTypeString', 'CG_GetKillerText', 'CG_DrawCenteredTextInRect',
    'CG_DrawHoldBreathHint', 'CG_DrawCursorhint', 'CG_DrawPlayerHealthBar', 'CG_DrawMantleHint',
    'CG_DrawPlayerWeaponName', 'CG_DrawPlayerWeaponNameBack', 'CG_DrawPlayerWeaponModeIcon',
    'CG_DrawPlayerCompass', 'CG_DrawPlayerCompassBack', 'CG_DrawPlayerCompassFriendliesOwnerdraw',
    'CG_DrawHudPic', 'CG_DrawPlayerHealthBack', 'CG_DrawLowHealthOverlay']
for name in unrelated:
    body = function(owner, name)
    code += body[:body.index('{')] + '{ abort(); }\n'
for name in ['CG_DrawOffHandName', 'CG_DrawOffHandIcon', 'CG_DrawOffHandHighlight',
             'CG_DrawOffHandAmmo', 'CG_DrawTeamBackground']:
    declaration = re.search(r'^extern (void ' + name + r'\([^;]*\));', owner, re.M)[1]
    code += declaration + ' { abort(); }\n'
for name in ['CG_AreHudMenusHidden_inl', 'CG_FadeHudMenu_inl', 'CG_FadeDurationMsec',
             'CG_HudFadeAlpha', 'CG_ServerMaterialName', 'CG_GetTranslatedLocationString']:
    code += function(owner, name)
if re.search(r'\bCG_DrawPlayerCompassPointers\([^;{]*\)\s*\{', owner):
    code += function(owner, 'CG_DrawPlayerCompassPointers')
code += function(owner, 'CG_CopyColor')
code += function(owner, 'CG_OwnerDraw')
(output / 'ownerdraw-source.h').write_text(code)

drawtools = source('src/PC/cgame_mp/cg_drawtools_mp.c')
compass = source('src/PC/cgame_mp/cg_compassfriendlies_mp.c')
math_path = 'src/PC/universal/com_math.c'
math_source = subprocess.run([*compile_flags(math_path), '-E', '-P', '-x', 'c', '-'],
                             input=source(math_path), cwd=ROOT, capture_output=True,
                             text=True, check=True).stdout
dependencies = ''.join(function(math_source, name) for name in
                       ['vectoyaw', 'AngleNormalize360', 'AngleNormalize180', 'AngleSubtract_core', 'AngleSubtract'])
dependencies += function(drawtools, 'CG_FadeColor')
dependencies += function(drawtools, 'CG_UpdateCompPointerOrientation')
dependencies += function(compass, 'dvar_value')
dependencies += function(compass, 'CG_ApplyCompassPointerRadiusScale')
(output / 'ownerdraw-dependencies.h').write_text(dependencies)

flags = [flag for flag in flags if flag != '-DNDEBUG']
sanitize = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
executable = output / 'ownerdraw'
subprocess.run([*flags, *sanitize, '-I' + str(output), str(ROOT / 'tests/hud/ownerdraw.c'),
                '-Wl,-dead_strip', '-o', str(executable)], cwd=ROOT, check=True)
environment = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
results, logs = [], []
for case in args.case or CASES:
    result = subprocess.run([str(executable), case], cwd=output, env=environment,
                            capture_output=True, text=True)
    print(result.stdout + result.stderr, end='', flush=True)
    logs.append(case + '\n' + result.stdout + result.stderr)
    results.append(dict(case=case, status='pass' if result.returncode == 0 else 'fail',
                        returncode=result.returncode))
(output / 'results.json').write_text(json.dumps(dict(baseline=args.baseline, cases=results), indent=2) + '\n')
(output / 'cases.log').write_text('\n'.join(logs))
failed = [result['case'] for result in results if result['status'] == 'fail']
print('FAIL ' + ', '.join(failed) if failed else 'PASS all selected ownerdraw fixtures', flush=True)
raise SystemExit(bool(failed))
