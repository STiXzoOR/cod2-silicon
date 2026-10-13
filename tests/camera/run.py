#!/usr/bin/env python3
"""Exercise the production camera with synthetic tags and Mac 1.3 expectations."""
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
parser.add_argument('--baseline')
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
(output / 'camera_source.h').write_bytes(camera)
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
                '-iquote', str(ROOT / 'src/PC/cgame_mp'), '-c', str(ROOT / 'tests/camera/camera.c'),
                '-o', str(fixture)], cwd=ROOT, check=True)
math_flags = [flag for flag in compile_entry('src/PC/universal/com_math.c') if flag != '-DNDEBUG']
subprocess.run([*math_flags, *sanitize, '-c', str(ROOT / 'src/PC/universal/com_math.c'),
                '-o', str(math_object)], cwd=ROOT, check=True)
executable = output / 'camera'
subprocess.run([camera_flags[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                str(fixture), str(math_object), '-o', str(executable)], cwd=ROOT, check=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
for case in args.case or ['weapon', 'locked', 'missing', 'gates', 'pipeline', 'normal', 'orbit']:
    subprocess.run([str(executable), case], cwd=output, check=True)
