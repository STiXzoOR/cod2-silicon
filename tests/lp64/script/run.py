#!/usr/bin/env python3
"""Run isolated real-script-source checks using the macOS target's flags."""
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
build = root / 'build-macos'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in commands if e['file'].endswith('/scr_vm.c')
             and 'cod2_macos.dir' in e['command'])
argv = shlex.split(entry['command'])
flags = argv[1:argv.index('-o')]
checks = [('runtime_test', ['src/PC/script/scr_memorytree.c']),
          ('variable_test', ['src/PC/script/scr_memorytree.c']),
          ('string_test', ['src/PC/script/scr_memorytree.c']),
          ('animation_test', []),
          ('bindings_test', ['tests/lp64/script/bindings_fixture.c']),
          ('storage_test', ['src/PC/script/scr_memorytree.c',
                            'build-macos/CMakeFiles/cod2_macos.dir/build/x64_gen/bss_native.c.o'])]
for name, sources in checks:
    output = build / ('ws6-' + name)
    result = subprocess.run([argv[0], *flags, '-fsanitize=address', '-Wl,-dead_strip',
                             f'tests/lp64/script/{name}.c', *sources, '-o', str(output)],
                            cwd=root, text=True, capture_output=True)
    (build / ('ws6-' + name + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout + result.stderr, file=sys.stderr)
        raise SystemExit(result.returncode)
    subprocess.run([str(output)], cwd=root, check=True)
for runner in ['compiler_checks.py', 'vm_semantics.py']:
    subprocess.run([sys.executable, str(root / 'tests/lp64/script' / runner)],
                   cwd=root, check=True)
