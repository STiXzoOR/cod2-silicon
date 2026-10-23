#!/usr/bin/env python3
"""Check Mac 1.3 prone trace order with synthetic collision responses."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from policy_build import function

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--compile-db', type=Path)
p.add_argument('--output', type=Path)
p.add_argument('--baseline')
a = p.parse_args()
out = (a.output or a.build / 'f1-misc').resolve()
out.mkdir(parents=True, exist_ok=True)
rows = json.loads((a.compile_db or a.build / 'compile_commands.engine.json').read_text())
path = 'src/PC/bgame/bg_misc.c'
row = next(r for r in rows if r['file'].endswith('/' + path) and 'cod2_macos.dir' in r['command'])
source_root = row['file'][:-len('/' + path)]
flags = row.get('arguments') or shlex.split(row['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
          '-I' + str(out), '-ffunction-sections', '-fdata-sections']
source = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT, text=True) if a.baseline else (ROOT / path).read_text()
source = subprocess.run(['unifdef', '-DCOD2_X64=1'], input=source, text=True, stdout=subprocess.PIPE, check=False).stdout
(out / 'f1_prone_source.h').write_text(function(source, 'BG_CheckProneValid') + function(source, 'BG_CheckProne'))
subprocess.run([*flags, str(ROOT / 'tests/movement/f1_misc.c'), '-Wl,-dead_strip', '-o', str(out / 'f1-misc')], cwd=ROOT, check=True)
subprocess.run([str(out / 'f1-misc')], check=True)
