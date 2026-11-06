#!/usr/bin/env python3
"""Run isolated compiler regressions with Mac 1.3-derived expectations."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, default=ROOT / 'build-macos/compile_commands.json')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--compiler-source', type=Path, default=ROOT / 'src/PC/script/scr_compiler.c')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
view = subprocess.run(['unifdef', '-DCOD2_X64', '-D__aarch64__', str(args.compiler_source)], capture_output=True, text=True)
if view.returncode not in (0, 1):
    raise SystemExit(view.stderr)
source = re.sub(r'#    if 0[\s\S]*?#    endif', '', view.stdout)

def function(name, text=source):
    match = re.search(r'^(?:static [^\n]*\n)?[ \t]*(?:(?:static|void|int|char)[^\n]*\b)?' + name + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError(name)
    depth = 0
    for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()] + '\n'
    raise ValueError('unterminated ' + name)

names = ['EvalPrimitiveExpression', 'EvalBinaryOperatorExpression',
         'EmitPrepareDeveloperCall', 'EmitFinishDeveloperCall',
         'Scr_MergeBranchChildBlocks', 'ScriptStatementListFirstExecNode']
header = '\n'.join(function(name) for name in names)
statement = function('EmitStatement')
match = re.search(r'    case 0x38: \{([\s\S]*?)\n    case 0x39:', statement)
assert match
header += '\nstatic void emit_waitmatch(sval_t *node, scr_block_t *block)\n{\n'
header += '    VariableCompileValue constValue;\n' + match[1].rsplit('}', 1)[0].replace('return 0;', 'return;') + '\n}\n'
match = re.search(r'    case 0x3a: \{([\s\S]*?)\n    case 0x3b:', statement)
assert match
header += '\nstatic void emit_notify(sval_t *node, scr_block_t *block)\n{\n'
header += '    VariableCompileValue constValue;\n' + match[1].rsplit('}', 1)[0].replace('return 0;', 'return;') + '\n}\n'
parser_source = (ROOT / 'src/PC/script/scr_parser.c').read_text()
header += function('RuntimeError', parser_source)
(out / 'f1_compiler_under_test.h').write_text(header)
rows = json.loads(args.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/scr_compiler.c') and
           'cod2_macos.dir/' in (r.get('command') or ' '.join(r['arguments'])))
flags = row.get('arguments') or shlex.split(row['command'])
origin = row['file'][:-len('/src/PC/script/scr_compiler.c')]
flags = [value.replace(origin, str(ROOT)) for value in flags]
compiler = flags.pop(0)
del flags[flags.index('-o'):]
flags = [value for value in flags if value != '-DNDEBUG']
command = [compiler, *flags, '-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
           '-fno-sanitize-recover=all', str(ROOT / 'tests/lp64/script/f1_compiler.c'),
           '-o', str(out / 'f1_compiler')]
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
(out / 'build.log').write_text(result.stdout + result.stderr)
if result.returncode:
    print(result.stderr[-5000:])
    raise SystemExit(result.returncode)
failed = False
for case in ['developer-ignore', 'developer-enabled', 'developer-nested',
             'vector-fold', 'negative-minimum', 'branch-public', 'waitmatch-zero', 'waitmatch-two',
             'notify-one', 'notify-two', 'runtime-dialog-empty', 'runtime-dialog-null']:
    result = subprocess.run([str(out / 'f1_compiler'), case], capture_output=True, text=True)
    (out / (case + '.log')).write_text(result.stdout + result.stderr)
    print(case + ': ' + ('PASS' if not result.returncode else 'FAIL'))
    if result.returncode:
        print((result.stdout + result.stderr)[-2000:])
    failed |= bool(result.returncode)
scanner_command = [compiler, *flags, '-UNDEBUG', '-fsanitize=address,undefined',
                   '-fno-sanitize-recover=all', '-ffunction-sections', '-fdata-sections',
                   '-Wl,-dead_strip', str(ROOT / 'tests/lp64/script/f1_compiler_scanner.c'),
                   '-o', str(out / 'f1_compiler_scanner')]
result = subprocess.run(scanner_command, cwd=ROOT, capture_output=True, text=True)
(out / 'scanner-build.log').write_text(result.stdout + result.stderr)
if result.returncode:
    print(result.stderr[-5000:])
    raise SystemExit(result.returncode)
for case in ['scanner-events', 'scanner-eof-token', 'scanner-last-match', 'scanner-no-fill', 'scanner-capacity', 'scanner-nul',
             'scanner-oom-buffer', 'scanner-oom-chars']:
    try:
        result = subprocess.run([str(out / 'f1_compiler_scanner'), case], capture_output=True,
                                text=True, timeout=5)
        log = result.stdout + result.stderr
        success = (result.returncode == 2 and log == 'out of dynamic memory in yy_create_buffer()\n') \
            if case.startswith('scanner-oom-') else result.returncode == 0
    except subprocess.TimeoutExpired:
        log = 'scanner exceeded five-second timeout\n'
        success = False
    (out / (case + '.log')).write_text(log)
    print(case + ': ' + ('PASS' if success else 'FAIL'))
    if not success:
        print(log[-2000:])
    failed |= not success
raise SystemExit(failed)
