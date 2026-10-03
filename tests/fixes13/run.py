#!/usr/bin/env python3
"""Build isolated tests from production C fragments; no game data or engine link needed."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build/ws9')
parser.add_argument('--test', action='append')
parser.add_argument('--baseline', help='extract production code from this local git commit')
args = parser.parse_args()
out = args.build.resolve() / 'fixes13-tests'
out.mkdir(parents=True, exist_ok=True)
entries = json.loads((args.build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/src/PC/server_mp/sv_init_mp.c'))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [f for f in flags if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-ffunction-sections', '-fdata-sections', '-I' + str(out),
          '-fsanitize=address', '-fno-omit-frame-pointer']
preprocessed = {}
ABI_SOURCES = ['src/PC/script/scr_compiler.c', 'src/PC/client_mp/cl_main_pc_mp.c',
               'src/PC/gfx_d3d/r_scene.c', 'src/PC/gfx_d3d/r_staticmodel_load_obj.c',
               'src/PC/cgame_mp/cg_main_mp.c', 'src/PC/win32/gfx_v60_threads.c',
               'src/PC/client_mp/cl_console_mp.c', 'src/PC/qcommon/net_chan_mp.c',
               'src/PC/EffectsCore/FxUtil.c', 'src/PC/script/scr_debugger_ui_lists.c',
               'src/PC/script/scr_debugger_ui_watch.c']


def function(path, name):
    if path not in preprocessed:
        source = subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], text=True, cwd=ROOT) if args.baseline else (ROOT / path).read_text()
        preprocessed[path] = subprocess.run(
            [*flags, '-iquote', str((ROOT / path).parent), '-E', '-P', '-x', 'c', '-'],
            input=source, capture_output=True, text=True, cwd=ROOT, check=True).stdout
    source = preprocessed[path]
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError(f'function {name} missing in {path}')
    # Ignore braces inside comments and string literals.
    tokens = re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', source[match.start():])
    depth = 0
    for token in tokens:
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError('unterminated function')


def run(name, generated, sources=()):
    if args.test and name not in args.test:
        return
    (out / f'{name}_source.h').write_text(generated)
    objects = []
    for index, source in enumerate([*sources, f'tests/fixes13/{name}.c']):
        obj = out / f'{name}_{index}.o'
        with (out / f'{name}_{index}.log').open('w') as log:
            result = subprocess.run([*flags, '-c', str(ROOT / source), '-o', str(obj)],
                                    cwd=ROOT, stdout=log, stderr=log)
        if result.returncode:
            print((out / f'{name}_{index}.log').read_text())
            result.check_returncode()
        objects.append(str(obj))
    exe = out / name
    subprocess.run(['clang', '-arch', 'arm64', '-fsanitize=address', '-Wl,-dead_strip',
                    *objects, '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print(f'PASS {name}', flush=True)


if not args.test or 'receive' in args.test:
    run('receive', function('src/Mac/Main/mac_main.c', 'Sys_GetEvent'), ['src/blobs/bss.c'])
if not args.test or 'snapshot' in args.test:
    path = 'src/PC/server_mp/sv_snapshot_mp.c'
    run('snapshot', ''.join(function(path, n) for n in
        ['SV_WriteOverflowRecoveryCommandsLocal', 'SV_SendClientSnapshot']))
if args.test and 'abi' in args.test:
    for path in ABI_SOURCES:
        subprocess.run([*flags, '-include', 'stdlib.h', '-include', 'string.h',
                        '-fsyntax-only', str(ROOT / path)], check=True,
                       stdout=subprocess.DEVNULL, stderr=(out / (Path(path).stem + '_abi.log')).open('w'))
    run('abi', '')
if not args.test or 'switch' in args.test:
    path = 'src/PC/script/scr_compiler.c'
    source = function(path, 'EmitSwitchStatement')
    start = source.index('if (numCases > 1)')
    end = source.index('nextCodePos = TempMalloc(0);', start)
    generated = function(path, 'CompareCaseInfo') + '''
static void check_cases(unsigned int (*caseTable)[2], int numCases)
{
    int i;
    CaseStatementInfo *caseStatement;
    qsort(caseTable, numCases, 8, (int (*)(const void *, const void *))CompareCaseInfo);
''' + source[start:end].replace('return 0;', 'return;') + '}\n'
    run('switch', generated)
