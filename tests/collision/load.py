#!/usr/bin/env python3
"""Check production brush partition construction without proprietary data."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline')
args = p.parse_args()
out = args.build.resolve() / ('collision-load-baseline' if args.baseline else 'collision-load')
out.mkdir(exist_ok=True)
rows = json.loads((args.build / 'compile_commands.json').read_text())
row = next(x for x in rows if x['file'].endswith('/src/PC/qcommon/cm_load_obj.c') and 'cod2_macos.dir' in x['command'])
command = shlex.split(row['command'])
flags = [x for x in command[:command.index('-o')] if x != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wl,-dead_strip']
source = 'src/PC/qcommon/cm_load_obj.c'
(out / 'cm_load_source.c').write_bytes(subprocess.check_output(['git', 'show', args.baseline + ':' + source], cwd=ROOT) if args.baseline else (ROOT / source).read_bytes())
exe = out / 'load'
subprocess.run([*flags, str(ROOT / 'tests/collision/load.c'), '-o', str(exe)], cwd=ROOT, check=True)
raise SystemExit(subprocess.run([str(exe)], env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')).returncode)
