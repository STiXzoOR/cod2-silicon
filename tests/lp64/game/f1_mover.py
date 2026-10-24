#!/usr/bin/env python3
"""Exercise production mover collision and trigger hints."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', required=True, type=Path)
p.add_argument('--output', required=True, type=Path)
p.add_argument('--baseline')
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
path = 'src/PC/game_mp/g_mover_mp.c'
source = (subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT)
          if a.baseline else (ROOT / path).read_bytes())
(out / 'f1_mover_source.c').write_bytes(source)
rows = json.loads(a.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/' + path))
command = row.get('arguments') or shlex.split(row['command'])
original = row['file'][:-len('/' + path)]
flags = [v.replace(original, str(ROOT)) for v in command[:command.index('-o')]
         if v not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
          '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
exe = out / 'f1_mover'
objects = []
for relative in ['tests/lp64/game/f1_mover.c', 'src/PC/universal/com_math.c', 'src/PC/universal/q_shared.c']:
    obj = out / (Path(relative).stem + '.o')
    cmd = [*flags, '-c', str(ROOT / relative), '-o', str(obj)]
    with (out / (Path(relative).stem + '.compile.log')).open('w') as log:
        subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=log, check=True)
    objects.append(str(obj))
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
with (out / 'run.log').open('w') as log:
    result = subprocess.run([str(exe)], stdout=log, stderr=log)
print((out / 'run.log').read_text())
raise SystemExit(result.returncode)
