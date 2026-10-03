#!/usr/bin/env python3
"""Run isolated real-script-source checks using the macOS target's flags."""
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
build = root / 'build-macos'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in commands if e['file'].endswith('/scr_vm.c')
             and 'cod2_macos.dir' in e['command'])
argv = shlex.split(entry['command'])
flags = argv[1:argv.index('-o')]
checks = [('runtime_test', ['src/PC/script/scr_memorytree.c'])]
for name, sources in checks:
    output = build / ('ws6-' + name)
    subprocess.run([argv[0], *flags, '-Wl,-dead_strip',
                    f'tests/lp64/script/{name}.c', *sources, '-o', str(output)],
                   cwd=root, check=True)
    subprocess.run([str(output)], cwd=root, check=True)
