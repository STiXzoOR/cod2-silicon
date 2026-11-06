#!/usr/bin/env python3
"""Run Mac-1.3-derived allocator, string table and animation services checks."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

FIXTURES = {
    'f1_services_test.c': ['reuse', 'force', 'restart', 'last-string', 'last-chain', 'allocator', 'literal-vector'],
    'f1_services_anim.c': ['anim-error', 'anim-lifetime'],
}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--case', choices=[case for cases in FIXTURES.values() for case in cases])
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
rows = json.loads(args.compile_db.read_text())
entry = next(e for e in rows if e['file'].endswith('/scr_stringlist.c')
             and 'cod2_macos.dir' in e.get('command', ''))
command = shlex.split(entry['command'])
flags = command[1:command.index('-o')]
flags = ['-UNDEBUG' if f == '-DNDEBUG' else f for f in flags]
args.output.mkdir(parents=True, exist_ok=True)
failed = False
for fixture, cases in FIXTURES.items():
    cases = [case for case in cases if not args.case or case == args.case]
    if not cases:
        continue
    binary = args.output / fixture.removesuffix('.c')
    sources = [str(root / 'src/PC/script/scr_main.c')] if fixture == 'f1_services_anim.c' else []
    result = subprocess.run([command[0], *flags, '-fsanitize=address,undefined',
                             '-fno-sanitize-recover=all', '-Wl,-dead_strip',
                             str(root / 'tests/lp64/script' / fixture), *sources, '-o', str(binary)],
                            cwd=root, text=True, capture_output=True)
    (args.output / (fixture + '.compile.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    for case in cases:
        try:
            result = subprocess.run([str(binary), case], cwd=root, text=True,
                                    capture_output=True, timeout=20)
            (args.output / (case + '.log')).write_text(result.stdout + result.stderr)
            print(case + ': ' + ('PASS' if result.returncode == 0 else 'FAIL'))
            failed |= result.returncode != 0
        except subprocess.TimeoutExpired:
            (args.output / (case + '.log')).write_text('Timed out after 20 seconds\n')
            print(case + ': TIMEOUT')
            failed = True
raise SystemExit(1 if failed else 0)
