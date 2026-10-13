#!/usr/bin/env python3
"""Check the production scoreboard border buffer and emitted rectangles under ASan."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline', help='Read production code from a local Git revision')
args = parser.parse_args()
output = args.output.resolve()
if output == ROOT or ROOT in output.parents:
    parser.error('--output must be outside the checkout')
output.mkdir(parents=True, exist_ok=True)
path = 'src/PC/cgame_mp/cg_scoreboard_mp.c'
row = next(row for row in json.loads(args.compile_commands.read_text())
           if row['file'].endswith('/' + path))
old_root = row['file'][:-len(path)].rstrip('/')
command = row.get('arguments') or shlex.split(row['command'])
flags, skip = [], False
for part in command:
    part = part.replace(old_root, str(ROOT))
    if skip:
        skip = False
    elif part in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif part not in ('-c', '-MD', '-MMD', '-DNDEBUG', str(ROOT / path)):
        flags.append(part)
source = (subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT,
                                 text=True) if args.baseline else (ROOT / path).read_text())
active = subprocess.run([*flags, '-iquote', str((ROOT / path).parent), '-E', '-P',
                         '-x', 'c', '-'], input=source, cwd=ROOT, check=True,
                        capture_output=True, text=True).stdout


def function(name):
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', active, re.M)
    if not match:
        raise ValueError('Missing production function ' + name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                            active[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return active[match.start():match.start() + token.end()] + '\n'
    raise ValueError('Unterminated production function ' + name)


scoreboard = function('CG_DrawScoreboard')
declaration = re.search(r'sbpicinfo_t borderLines\[[^]]+\];', scoreboard)[0]
start = scoreboard.index('    {\n        const vec_t *cw = (const vec_t *)imp_colorWhite;')
end = scoreboard.index('\n    gameType =', start)
generated = function('CalcBorderLines') + '\nstatic void draw_borders(float borderAlpha)\n{\n'
generated += '    ' + declaration + '\n    int numLines;\n    Material *material;\n    float color[4];\n'
generated += scoreboard[start:end] + '\n}\n'
(output / 'scoreboard_source.h').write_text(generated)
sanitize = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer']
exe = output / 'scoreboard'
with (output / 'compile.log').open('w') as log:
    subprocess.run([*flags, *sanitize, '-I' + str(output), str(ROOT / 'tests/hud/scoreboard.c'),
                    '-o', str(exe)], cwd=ROOT, stdout=log, stderr=log, check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True, cwd=output)
(output / 'cases.log').write_text(result.stdout + result.stderr)
(output / 'results.json').write_text(json.dumps(dict(baseline=args.baseline,
    status='pass' if result.returncode == 0 else 'fail', returncode=result.returncode), indent=2) + '\n')
print(result.stdout + result.stderr, end='', flush=True)
raise SystemExit(bool(result.returncode))
