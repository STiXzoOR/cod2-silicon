#!/usr/bin/env python3
"""Exercise the native production shutdown lifecycle with Mac call-order contracts."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CASES = ['keep', 'keep-save', 'keep-active', 'keep-active-save', 'free', 'free-save', 'free-active', 'free-active-save', 'io', 'no-io']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
p.add_argument('--case', action='append', choices=CASES)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
source = 'src/PC/game_mp/g_main_mp.c'
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/' + source))
command = entry.get('arguments') or shlex.split(entry['command'])
old_root = entry['file'][:-len('/' + source)]
flags = [arg.replace(old_root, str(ROOT)) for arg in command[:command.index('-o')] if arg not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
text = subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT).decode() if a.baseline else (ROOT / source).read_text()
# Preprocess before extraction so alternate platforms and disabled debug code
# cannot contribute dependencies or change brace matching.
snapshot = out / 'g_main_mp.c'
snapshot.write_text(text)
preprocessed = subprocess.check_output([*flags, '-E', '-P', str(snapshot)], cwd=ROOT).decode()
start = preprocessed.index('void G_ShutdownGame(qboolean freeScripts)\n{')
brace = preprocessed.index('{', start)
depth = 1
end = brace + 1
while depth:
    depth += (preprocessed[end] == '{') - (preprocessed[end] == '}')
    end += 1
(out / 'shutdown_body.inc').write_text(preprocessed[start:end] + '\n')
exe = out / 'main_shutdown'
with (out / 'compile.log').open('w') as log:
    subprocess.run([*flags, str(ROOT / 'tests/lp64/game/main_shutdown.c'), '-o', str(exe)], cwd=ROOT, stdout=log, stderr=log, check=True)
failed = []
for case in a.case or CASES:
    with (out / (case + '.log')).open('w') as log:
        result = subprocess.run([str(exe), case], cwd=out, stdout=log, stderr=log)
    print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed game-shutdown cases: ' + ', '.join(failed))
