#!/usr/bin/env python3
"""Release must retain optimization; no performance claim follows from this check."""
import argparse
import json
from pathlib import Path
import shlex

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build/ws9'))
args = parser.parse_args()
cache = (args.build / 'CMakeCache.txt').read_text()
assert 'CMAKE_BUILD_TYPE:STRING=Release\n' in cache, 'configure this check as Release'
entries = json.loads((args.build / 'compile_commands.json').read_text())
for target in ('cod2_macos', 'cod2_macos_ded'):
    command = next(e['command'] for e in entries
                   if f'{target}.dir/' in e['command'] and e['file'].endswith('/scr_vm.c'))
    opts = [f for f in shlex.split(command) if f.startswith('-O')]
    assert opts and opts[-1] in ('-O2', '-O3'), (target, opts)
    assert '-ffp-contract=off' in command
print('PASS Release optimization for client and dedicated server; FP contraction stays off')
