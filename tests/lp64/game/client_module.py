#!/usr/bin/env python3
"""Check complete production client, shared utility and math TUs under sanitizers."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CASES = ['compass-yaw', 'compass-tie', 'compass-controls', 'view-lean',
         'view-controls', 'view-tag-error', 'disconnect']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', help='Read the production client source from this local revision')
p.add_argument('--case', action='append', choices=CASES)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
rows = json.loads(a.compile_db.read_text())
client = 'src/PC/game_mp/g_client_mp.c'


def flags_for(source):
    entry = next(row for row in rows if row['file'].endswith('/' + source))
    command = entry.get('arguments') or shlex.split(entry['command'])
    old_root = entry['file'][:-len('/' + source)]
    flags = [arg.replace(old_root, str(ROOT)) for arg in command[:command.index('-o')]
             if arg not in ('-c', '-DNDEBUG')]
    return flags + ['-UNDEBUG', '-ffunction-sections', '-fdata-sections',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer']


client_source = out / 'g_client_mp.c'
client_source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + client], cwd=ROOT)
                          if a.baseline else (ROOT / client).read_bytes())
objects = []
sources = [(client_source, client), (ROOT / 'tests/lp64/game/client_module.c', client)]
for name in ['com_math', 'q_shared']:
    path = 'src/PC/universal/' + name + '.c'
    sources.append((ROOT / path, path))
for path, source in sources:
    obj = out / (path.stem + '.o')
    with (out / (path.stem + '.compile.log')).open('w') as log:
        subprocess.run([*flags_for(source), '-c', str(path), '-o', str(obj)], cwd=ROOT,
                       stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'client_module'
subprocess.run([flags_for(client)[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
failed = []
for case in a.case or CASES:
    with (out / (case + '.log')).open('w') as log:
        result = subprocess.run([str(exe), case], stdout=log, stderr=log, cwd=out)
    print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed client-module cases: ' + ', '.join(failed))
