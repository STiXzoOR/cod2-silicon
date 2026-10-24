#!/usr/bin/env python3
"""Test F1 animation reconciliation using real animation, parser and angle math."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='read animation source from a local commit')
parser.add_argument('--case', action='append', choices=['play', 'condition', 'selection', 'lerp', 'angles', 'controllers', 'dobj', 'parser', 'codx'])
args = parser.parse_args()
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)

source = 'src/PC/bgame/bg_animation_mp.c'
for path, filename in [(source, 'animation_source.h'), ('src/PC/bgame/cod2x_animation.h', 'cod2x_animation.h')]:
    data = subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=ROOT) if args.baseline else (ROOT / path).read_bytes()
    (output / filename).write_bytes(data)

entries = json.loads(args.compile_db.read_text())
entry = next(row for row in entries if row['file'].endswith('/' + source))
source_root = entry['file'][:-len('/' + source)]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [flag.replace(source_root, str(ROOT)) for flag in flags[:flags.index('-o')] if flag != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(output), '-I' + str(ROOT / 'src/PC/bgame'),
          '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer']
objects = []
for path in ['tests/movement/f1_animation.c', 'src/PC/universal/com_math.c', 'src/PC/universal/q_parse.c']:
    obj = output / (Path(path).stem + '.o')
    with (output / (Path(path).stem + '.compile.log')).open('w') as log:
        subprocess.run([*flags, '-c', str(ROOT / path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
executable = output / 'f1_animation'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                *objects, '-o', str(executable)], check=True)
for case in args.case or [None]:
    subprocess.run([str(executable), *([] if case is None else [case])], check=True)
