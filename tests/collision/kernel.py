#!/usr/bin/env python3
"""Run direct collision kernel contracts using production translation units."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--test', choices=['mesh', 'world', 'contents'], required=True)
p.add_argument('--baseline')
a = p.parse_args()
out = a.build.resolve() / ('collision-' + a.test + ('-baseline' if a.baseline else ''))
out.mkdir(exist_ok=True)
row = next(x for x in json.loads((a.build / 'compile_commands.json').read_text()) if x['file'].endswith('/src/PC/qcommon/cm_trace.c') and 'cod2_macos.dir' in x['command'])
command = shlex.split(row['command'])
flags = [x for x in command[:command.index('-o')] if x != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wl,-dead_strip']
for name in ['cm_trace', 'cm_mesh', 'cm_tracebox', 'cm_world', 'cm_test']:
    source = 'src/PC/qcommon/' + name + '.c'
    (out / (name + '_source.c')).write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT) if a.baseline else (ROOT / source).read_bytes())
exe = out / a.test
sources = [ROOT / 'tests/collision' / (a.test + '.c')]
if a.test == 'mesh':
    sources += [out / 'cm_tracebox_source.c']
subprocess.run([*flags, *map(str, sources), '-o', str(exe)], cwd=ROOT, check=True)
raise SystemExit(subprocess.run([str(exe)], env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1')).returncode)
