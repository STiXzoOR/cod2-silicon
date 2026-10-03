#!/usr/bin/env python3
"""Run real compiler/scanner paths with ASan, using existing CMake client flags."""
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'build-macos/ws6-compiler'
OUT.mkdir(parents=True, exist_ok=True)
rows = json.loads((ROOT / 'build-macos/compile_commands.json').read_text())
row = next(r for r in rows if r['file'].endswith('/scr_compiler.c'))
flags = shlex.split(row['command'])
compiler = flags.pop(0)
del flags[flags.index('-o'):]
source = (ROOT / 'src/PC/script/scr_compiler.c').read_text()
sizes = re.findall(r'\(scr_block_t \*\*\)Hunk_AllocateTempMemoryHighInternal\(([^;]+)\);', source)
assert sizes
(OUT / 'compiler_allocations.h').write_text(
    'static const size_t compiler_child_array_sizes[] = {\n' + ',\n'.join(sizes) + '\n};\n')
failed = False
for fixture, cases in [
    ('compiler_behavior.c', ['identity', 'shutdown', 'error-position', 'child-arrays', 'builtin-cache']),
    ('compiler_scanner.c', ['scanner-buffer']),
    ('compiler_parser.c', ['parser-native-nodes']),
]:
    binary = OUT / fixture.removesuffix('.c')
    command = [compiler, *flags, '-I' + str(OUT), '-fsanitize=address',
               '-ffunction-sections', '-fdata-sections', '-Wl,-dead_strip',
               str(ROOT / 'tests/lp64/script' / fixture), '-o', str(binary)]
    build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    (OUT / (fixture + '.log')).write_text(build.stderr)
    if build.returncode:
        print(build.stderr)
        raise SystemExit(build.returncode)
    for case in cases:
        run = subprocess.run([str(binary), *([case] if fixture == 'compiler_behavior.c' else [])],
                             cwd=ROOT, capture_output=True, text=True)
        (OUT / (case + '.log')).write_text(run.stdout + run.stderr)
        print(f'{case}: ' + ('PASS' if run.returncode == 0 else f'FAIL ({run.returncode})'))
        failed |= bool(run.returncode)
raise SystemExit(failed)
