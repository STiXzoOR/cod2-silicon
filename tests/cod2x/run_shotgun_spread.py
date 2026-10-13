#!/usr/bin/env python3
"""Test authoritative bullet endpoints/damage with real engine flags; no game data."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WEAPON = 'src/PC/game_mp/g_weapon_mp.c'
MAIN = 'src/PC/game_mp/g_main_mp.c'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='Read production sources from this local Git revision')
parser.add_argument('--skip-registration', action='store_true', help='Characterize spread before its dvar exists')
parser.add_argument('--release-tus', action='store_true', help='Also compile changed client/dedicated Release translation units')
args = parser.parse_args()
args.output = args.output.resolve()
args.output.mkdir(parents=True, exist_ok=False)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')


def source(path):
    if args.baseline:
        return subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT)
    return (ROOT / path).read_bytes()


rows = json.loads(args.compile_db.read_text())
entry = next(row for row in rows if row['file'].endswith('/' + WEAPON)
             and 'cod2_macos.dir/' in row.get('command', str(row.get('arguments', []))))
old_root = Path(entry['file']).parents[3]
remapped = []
for row in rows:
    remapped.append({
        'directory': str(ROOT),
        'file': row['file'].replace(str(old_root), str(ROOT)),
        'arguments': [word.replace(str(old_root), str(ROOT)) for word in
                      (row.get('arguments') or shlex.split(row['command']))],
    })
(args.output / 'compile_commands.engine.json').write_text(json.dumps(remapped, indent=2) + '\n')
scoped = [row for row in remapped if row['file'] in (str(ROOT / WEAPON), str(ROOT / MAIN))]
(args.output / 'compile_commands.scoped.json').write_text(json.dumps(scoped, indent=2) + '\n')


def flags(row, assertions=False):
    result, skip = [], False
    for word in row['arguments']:
        if skip:
            skip = False
        elif word in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif word not in ('-c', '-MD', '-MMD', row['file']):
            if not assertions or word != '-DNDEBUG':
                result.append(word)
    if assertions:
        result += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                   '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
                   '-I' + str(args.output), '-iquote', str(ROOT / Path(WEAPON).parent)]
    return result


def function(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError('Missing production function ' + name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()] + '\n'
    raise ValueError('Unterminated production function ' + name)


fixture_flags = flags(next(row for row in remapped if row['file'] == str(ROOT / WEAPON)
                          and 'cod2_macos.dir/' in str(row['arguments'])), assertions=True)
(args.output / 'shotgun_weapon_source.h').write_bytes(source(WEAPON))
registration = ''
if not args.skip_registration:
    processed = subprocess.run([*fixture_flags, '-E', '-P', '-iquote', str(ROOT / Path(MAIN).parent),
                                '-x', 'c', '-'], input=source(MAIN), cwd=ROOT,
                               capture_output=True, check=True).stdout.decode()
    registration = function(processed, 'G_RegisterDvars_impl')
    globals_ = sorted(set(re.findall(r'\b(\w+)\s*=\s*Dvar_Register\w+\(', registration))
                      - {'g_antilag', 'g_shotgun_spread_fix', 'sv_fps'})
    registration = ''.join('const dvar_t *' + name + ';\n' for name in globals_) + registration
    registration += function(processed, 'G_RegisterDvars')
else:
    fixture_flags += ['-DFIXTURE_SKIP_REGISTRATION=1']
(args.output / 'shotgun_registration_source.h').write_text(registration)

exe = args.output / 'shotgun_spread'
with (args.output / 'fixture_compile.log').open('w') as log:
    result = subprocess.run([*fixture_flags, '-Wl,-dead_strip', str(ROOT / 'tests/cod2x/test_shotgun_spread.c'),
                             '-o', str(exe)], cwd=ROOT, stdout=log, stderr=log)
if result.returncode:
    print((args.output / 'fixture_compile.log').read_text())
    result.check_returncode()
result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True)
(args.output / 'fixture_run.log').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr, end='')
result.check_returncode()

if args.release_tus:
    for index, row in enumerate(scoped):
        opts = [word for word in row['arguments'] if word.startswith('-O')]
        assert opts and opts[-1] in ('-O2', '-O3'), (row['file'], opts)
        assert '-DNDEBUG' in row['arguments'] and '-ffp-contract=off' in row['arguments']
        command = flags(row) + ['-c', row['file'], '-o', str(args.output / ('release-' + str(index) + '.o'))]
        with (args.output / ('release-' + str(index) + '.log')).open('w') as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=log, check=True)
    print('PASS Release optimization/FP flags and ' + str(len(scoped)) + ' changed client/dedicated translation units')
