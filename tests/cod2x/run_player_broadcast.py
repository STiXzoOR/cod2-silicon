#!/usr/bin/env python3
"""Exercise production player broadcast registration, frame caller and visibility.

Uses actual engine flags with private output paths; no engine/game data is run.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
GAME = 'src/PC/game_mp/g_main_mp.c'
SNAPSHOT = 'src/PC/server_mp/sv_snapshot_mp.c'


def function(source, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError(f'missing function: {name}')
    depth = 0
    tokens = re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', source[match.start():])
    for token in tokens:
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError(f'unterminated function: {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', help='read production sources from this Git commit')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    entries = json.loads(args.compile_db.read_text())
    failed_targets = []
    for target in ('cod2_macos', 'cod2_macos_ded'):
        entry = next(row for row in entries if row['file'].endswith('/' + GAME) and
                     target + '.dir/' in (row.get('command') or ' '.join(row['arguments'])))
        original_root = entry['file'][:-len('/' + GAME)]
        flags = entry.get('arguments') or shlex.split(entry['command'])
        flags = [flag.replace(original_root, str(ROOT)) for flag in flags[:flags.index('-o')]]
        flags = [flag for flag in flags if flag != '-DNDEBUG']
        flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cache = {}

        def active(path):
            if path not in cache:
                source = (subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'],
                                                  cwd=ROOT, text=True) if args.baseline else
                          (ROOT / path).read_text())
                cache[path] = subprocess.run([*flags, '-iquote', str((ROOT / path).parent),
                                             '-E', '-P', '-x', 'c', '-'], input=source,
                                            cwd=ROOT, capture_output=True, text=True, check=True).stdout
            return cache[path]

        game = active(GAME)
        generated = re.search(r'enum\s*\{\s*GMAIN_CS_VOTE_TIME\b[\s\S]*?\};', game)[0] + '\n'
        registration = function(game, 'G_RegisterDvars_impl')
        globals_used = sorted(set(re.findall(r'\b(\w+)\s*=\s*Dvar_Register', registration)))
        generated += ''.join('static const dvar_t *' + name + ';\n' for name in globals_used)
        if 'sv_playerBroadcastLimit' not in globals_used:
            generated += 'static const dvar_t *sv_playerBroadcastLimit;\n'
        generated += registration + function(game, 'G_RegisterDvars')
        if re.search(r'\bG_RefreshPlayerBroadcast\(void\)\s*\{', game):
            generated += function(game, 'G_RefreshPlayerBroadcast')
        generated += function(game, 'G_RunFrame')
        generated += function(active(SNAPSHOT), 'SV_BitVectorTestLocal')
        generated += function(active(SNAPSHOT), 'SV_EntityIsVisibleToClientLocal')
        (out / 'player_broadcast_source.h').write_text(generated)
        exe = out / target
        subprocess.run([*flags, str(ROOT / 'tests/cod2x/test_player_broadcast.c'),
                        '-o', str(exe)], cwd=ROOT, check=True,
                       stdout=(out / (target + '-compile.log')).open('w'),
                       stderr=subprocess.STDOUT)
        result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True)
        (out / (target + '-result.log')).write_text(result.stdout + result.stderr)
        lines = result.stdout.splitlines()
        print(target + ': ' + '\n'.join(lines[:8]), flush=True)
        if len(lines) > 8:
            print(f'{len(lines)} result lines retained in {target}-result.log', flush=True)
        if result.returncode:
            failed_targets.append(target)
    if failed_targets:
        raise SystemExit('Failed fixtures: ' + ', '.join(failed_targets))


if __name__ == '__main__':
    main()
