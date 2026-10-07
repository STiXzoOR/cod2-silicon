#!/usr/bin/env python3
"""Exercise production level commands and teardown using actual engine flags."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline')
parser.add_argument('--dedicated', action='store_true')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
rows = json.loads(args.compile_db.read_text())
target = 'cod2_macos_ded.dir' if args.dedicated else 'cod2_macos.dir'
path = 'src/PC/server_mp/sv_init_mp.c'
row = next(r for r in rows if r['file'].endswith('/' + path) and target in str(r))
old_root = row['file'][:-len('/' + path)]
argv = row.get('arguments') or shlex.split(row['command'])
flags = [s.replace(old_root, str(ROOT)) for s in argv[:argv.index('-o')] if s != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']


def source(path):
    if args.baseline:
        return subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT)
    return (ROOT / path).read_bytes()


def active(path):
    return subprocess.run([*flags, '-E', '-P', '-iquote', str((ROOT / path).parent), '-x', 'c', '-'],
                          input=source(path), cwd=ROOT, capture_output=True, check=True).stdout.decode()


def function(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError('Missing production function ' + name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()] + '\n'
    raise ValueError('Unterminated production function ' + name)


init = active(path)
globals_ = sorted(set(re.findall(r'^extern const dvar_t \*(\w+);', init, re.M)))
(out / 'stop_globals.h').write_text(''.join('const dvar_t *' + n + ';\n' for n in globals_))
game = active('src/PC/game_mp/g_scr_main_mp.c')
ccmd = active('src/PC/server_mp/sv_ccmds_mp.c')
implemented = 'GScr_Cod2xBeforeMapChange' in game
(out / 'stop_features.h').write_text('#define STOP_CALLBACK_IMPLEMENTED ' + str(int(implemented)) + '\n')
for text, names, header in [
    (game, ['GScr_LoadScriptFunction', 'GScr_LoadGameTypeScript', 'GScr_FreeScripts',
            'GScr_MapRestart', 'GScr_LoadMap', 'GScr_ExitLevel'], 'stop_game_source.h'),
    (ccmd, ['SV_ImportedDvar', 'SV_ConsumeMapRotationToken', 'SV_MapRestart', 'SV_MapRestart_f', 'SV_FastRestart_f',
            'SV_Map_f', 'SV_MapRotate_f'], 'stop_commands_source.h'),
    (init, ['SV_Init', 'SV_InitDvar', 'SV_InitArchivedSnapshot', 'SV_Shutdown', 'SV_SpawnServer'],
     'stop_server_source.h'),
    (active('src/PC/game_mp/g_main_mp.c'), ['G_GetSavePersist', 'G_SetSavePersist', 'ExitLevel',
                                        'G_ShutdownGame'], 'stop_main_source.h'),
    (active('src/PC/qcommon/cmd.c'), ['Cbuf_Init', 'Cbuf_AddTextInternal', 'Cbuf_AddText',
                                   'Cbuf_InsertText', 'Cbuf_ExecuteText', 'Cbuf_ExecuteInternal',
                                   'Cbuf_Execute'], 'stop_buffer_source.h'),
]:
    prefix = ''
    if implemented and header == 'stop_game_source.h':
        prefix = '\n'.join(re.findall(r'^static .*\bcod2xStop\w*[^\n]*;', text, re.M)) + '\n'
        names = ['GScr_Cod2xResetMapChange', 'GScr_Cod2xCallStopGameType', 'GScr_Cod2xBeforeMapChange',
                 'GScr_Cod2xConsumeMapChange', 'GScr_Cod2xQueueMapChange', 'GScr_Cod2xQueueCommand',
                 'GScr_Cod2xQueueExitLevel', *names]
    if implemented and header == 'stop_commands_source.h':
        prefix = '\n'.join(re.findall(r'^static .*\bcod2xMapChangeDepth[^\n]*;', text, re.M)) + '\n'
        names = ['SV_Cod2xBeginMapChangeApproved', 'SV_Cod2xBeginMapChange', 'SV_Cod2xEndMapChange', *names]
    (out / header).write_text(prefix + ''.join(function(text, name) for name in names))
objects = []
for path, name in [(ROOT / 'tests/cod2x/stop_gametype.c', 'fixture'),
                   (ROOT / 'src/PC/qcommon/cod2x_runtime.c', 'runtime'),
                   (ROOT / 'src/PC/universal/dvar.c', 'dvar')]:
    obj = out / (name + '.o')
    with (out / (name + '.compile.log')).open('w') as log:
        subprocess.run([*flags, '-I' + str(ROOT / 'src/PC/qcommon'), '-c', str(path), '-o', str(obj)],
                       cwd=ROOT, stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'stop_gametype'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
                *objects, '-o', str(exe)], check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True)
(out / 'run.log').write_text(result.stdout + result.stderr)
print(result.stdout, end='')
if result.returncode:
    print('\n'.join(result.stderr.splitlines()[-8:]))
raise SystemExit(result.returncode)
