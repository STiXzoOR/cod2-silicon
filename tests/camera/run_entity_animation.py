#!/usr/bin/env python3
"""Check production entity animation consumers with synthetic DObj and scene services."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-commands', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', help='Read historical player and entity source with current fixture')
a = p.parse_args()
out = a.output.resolve()
if out == ROOT or ROOT in out.parents:
    p.error('--output must be outside the checkout')
out.mkdir(parents=True, exist_ok=True)
rows = json.loads(a.compile_commands.read_text())

def flags(relative):
    row = next(r for r in rows if r['file'].endswith('/' + relative)
               and 'cod2_macos.dir' in (r.get('command') or ' '.join(r['arguments'])))
    old_root = row['file'][:-len(relative)].rstrip('/')
    command = row.get('arguments') or shlex.split(row['command'])
    result, skip = [], False
    for arg in command:
        if skip:
            skip = False
        elif arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', '-DNDEBUG', row['file']):
            result.append(arg.replace(old_root, str(ROOT)))
    return result

paths = {name: f'src/PC/cgame_mp/cg_{name}_mp.c' for name in ['players', 'ents']}
for name, path in paths.items():
    content = subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
    (out / (name + '_source.h')).write_bytes(content)
san = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
       '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
objects = []
for name, path, source, extra in [
    ('fixture', paths['players'], ROOT / 'tests/camera/entity_animation.c', ['-I' + str(out)]),
    ('ents', paths['ents'], out / 'ents_source.h', ['-x', 'c', '-DCG_AddCEntityToScene=Unused_CG_AddCEntityToScene', '-DCG_DObjGetLocalTagMatrix=Unused_CG_DObjGetLocalTagMatrix']),
    ('math', 'src/PC/universal/com_math.c', ROOT / 'src/PC/universal/com_math.c', []),
]:
    obj = out / (name + '.o')
    subprocess.run([*flags(path), *san, *extra, '-c', str(source), '-o', str(obj)], cwd=ROOT, check=True)
    objects.append(str(obj))
exe = out / 'entity_animation'
subprocess.run([flags(paths['players'])[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
failed = []
for case in ['player', 'corpse', 'corpse-first', 'corpse-last', 'mover-enter', 'mover-exit', 'mover-rounding', 'mover-bounds']:
    result = subprocess.run([str(exe), case], capture_output=True, text=True,
                            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}')
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
