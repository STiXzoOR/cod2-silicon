#!/usr/bin/env python3
"""Exercise authoritative grenade stance correction through production controllers."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'movement'))
from policy_build import compile_policy

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='read production source from a local commit')
parser.add_argument('--revision-matrix', action='store_true')
args = parser.parse_args()
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)


def source(path):
    if args.baseline:
        return subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


animation_source = 'src/PC/bgame/bg_animation_mp.c'
(output / 'animation_source.h').write_text(source(animation_source))
(output / 'cod2x_animation.h').write_text(source('src/PC/bgame/cod2x_animation.h'))
for name, path, start, end in [
    ('server_controller_source.h', 'src/PC/game_mp/g_active_mp.c',
     'void G_PlayerController(gentity_t *self, int *partBits)\n{', '\nvoid G_PlayerEvent('),
    ('playerstate_source.h', 'src/PC/server_mp/sv_game_mp.c',
     'playerState_t *SV_GameClientNum(int num)\n{', '\nbyte *SV_SvEntityForGentity('),
]:
    text = source(path)
    first = text.index(start)
    (output / name).write_text(text[first:text.index(end, first)])

entries = json.loads(args.compile_db.read_text())
entry = next(row for row in entries if row['file'].endswith('/' + animation_source))
source_root = entry['file'][:-len('/' + animation_source)]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [flag.replace(source_root, str(ROOT)) for flag in flags if flag != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(output), '-I' + str(ROOT / 'src/PC/bgame'),
          '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer']
objects = []
if args.revision_matrix:
    flags += ['-DGAMEPLAY_MATRIX=1']
    objects += list(map(str, compile_policy(ROOT, output, flags)))
for path, name in [('tests/cod2x/animation_stance.c', 'animation_stance'),
                   ('src/PC/universal/com_math.c', 'com_math')]:
    obj = output / (name + '.o')
    with (output / (name + '.compile.log')).open('w') as log:
        subprocess.run([*flags, '-c', str(ROOT / path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
executable = output / 'animation_stance'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(executable)], check=True)
subprocess.run([str(executable)], check=True)
