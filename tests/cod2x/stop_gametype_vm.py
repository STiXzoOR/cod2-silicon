#!/usr/bin/env python3
"""Load, compile, resolve and execute synthetic StopGameType GSC in the real VM."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline', help='Read game callback/entry bodies from a local Git commit')
p.add_argument('--ubsan', action='store_true', help='Also expose retained full compiler/parser/VM UBSan limitations')
p.add_argument('--dedicated', action='store_true')
p.add_argument('--compiler-source', type=Path, help='Use an explicitly supplied private compiler TU; other script sources remain local')
args = p.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
rows = json.loads(args.compile_db.read_text())
target = 'cod2_macos_ded.dir' if args.dedicated else 'cod2_macos.dir'
row = next(r for r in rows if r['file'].endswith('/src/PC/script/scr_vm.c') and target in str(r))
old_root = row['file'][:-len('/src/PC/script/scr_vm.c')]
argv = row.get('arguments') or shlex.split(row['command'])
flags = [s.replace(old_root, str(ROOT)) for s in argv[:argv.index('-o')] if s != '-DNDEBUG']
sanitizers = 'address,undefined' if args.ubsan else 'address'
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=' + sanitizers, '-fno-omit-frame-pointer',
          '-ffunction-sections', '-fdata-sections']
if args.ubsan:
    flags += ['-fno-sanitize-recover=all']
game_path = 'src/PC/game_mp/g_scr_main_mp.c'
game_source = (subprocess.check_output(['git', 'show', args.baseline + ':' + game_path], cwd=ROOT, text=True)
               if args.baseline else (ROOT / game_path).read_text())
game = subprocess.run([*flags, '-E', '-P', '-x', 'c', '-'], input=game_source,
                      capture_output=True, text=True, check=True).stdout


def function(text, name):
    m = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    if not m:
        raise ValueError('Missing production function ' + name)
    depth = 0
    for t in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[m.start():]):
        if t[0] == '{':
            depth += 1
        elif t[0] == '}':
            depth -= 1
            if not depth:
                return text[m.start():m.start() + t.end()] + '\n'


names = ['GScr_MapRestart', 'GScr_LoadMap', 'GScr_ExitLevel',
         'GScr_LoadScriptFunction', 'GScr_LoadGameTypeScript', 'Scr_RandomInt']
implemented = 'GScr_Cod2xBeforeMapChange' in game
prefix = '#define STOP_VM_CALLBACK_IMPLEMENTED ' + str(int(implemented)) + '\n'
(out / 'stop_vm_features.h').write_text(prefix)
if 'GScr_Cod2xBeforeMapChange' in game:
    prefix += '\n'.join(re.findall(r'^static .*\bcod2xStop\w*[^\n]*;', game, re.M)) + '\n'
    names = ['GScr_Cod2xResetMapChange', 'GScr_Cod2xCallStopGameType', 'GScr_Cod2xBeforeMapChange',
             'GScr_Cod2xConsumeMapChange', 'GScr_Cod2xQueueMapChange', 'GScr_Cod2xQueueCommand', 'GScr_Cod2xQueueExitLevel',
             *names]
(out / 'stop_vm_game.h').write_text(prefix + ''.join(function(game, n) for n in names))
errors = ''
for path, name in [('src/PC/qcommon/common.c', 'Com_Error'),
                   ('src/PC/script/scr_parser.c', 'RuntimeError')]:
    text = subprocess.run([*flags, '-iquote', str((ROOT / path).parent), '-E', '-P', '-x', 'c', '-'], input=(ROOT / path).read_text(),
                          capture_output=True, text=True, check=True).stdout
    errors += function(text, name)
(out / 'stop_vm_errors.h').write_text(errors)
objects = []
for path in ['tests/cod2x/stop_gametype_vm.c', 'src/PC/script/scr_main.c',
             'src/PC/script/scr_compiler.c', 'src/PC/script/scr_yacc.c',
             'src/PC/script/scr_parsetree.c', 'src/PC/script/scr_vm.c',
             'src/PC/script/scr_variable.c', 'src/PC/script/scr_stringlist.c',
             'src/PC/script/scr_memorytree.c', 'src/PC/script/scr_tempmemory.c']:
    obj = out / (Path(path).stem + '.o')
    with (out / (Path(path).stem + '.compile.log')).open('w') as log:
        source = args.compiler_source.resolve() if path == 'src/PC/script/scr_compiler.c' and args.compiler_source else ROOT / path
        subprocess.run([*flags, '-c', str(source), '-o', str(obj)], stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'stop_gametype_vm'
link = subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=' + sanitizers, '-Wl,-dead_strip',
                       *objects, '-o', str(exe)], capture_output=True, text=True)
(out / 'link.log').write_text(link.stdout + link.stderr)
if link.returncode:
    print(link.stderr)
    raise SystemExit(link.returncode)
run = subprocess.run([str(exe)], capture_output=True, text=True)
(out / 'run.log').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
