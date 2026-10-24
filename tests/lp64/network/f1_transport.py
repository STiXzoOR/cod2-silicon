#!/usr/bin/env python3
"""One sequential source fixture for Mac 1.3 Netchan transmission."""
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
path = 'src/PC/qcommon/net_chan_mp.c'
source = (subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT)
          if a.baseline else (ROOT / path).read_bytes())
(out / 'f1_transport_source.c').write_bytes(source)
rows = json.loads(a.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/' + path))
command = row.get('arguments') or shlex.split(row['command'])
original = row['file'][:-len('/' + path)]
flags = [v.replace(original, str(ROOT)) for v in command[:command.index('-o')]
         if v not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
exe = out / 'f1_transport'
cmd = [*flags, str(ROOT / 'tests/lp64/network/f1_transport.c'),
       '-Wl,-dead_strip', '-o', str(exe)]
(out / 'command.txt').write_text(shlex.join(cmd) + '\n')
with (out / 'compile.log').open('w') as log:
    subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=log, check=True)
with (out / 'run.log').open('w') as log:
    result = subprocess.run([str(exe)], stdout=log, stderr=log)
print((out / 'run.log').read_text())
raise SystemExit(result.returncode)
