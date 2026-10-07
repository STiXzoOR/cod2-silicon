#!/usr/bin/env python3
"""Run native host/client lifecycle bodies with real dvars and synthetic services."""
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
parser.add_argument('--baseline', help='Read production sources from a local Git commit')
parser.add_argument('--dedicated', action='store_true')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
entries = json.loads(args.compile_db.read_text())
source_path = 'src/PC/server_mp/sv_init_mp.c'
target = 'cod2_macos_ded.dir' if args.dedicated else 'cod2_macos.dir'
entry = next(row for row in entries if row['file'].endswith('/' + source_path)
             and target in row.get('command', str(row.get('arguments', []))))
old_root = entry['file'][:-len('/' + source_path)]
command = entry.get('arguments') or shlex.split(entry['command'])
flags = command[:command.index('-o')]
flags = [word.replace(old_root, str(ROOT)) for word in flags if word != '-DNDEBUG']
flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
if args.dedicated:
    flags += ['-DDEDICATED=1']


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


init = active(source_path)
commands = active('src/PC/server_mp/sv_ccmds_mp.c')
has_map_change_guard = 'SV_Cod2xBeginMapChangeApproved' in commands
map_change_source = ''
if has_map_change_guard:
    map_change_source = re.search(r'^static unsigned int cod2xMapChangeDepth;', commands, re.M)[0] + '\n'
    map_change_source += ''.join(function(commands, name) for name in
                               ('SV_Cod2xBeginMapChangeApproved', 'SV_Cod2xBeginMapChange', 'SV_Cod2xEndMapChange'))
(out / 'host_map_change_source.h').write_text(map_change_source)
globals_ = sorted(set(re.findall(r'^extern const dvar_t \*(\w+);', init, re.M)))
(out / 'host_globals.h').write_text(
    '#define HOST_HAS_MAP_CHANGE_GUARD ' + str(int(has_map_change_guard)) + '\n' +
    '#define HOST_HAS_IDENTITY_CLEANUP ' + str(int('SV_Cod2xClearIdentities();' in function(init, 'SV_Shutdown'))) + '\n' +
    ''.join('const dvar_t *' + name + ';\n' for name in globals_))
for path, names, output in [
    (source_path, ['SV_Init', 'SV_InitDvar', 'SV_InitArchivedSnapshot', 'SV_Shutdown', 'SV_SpawnServer'], 'host_server_source.h'),
    ('src/PC/server_mp/sv_client_mp.c', ['SV_ClientEnterWorld'], 'host_begin_source.h'),
    ('src/PC/server_mp/sv_ccmds_mp.c', ['SV_ImportedDvar', 'SV_MapRestart'], 'host_restart_source.h'),
    ('src/PC/client_mp/cl_parse_mp.c', ['CL_SystemInfoChanged'], 'host_systeminfo_source.h'),
    ('src/PC/cgame_mp/cg_servercmds_mp.c', ['CG_SetClientDvarFromServer'], 'host_reliable_source.h'),
    ('src/PC/universal/q_shared.c', ['I_strnicmp_core', 'Info_ValueForKey', 'Info_NextPair'], 'host_info_source.h'),
]:
    text = init if path == source_path else active(path)
    if output == 'host_begin_source.h' and re.search(r'^.*\bSV_Cod2xClientBegin\([^;{]*\)\s*\{', text, re.M):
        names = ['SV_Cod2xClientBegin', *names]
        # Every native begin path must use the production publishing boundary.
        assert len(re.findall(r'\bSV_Cod2xClientBegin\([^;{}]*\);', text)) == 4
        assert len(re.findall(r'\bClientBegin\([^;{}]*\);', text)) == 2  # declaration + helper
    prefix = ''
    if output == 'host_server_source.h' and 'SV_Cod2xUdpInit();' in text:
        for header in ('cod2x_udp_operations.h', 'cod2x_udp_operations_impl.h'):
            (out / header).write_bytes(source('src/PC/server_mp/' + header))
        prefix = '#include "cod2x_udp_operations_impl.h"\n'
    (out / output).write_text(prefix + ''.join(function(text, name) for name in names))
(out / 'cod2x_runtime.c').write_bytes(source('src/PC/qcommon/cod2x_runtime.c'))
(out / 'cod2x.h').write_bytes(source('src/PC/qcommon/cod2x.h'))
objects = []
for path, name in [(ROOT / 'tests/cod2x/host_lifecycle.c', 'fixture'),
                   (out / 'cod2x_runtime.c', 'runtime'),
                   (ROOT / 'src/PC/universal/dvar.c', 'dvar')]:
    obj = out / (name + '.o')
    with (out / (name + '.compile.log')).open('w') as log:
        subprocess.run([*flags, '-I' + str(ROOT / 'src/PC/qcommon'), '-c', str(path), '-o', str(obj)],
                       cwd=ROOT, stdout=log, stderr=log, check=True)
    objects.append(str(obj))
exe = out / 'host_lifecycle'
subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
