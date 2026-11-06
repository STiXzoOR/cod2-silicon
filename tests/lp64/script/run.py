#!/usr/bin/env python3
"""Run isolated real-script-source checks using the macOS target's flags."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=root / 'build-macos')
p.add_argument('--output', type=Path)
a = p.parse_args()
build = a.build.resolve()
fixture_output = a.output.resolve() if a.output else build
fixture_output.mkdir(parents=True, exist_ok=True)
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in commands if e['file'].endswith('/scr_vm.c')
             and 'cod2_macos.dir' in e['command'])
argv = shlex.split(entry['command'])
flags = argv[1:argv.index('-o')]
bss_entry = next(e for e in commands if e['file'].endswith('/bss_native.c')
                 and 'cod2_macos.dir' in e['command'])
bss_argv = shlex.split(bss_entry['command'])
bss_object = Path(bss_entry['directory']) / bss_argv[bss_argv.index('-o') + 1]
checks = [('runtime_test', ['src/PC/script/scr_memorytree.c']),
          ('variable_test', ['src/PC/script/scr_memorytree.c']),
          ('string_test', ['src/PC/script/scr_memorytree.c']),
          ('animation_test', []),
          ('bindings_test', ['tests/lp64/script/bindings_fixture.c']),
          ('storage_test', ['src/PC/script/scr_memorytree.c',
                            str(bss_object)])]
for name, sources in checks:
    output = fixture_output / ('ws6-' + name)
    result = subprocess.run([argv[0], *flags, '-fsanitize=address', '-Wl,-dead_strip',
                             f'tests/lp64/script/{name}.c', *sources,
                             'src/PC/qcommon/port_debug.c', '-o', str(output)],
                            cwd=root, text=True, capture_output=True)
    (fixture_output / ('ws6-' + name + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout + result.stderr, file=sys.stderr)
        raise SystemExit(result.returncode)
    subprocess.run([str(output)], cwd=root, check=True)
for runner in ['compiler_checks.py', 'vm_semantics.py']:
    subprocess.run([sys.executable, str(root / 'tests/lp64/script' / runner),
                    '--compile-db', str(build / 'compile_commands.json'),
                    '--output', str(fixture_output / runner.removesuffix('.py'))],
                   cwd=root, check=True)
