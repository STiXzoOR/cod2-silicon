#!/usr/bin/env python3
"""Run isolated arm64 tests using this worktree's actual CMake flags."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
parser.add_argument('--baseline', help='test source from a local git commit without changing checkout')
args = parser.parse_args()
build = args.build.resolve()
output = build / ('ws6-test-baseline' if args.baseline else 'ws6-tests')
output.mkdir(parents=True, exist_ok=True)
entries = json.loads((build / 'compile_commands.json').read_text())


def compile_source(source, name, baseline=False):
    entry = next(e for e in entries if e['file'].endswith('/src/PC/server_mp/sv_init_mp.c'))
    flags = shlex.split(entry['command'])
    flags = flags[:flags.index('-o')]
    flags += ['-ffunction-sections', '-fdata-sections', '-Wno-unused-parameter',
              '-fsanitize=address', '-fno-omit-frame-pointer']
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
                      ('netchan', ['src/PC/server_mp/sv_net_chan_mp.c'])]:
    objects = [compile_source(s, Path(s).stem, True) for s in sources]
    objects.append(compile_source(f'tests/lp64/game/{name}.c', name))
    exe = output / name
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address', '-Wl,-dead_strip', *map(str, objects), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], cwd=output, check=True)
