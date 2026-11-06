#!/usr/bin/env python3
"""Exercise the complete turret use bodies against Mac branch and ordering contracts."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CASES = ['behind-boundary', 'behind-nonfinite', 'usability', 'use-stances', 'stop-stances', 'free-owner', 'free-unowned', 'init-tags', 'init-traces']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
p.add_argument('--case', action='append', choices=CASES)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
source = 'src/PC/game_mp/g_misc_mp.c'
entry = next(r for r in json.loads(a.compile_db.read_text()) if r['file'].endswith('/' + source))
command = entry.get('arguments') or shlex.split(entry['command'])
old_root = entry['file'][:-len('/' + source)]
flags = [arg.replace(old_root, str(ROOT)) for arg in command[:command.index('-o')] if arg not in ('-c', '-DNDEBUG')]
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
text = subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT).decode() if a.baseline else (ROOT / source).read_text()
# Preprocess before extraction so alternate platforms and disabled debug code
# cannot contribute dependencies or change brace matching.
snapshot = out / 'g_misc_mp.c'
snapshot.write_text(text)
preprocessed = subprocess.check_output([*flags, '-E', '-P', str(snapshot)], cwd=ROOT).decode()
bodies = []
start = preprocessed.index('enum {\n    GMISC_TURRET_STANCE_INVALID')
end = preprocessed.index('};', start) + 2
bodies.append(preprocessed[start:end])
for signature in ['static qboolean turret_behind(gentity_t *self, gentity_t *other)',
                  'void G_ClientStopUsingTurret(gentity_t *self)',
                  'void G_FreeTurret(gentity_t *self)',
                  'void turret_think_init(gentity_t *self)',
                  'qboolean G_IsTurretUsable(gentity_t *self, gentity_t *owner)',
                  'void turret_use(gentity_t *self, gentity_t *owner, gentity_t *activator)']:
    start = preprocessed.index(signature + '\n{')
    brace = preprocessed.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (preprocessed[end] == '{') - (preprocessed[end] == '}')
        end += 1
    bodies.append(preprocessed[start:end])
(out / 'use_body.inc').write_text('\n\n'.join(bodies) + '\n')
exe = out / 'misc_use'
with (out / 'compile.log').open('w') as log:
    subprocess.run([*flags, str(ROOT / 'tests/lp64/game/misc_use.c'), '-o', str(exe)], cwd=ROOT, stdout=log, stderr=log, check=True)
failed = []
for case in a.case or CASES:
    with (out / (case + '.log')).open('w') as log:
        result = subprocess.run([str(exe), case], cwd=out, stdout=log, stderr=log)
    print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed turret-use cases: ' + ', '.join(failed))
