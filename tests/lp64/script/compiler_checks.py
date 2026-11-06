#!/usr/bin/env python3
"""Run real compiler/scanner paths using remapped CMake engine flags."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, default=ROOT / 'build-macos/compile_commands.json')
parser.add_argument('--output', type=Path, default=ROOT / 'build-macos/ws6-compiler')
parser.add_argument('--target', choices=['cod2_macos', 'cod2_macos_ded'], default='cod2_macos')
parser.add_argument('--compiler-source', type=Path, default=ROOT / 'src/PC/script/scr_compiler.c')
parser.add_argument('--parser-sanitizers', default='address,undefined')
parser.add_argument('cases', nargs='*')
args = parser.parse_args()
OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
rows = json.loads(args.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/src/PC/script/scr_compiler.c') and
           args.target + '.dir/' in (r.get('command') or ' '.join(r['arguments'])))
original = row['file'][:-len('/src/PC/script/scr_compiler.c')]
flags = row.get('arguments') or shlex.split(row['command'])
flags = [value.replace(original, str(ROOT)) for value in flags]
compiler = flags.pop(0)
del flags[flags.index('-o'):]
flags = [value for value in flags if value not in ('-c', '-DNDEBUG')]
flags.append('-UNDEBUG')
source = args.compiler_source.read_text()
sizes = re.findall(r'\(scr_block_t \*\*\)Hunk_AllocateTempMemoryHighInternal\(([^;]+)\);', source)
assert sizes
(OUT / 'compiler_allocations.h').write_text(
    'static const size_t compiler_child_array_sizes[] = {\n' + ',\n'.join(sizes) + '\n};\n')


def function(source, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError('missing production function: ' + name)
    depth = 0
    for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', source[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError('unterminated production function: ' + name)


values = subprocess.run(['unifdef', '-DCOD2_X64',
                         str(ROOT / 'src/PC/script/scr_variable.c')],
                        capture_output=True, text=True)
if values.returncode not in (0, 1):
    raise SystemExit(values.stderr)
values = values.stdout
scalar_helpers = '\n'.join(re.findall(r'^#define (?:SCRVL_\w+|VG_STATUS\(id\)).*', values, re.M)) + '\n'
scalar_helpers += ''.join(function(values, name) for name in
                          ['Scr_CastBool_core', 'Scr_CastBool', 'Scr_CastNumericPair', 'Scr_EvalEquality'])
(OUT / 'compiler_values.h').write_text(scalar_helpers)

fixtures = [
    ('compiler_behavior.c', ['identity', 'shutdown', 'error-position', 'child-arrays', 'builtin-cache'], 'address'),
    ('compiler_scanner.c', ['scanner-buffer'], 'address'),
    ('compiler_parser.c', ['parser-native-nodes'], args.parser_sanitizers),
    ('compiler_operands.c', ['builtin-method-odd', 'builtin-method-aligned',
                             'builtin-function-odd', 'builtin-function-aligned',
                             'literal-float', 'literal-integer', 'literal-string',
                             'literal-localized', 'literal-vector', 'canonical-local',
                             'integer-boundaries'], 'address,undefined'),
    ('compiler_loops.c', ['while-variable', 'while-true', 'while-float', 'while-binary',
                          'while-float-zero', 'while-binary-false',
                          'while-false', 'while-string', 'while-continue', 'while-nested',
                          'while-break-intersection', 'for-variable', 'for-true', 'for-omitted',
                          'for-binary', 'for-binary-false', 'for-false', 'for-string',
                          'loop-local-limit', 'loop-true-local-limit',
                          'for-literal-true', 'for-literal-false', 'while-literal-true',
                          'while-literal-false', 'parser-stack-growth', 'parser-bad-syntax',
                          'parser-unexpected-eof'], args.parser_sanitizers),
]
unknown_cases = set(args.cases) - {case for _, cases, _ in fixtures for case in cases}
if unknown_cases:
    parser.error('unknown cases: ' + ', '.join(sorted(unknown_cases)))

failed = False
for fixture, cases, sanitizer in fixtures:
    cases = [case for case in cases if not args.cases or case in args.cases]
    if not cases:
        continue
    binary = OUT / fixture.removesuffix('.c')
    command = [compiler, *flags, '-I' + str(OUT), '-fsanitize=' + sanitizer,
               '-fno-sanitize-recover=all',
               '-DSCR_COMPILER_SOURCE=' + json.dumps(str(args.compiler_source.resolve())),
               '-ffunction-sections', '-fdata-sections', '-Wl,-dead_strip',
               str(ROOT / 'tests/lp64/script' / fixture), '-o', str(binary)]
    build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    (OUT / (fixture + '.log')).write_text(build.stderr)
    if build.returncode:
        print(build.stderr[-4000:])
        raise SystemExit(build.returncode)
    for case in cases:
        run = subprocess.run([str(binary), *([case] if fixture in ('compiler_behavior.c', 'compiler_loops.c', 'compiler_operands.c') else [])],
                             cwd=ROOT, capture_output=True, text=True)
        (OUT / (case + '.log')).write_text(run.stdout + run.stderr)
        print(f'{case}: ' + ('PASS' if run.returncode == 0 else f'FAIL ({run.returncode})'))
        if run.returncode:
            print((run.stdout + run.stderr)[-3000:])
        failed |= bool(run.returncode)
raise SystemExit(failed)
