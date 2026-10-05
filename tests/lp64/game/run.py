#!/usr/bin/env python3
"""Run isolated arm64 tests using this worktree's actual CMake flags."""
import argparse
import json
from pathlib import Path
import shlex
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
parser.add_argument('--baseline', help='test source from a local git commit without changing checkout')
parser.add_argument('--test', action='append', help='run only a named test (repeatable)')
args = parser.parse_args()
build = args.build.resolve()
output = build / ('ws6-test-baseline' if args.baseline else 'ws6-tests')
output.mkdir(parents=True, exist_ok=True)
# Field labels are opaque to this test; use empty definitions, never retail payloads.
declarations = (ROOT / 'src/PC/bgame/bg_weapons_load_obj_weaponDefFields_decls.inc').read_text()
symbols = re.findall(r'extern const char (\w+)\[\];', declarations)
(output / 'ws6_weapon_symbols.h').write_text(''.join(f'const char {symbol}[] = "";\n' for symbol in symbols))
include_sources = {
    'ws6_snapshot_source.c': 'src/PC/server_mp/sv_snapshot_mp.c',
    'ws6_weapons_source.c': 'src/PC/bgame/bg_weapons_load_obj.c',
    'ws39_player_use.c': 'src/PC/game_mp/player_use_mp.c',
    'ws39_cm_world.c': 'src/PC/qcommon/cm_world.c',
    'ws40_g_utils.c': 'src/PC/game_mp/g_utils_mp.c',
    'bg_weapons_load_obj_weaponDefFields.inc': 'src/PC/bgame/bg_weapons_load_obj_weaponDefFields.inc',
    'bg_weapons_load_obj_weaponDefFields_decls.inc': 'src/PC/bgame/bg_weapons_load_obj_weaponDefFields_decls.inc',
}
for name, source in include_sources.items():
    data = subprocess.check_output(['git', 'show', f'{args.baseline}:{source}'], cwd=ROOT) if args.baseline else (ROOT / source).read_bytes()
    (output / name).write_bytes(data)
entries = json.loads((build / 'compile_commands.json').read_text())


def compile_source(source, name, baseline=False):
    entry = next(e for e in entries if e['file'].endswith('/src/PC/server_mp/sv_init_mp.c'))
    flags = shlex.split(entry['command'])
    flags = flags[:flags.index('-o')]
    flags += ['-ffunction-sections', '-fdata-sections', '-Wno-unused-parameter',
              '-fsanitize=address', '-fno-omit-frame-pointer', '-I' + str(output)]
    if baseline and args.baseline:
        path = output / Path(source).name
        path.write_bytes(subprocess.check_output(['git', 'show', f'{args.baseline}:{source}'], cwd=ROOT))
    else:
        path = ROOT / source
    obj = output / (name + '.o')
    subprocess.run([*flags, '-c', str(path), '-o', str(obj)], cwd=ROOT, check=True,
                   stdout=subprocess.PIPE, stderr=(output / (name + '.log')).open('w'))
    return obj


for name, sources in [('startup', ['src/PC/server_mp/sv_init_mp.c']),
                      ('script_api', ['src/PC/game_mp/g_scr_main_mp.c']),
                      ('snapshot', []),
                      ('netchan', ['src/PC/server_mp/sv_net_chan_mp.c']),
                      ('weapon_fields', []),
                      ('spawn', ['src/PC/game_mp/g_utils_mp.c']),
                      ('use_list', []),
                      ('world_links', []),
                      ('fx_channels', ['src/PC/EffectsCore/FxTemplate.c', 'src/PC/EffectsCore/FxCurve_load_obj.c']),
                      ('shellshock_file', ['src/PC/cgame_mp/cg_shellshock.c']),
                      ('grenades', ['src/PC/game_mp/g_missile_mp.c', 'src/PC/bgame/bg_misc.c', 'src/PC/universal/com_math.c']),
                      ('wire', ['src/PC/qcommon/msg_mp.c'])]:
    if args.test and name not in args.test:
        continue
    objects = [compile_source(s, Path(s).stem, True) for s in sources]
    objects.append(compile_source(f'tests/lp64/game/{name}.c', name))
    exe = output / name
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address', '-Wl,-dead_strip', *map(str, objects), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], cwd=output, check=True)
