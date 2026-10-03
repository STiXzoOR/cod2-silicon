#!/usr/bin/env python3
"""Exercise retained VM helpers and the exact Object/JumpBack opcode bodies."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
build = root / 'build-macos'
output = build / 'ws6-vm-semantics'
output.mkdir(exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'src/PC/script/scr_vm.c')
parser.add_argument('cases', nargs='*')
args = parser.parse_args()
source_path = args.source.resolve()
source = source_path.read_text()
cases = []
for opcode in ('Object', 'JumpBack'):
    match = re.search(rf'        case VMOP_{opcode}: \{{\n(.*?)'
                      r'(?=        case VMOP_)', source, re.S)
    if not match:
        raise SystemExit(f'cannot locate real VMOP_{opcode} case')
    cases.append(f'/* Extracted without rewriting from scr_vm.c. */\n'
                 f'        case VMOP_{opcode}: {{\n{match[1]}')
(output / 'vm_semantics_cases.inc').write_text('\n'.join(cases))
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in commands if e['file'].endswith('/scr_vm.c')
             and 'cod2_macos.dir' in e['command'])
argv = shlex.split(entry['command'])
flags = argv[1:argv.index('-o')]
binary = output / 'test'
result = subprocess.run([argv[0], *flags, '-fsanitize=address,undefined',
                         '-fno-sanitize-recover=all', '-Wl,-dead_strip', '-I' + str(output),
                         '-I' + str(root / 'src/PC/script'),
                         '-DVM_SEMANTICS_SOURCE=' + json.dumps(str(source_path)),
                         'tests/lp64/script/vm_semantics.c', '-o', str(binary)],
                        cwd=root, text=True, capture_output=True)
(output / 'compile.log').write_text(result.stdout + result.stderr)
if result.returncode:
    print(result.stdout + result.stderr, file=sys.stderr)
    raise SystemExit(result.returncode)
names = args.cases or ['root_return', 'thread_return', 'child_return',
                        'object', 'ring', 'timeout_before', 'timeout_kill',
                        'timeout_parents', 'timeout_thread', 'timeout_loading',
                        'timeout_terminal', 'timeout_terminal_vm', 'timeout_wrap',
                        'reset_clock', 'unaligned']
failed = False
for name in names:
    result = subprocess.run([str(binary), name], cwd=root, text=True,
                            capture_output=True)
    (output / (name + '.log')).write_text(result.stdout + result.stderr)
    print(f'{name}: {"PASS" if result.returncode == 0 else "FAIL"}')
    if result.returncode:
        failed = True
        print(result.stdout + result.stderr, file=sys.stderr)
raise SystemExit(int(failed))
