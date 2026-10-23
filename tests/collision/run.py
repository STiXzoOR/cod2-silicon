#!/usr/bin/env python3
"""Exercise production collision sources with actual native engine flags."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--baseline', help='Read production sources from this local Git revision')
args = parser.parse_args()
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
out = args.build.resolve() / ('collision-baseline' if args.baseline else 'collision-tests')
out.mkdir(parents=True, exist_ok=True)
database = args.build / 'compile_commands.engine.json'
if not database.exists():
    database = args.build / 'compile_commands.json'
rows = json.loads(database.read_text())
entry = next(row for row in rows if row['file'].endswith('/src/PC/qcommon/cm_trace.c')
             and 'cod2_macos.dir' in row['command'])
command = shlex.split(entry['command'])
flags = command[:command.index('-o')]
old_root = str(Path(entry['file']).parents[3])
flags = [flag.replace(old_root, str(ROOT)) for flag in flags if flag != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
sources = []
for name in ['cm_trace', 'cm_mesh', 'cm_tracebox']:
    relative = 'src/PC/qcommon/' + name + '.c'
    contents = subprocess.check_output(['git', 'show', args.baseline + ':' + relative], cwd=ROOT) if args.baseline else (ROOT / relative).read_bytes()
    copy = out / (name + '_source.c')
    copy.write_bytes(contents)
    if name != 'cm_trace':
        sources.append(str(copy))
exe = out / 'collision'
result = subprocess.run([*flags, '-Wl,-dead_strip', str(ROOT / 'tests/collision/collision.c'),
                         *sources, '-o', str(exe)], cwd=ROOT, capture_output=True, text=True)
if result.returncode:
    print(result.stderr)
    result.check_returncode()
failures = subprocess.run([str(exe)], cwd=ROOT).returncode != 0
for script, extra in [('load.py', []), ('kernel.py', ['--test', 'mesh']),
                      ('kernel.py', ['--test', 'world']), ('kernel.py', ['--test', 'contents'])]:
    command = [sys.executable, str(ROOT / 'tests/collision' / script), '--build', str(args.build), *extra]
    if args.baseline:
        command += ['--baseline', args.baseline]
    failures += subprocess.run(command, cwd=ROOT).returncode != 0
print(f'collision suites: {failures} failing / 5')
raise SystemExit(bool(failures))
