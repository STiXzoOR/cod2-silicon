#!/usr/bin/env python3
"""Compile and replay the production hooks in a synthetic two-client world."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/parity'))
from compare import read_trace, compare

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
entry = next(row for row in json.loads(args.compile_db.read_text())
             if row['file'].endswith('/src/unix/sysdiff_statehash.c'))
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [x for x in flags if x != '-DNDEBUG']
binary = out / 'hooks_fixture'
subprocess.run([*flags, '-UNDEBUG', '-fsanitize=address,undefined',
                str(ROOT / 'tools/oracle/hooks_fixture.c'),
                str(ROOT / 'src/unix/sysdiff_statehash.c'), '-o', str(binary)], check=True)


def run(name, replay=None, stream=None, expect=0):
    path = out / name
    path.mkdir(exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('SYSDIFF_')}
    env.update(SYSDIFF_STATEHASH=str(path / 'statehash.txt'), SYSDIFF_PLAYERS=str(path / 'players.txt'),
               SYSDIFF_MAXFRAMES='24', UBSAN_OPTIONS='halt_on_error=1')
    if replay:
        env['SYSDIFF_REPLAY'] = str(replay)
        env['ORACLE_FIXTURE_BASE'] = '9000'
        env['ORACLE_FIXTURE_NETWORK'] = '1'
    else:
        env['SYSDIFF_USERCMDS'] = str(stream or path / 'usercmds.txt')
    result = subprocess.run([str(binary)], env=env, capture_output=True, text=True)
    assert result.returncode == expect, (name, result.returncode, result.stderr)
    return path


record = run('record')
replay = run('replay', record / 'usercmds.txt')
assert compare(read_trace(record / 'statehash.txt'), read_trace(replay / 'statehash.txt')) is None
assert (record / 'players.txt').read_bytes() == (replay / 'players.txt').read_bytes()
assert len((record / 'players.txt').read_text().splitlines()) == 24 * 3
first_player = (record / 'players.txt').read_text().splitlines()[1]
assert 'origin 417e0000,00000000,00000000' in first_player
assert 'pm_time 50 weapon 7' in first_player
rows = (record / 'usercmds.txt').read_text().splitlines()
assert rows[0] == 'cod2-replay 1' and rows[-1] == 'end 24'
assert len(rows) == 24 * 2 + 2
broken = out / 'truncated.txt'
broken.write_text('\n'.join(rows[:-1]) + '\n')
run('truncated', broken, expect=2)
broken = out / 'wrong-client.txt'
rows[1] = rows[1].replace('cmd 1 0 ', 'cmd 1 63 ', 1)
broken.write_text('\n'.join(rows) + '\n')
run('wrong-client', broken, expect=2)
print('PASS: recorded/replayed 24 two-client frames; raw player bits match; malformed streams fail')
