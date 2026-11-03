#!/usr/bin/env python3
"""Check G_InitGame's script-load dependency boundary using production C.

Only GScr_LoadScripts is extracted; nested loaders and script VM operations are
fixture seams. This does not certify the remaining g_scr_main module.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/cod2x'))
from run_player_broadcast import function

CASES = ['map', 'empty-map', 'null-map-string', 'null-map-dvar']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
p.add_argument('--case', action='append', choices=CASES)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
source = 'src/PC/game_mp/g_scr_main_mp.c'
rows = json.loads(a.compile_db.read_text())
production = subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT, text=True) if a.baseline else (ROOT / source).read_text()
failed = []
for target in ('cod2_macos', 'cod2_macos_ded'):
    entry = next(r for r in rows if r['file'].endswith('/' + source) and target + '.dir/' in (r.get('command') or ' '.join(r['arguments'])))
    command = entry.get('arguments') or shlex.split(entry['command'])
    old_root = entry['file'][:-len('/' + source)]
    flags = [x.replace(old_root, str(ROOT)) for x in command[:command.index('-o')] if x not in ('-c', '-DNDEBUG')]
    flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    active = subprocess.run([*flags, '-iquote', str((ROOT / source).parent), '-E', '-P', '-x', 'c', '-'], input=production, text=True, capture_output=True, check=True, cwd=ROOT).stdout
    (out / 'script_load_body.inc').write_text(function(active, 'GScr_LoadScripts'))
    exe = out / target
    with (out / (target + '.compile.log')).open('w') as log:
        subprocess.run([*flags, str(ROOT / 'tests/lp64/game/main_script_load.c'), '-o', str(exe)], cwd=ROOT, stdout=log, stderr=log, check=True)
    for case in a.case or CASES:
        with (out / (target + '-' + case + '.log')).open('w') as log:
            result = subprocess.run([str(exe), case], cwd=out, stdout=log, stderr=log)
        print(target + '/' + case + ': ' + ('FAIL' if result.returncode else 'pass'), flush=True)
        if result.returncode:
            failed.append(target + '/' + case)
if failed:
    raise SystemExit('Failed script-load boundary cases: ' + ', '.join(failed))
