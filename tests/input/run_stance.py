#!/usr/bin/env python3
"""Exercise production keyboard stance and command construction; no game or display."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='Read production sources from this local Git revision')
args = parser.parse_args()
args.output = args.output.resolve()
if args.output == ROOT or ROOT in args.output.parents:
    parser.error('--output must be outside the checkout')
args.output.mkdir(parents=True, exist_ok=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
rows = json.loads(args.compile_db.read_text())
entry = next(row for row in rows if row['file'].endswith('/src/PC/client_mp/cl_input.c')
             and 'cod2_macos.dir' in row.get('command', str(row.get('arguments', []))))
old_root = Path(entry['file']).parents[3]
command = entry.get('arguments') or shlex.split(entry['command'])
flags = []
skip = False
for word in command:
    if skip:
        skip = False
        continue
    if word in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif word not in ('-c', '-MD', '-MMD', '-DNDEBUG', entry['file']):
        flags.append(word.replace(str(old_root), str(ROOT)))
flags += ['-UNDEBUG', '-I' + str(args.output.resolve()), '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']


def source(path):
    if args.baseline:
        return subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT)
    return (ROOT / path).read_bytes()


def active(path):
    return subprocess.run([*flags, '-E', '-P', '-iquote', str((ROOT / path).parent), '-x', 'c', '-'],
                          input=source(path), cwd=ROOT, capture_output=True, check=True).stdout.decode()


def function(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError('Missing production function ' + name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()] + '\n'
    raise ValueError('Unterminated production function ' + name)


text = active('src/PC/client_mp/cl_input.c')
# Include actual handlers: the fixture supplies only command arguments and globals.
handlers = re.findall(r'^void (IN_\w+)\([^;{]*\)\s*\{', text, re.M)
names = ['CL_ConsumeButtonPress', 'CL_KeyState', 'CL_KeyMoveValue',
         'CL_ClampMouseAxisDelta', 'CL_FiniteOrDefault', *handlers,
         'CL_CmdButtons', 'CL_MouseMove', 'CL_AdjustAngles', 'CL_KeyMove', 'CL_CreateCmd']
generated = ''
mg = re.search(r'^static const dvar_t \*cl_sensitivityMG\s*;', text, re.M)
if mg:
    generated += mg[0] + '\n'
weapons = active('src/PC/bgame/bg_weapons.c')
generated += function(weapons, 'PM_IsBinocularsADS_core')
generated += function(weapons, 'PM_IsBinocularsADS')
generated += function(active('src/PC/universal/com_math.c'), 'ClampChar')
generated += ''.join(function(text, name) for name in names)
(args.output / 'stance_source.h').write_text(generated)
if not args.baseline:
    subprocess.run([*flags, '-c', str(ROOT / 'src/PC/client_mp/cl_input.c'),
                    '-o', str(args.output / 'cl_input.o')], cwd=ROOT, check=True)
exe = args.output / 'stance'
subprocess.run([*flags, '-fno-sanitize-recover=all', '-Wl,-dead_strip',
                str(ROOT / 'tests/input/stance.c'), '-o', str(exe)], cwd=ROOT, check=True)
result = subprocess.run([str(exe)], cwd=ROOT)
raise SystemExit(result.returncode)
