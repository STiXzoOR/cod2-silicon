#!/usr/bin/env python3
"""Exercise production turret/spectator HUD routines with Mac 1.3 contracts."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='Read the HUD implementation from a Git revision')
parser.add_argument('--case', action='append', choices=['turret-material', 'turret-gates',
    'turret-color', 'turret-placement', 'spectator-gates', 'spectator-bindings',
    'spectator-layout', 'follow-policy'])
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


path = 'src/PC/cgame_mp/cg_draw_mp.c'
source = (subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT)
          if args.baseline else (ROOT / path).read_bytes())
(output / 'hud_source.h').write_bytes(source)
flags = compile_entry(path)
# Verify the real translation unit with the engine's flags before the fixture.
if not args.baseline:
    subprocess.run([*flags, '-c', str(ROOT / path), '-o', str(output / 'hud-production.o')],
                   cwd=ROOT, check=True)

# Keep the real client draw adapter and screen-placement implementation in the
# path between CG_DrawTurretCrossHair and the captured renderer command.
adapter_path = 'src/PC/client_mp/cl_cgame_mp.c'
active = subprocess.run([*compile_entry(adapter_path), '-E', '-P', str(ROOT / adapter_path)],
                        cwd=ROOT, check=True, capture_output=True, text=True).stdout
match = re.search(r'^void CL_DrawStretchPic\([^;{]*\)\s*\{', active, re.M)
if not match:
    raise ValueError('Missing production CL_DrawStretchPic')
depth = 0
for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', active[match.start():]):
    if token[0] == '{':
        depth += 1
    elif token[0] == '}':
        depth -= 1
        if not depth:
            (output / 'draw_adapter.h').write_text(active[match.start():match.start() + token.end()] + '\n')
            break
else:
    raise ValueError('Unterminated production CL_DrawStretchPic')

sanitize = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
flags = [flag for flag in flags if flag != '-DNDEBUG']
fixture = output / 'hud.o'
placement = output / 'placement.o'
subprocess.run([*flags, *sanitize, '-I' + str(output), '-c', str(ROOT / 'tests/hud/hud.c'),
                '-o', str(fixture)], cwd=ROOT, check=True)
placement_path = 'src/PC/client_mp/screen_placement_mp.c'
placement_flags = [flag for flag in compile_entry(placement_path) if flag != '-DNDEBUG']
subprocess.run([*placement_flags, *sanitize, '-c', str(ROOT / placement_path), '-o', str(placement)],
               cwd=ROOT, check=True)
executable = output / 'hud'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                str(fixture), str(placement), '-o', str(executable)], cwd=ROOT, check=True)
environment = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
results = []
logs = []
for case in args.case or ['turret-material', 'turret-gates', 'turret-color', 'turret-placement',
                         'spectator-gates', 'spectator-bindings', 'spectator-layout', 'follow-policy']:
    result = subprocess.run([str(executable), case], cwd=output, env=environment,
                            capture_output=True, text=True)
    print(result.stdout + result.stderr, end='', flush=True)
    logs.append(case + '\n' + result.stdout + result.stderr)
    results.append(dict(case=case, status='pass' if result.returncode == 0 else 'fail',
                        returncode=result.returncode))
(output / 'results.json').write_text(json.dumps(dict(baseline=args.baseline, cases=results), indent=2) + '\n')
(output / 'cases.log').write_text('\n'.join(logs))
failed = [result['case'] for result in results if result['status'] == 'fail']
print('FAIL ' + ', '.join(failed) if failed else 'PASS all selected HUD fixtures', flush=True)
raise SystemExit(bool(failed))
