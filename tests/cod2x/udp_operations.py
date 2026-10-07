#!/usr/bin/env python3
"""Exercise production UDP dispatch/registration under actual Release flags."""
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
parser.add_argument('--hardening', action='store_true')
parser.add_argument('--compile-engine', action='store_true', help='Also compile changed full translation units with unchanged Release flags')
args = parser.parse_args()
if args.baseline and args.compile_engine:
    parser.error('--baseline cannot be combined with --compile-engine')
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
rows = json.loads(args.compile_db.read_text())


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


failures = []
for target in ('cod2_macos', 'cod2_macos_ded'):
    path = 'src/PC/server_mp/sv_main_mp.c'
    row = next(row for row in rows if row['file'].endswith('/' + path)
               and target + '.dir/' in row.get('command', str(row.get('arguments', []))))
    original = row['file'][:-len('/' + path)]
    if args.compile_engine:
        for unit in ('sv_init_mp.c', 'sv_main_mp.c', 'sv_main_pc_mp.c'):
            engine = next(entry for entry in rows if entry['file'].endswith('/src/PC/server_mp/' + unit)
                          and target + '.dir/' in entry.get('command', str(entry.get('arguments', []))))
            argv_ = engine.get('arguments') or shlex.split(engine['command'])
            argv_ = [word.replace(original, str(ROOT)) for word in argv_]
            argv_[argv_.index('-o') + 1] = str(out / (target + '-' + unit + '.o'))
            if args.hardening:
                argv_.insert(1, '-DCOD2_FEATURE_NET_HARDENING=1')
            result = subprocess.run(argv_, cwd=ROOT, text=True, capture_output=True)
            (out / (target + '-' + unit + '.compile.log')).write_text(
                shlex.join(argv_) + '\n' + result.stdout + result.stderr)
            if result.returncode:
                print(result.stdout + result.stderr)
                raise SystemExit('Release engine compilation failed: ' + target + '/' + unit)
        print(target + ': three full engine translation units compiled with Release flags', flush=True)
    command = row.get('arguments') or shlex.split(row['command'])
    flags = [word.replace(original, str(ROOT)) for word in command[:command.index('-o')]
             if word not in ('-DNDEBUG', '-c')]
    flags += ['-UNDEBUG', '-I' + str(out), '-fsanitize=address,undefined',
              '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
    if args.hardening:
        flags += ['-DCOD2_FEATURE_NET_HARDENING=1']

    def source(path):
        return (subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT)
                if args.baseline else (ROOT / path).read_bytes())

    def active(path):
        return subprocess.run([*flags, '-E', '-P', '-iquote', str((ROOT / path).parent), '-x', 'c', '-'],
                              input=source(path), cwd=ROOT, capture_output=True, check=True).stdout.decode()

    init = active('src/PC/server_mp/sv_init_mp.c')
    globals_ = set(re.findall(r'^extern const dvar_t \*(\w+);', init, re.M))
    globals_ |= {'sv_punkbuster', 'sv_wwwDownload', 'sv_wwwBaseURL', 'sv_wwwDlDisconnected', 'nextmap'}
    (out / 'udp_globals.h').write_text(''.join('const dvar_t *' + name + ';\n' for name in sorted(globals_)))
    main = active(path)
    has_limiter = 'SV_Cod2xUdpAllow' in main
    prefix = ''
    if has_limiter:
        for header in ('cod2x_udp_operations.h', 'cod2x_udp_operations_impl.h'):
            (out / header).write_bytes(source('src/PC/server_mp/' + header))
        prefix = '#include "cod2x_udp_operations_impl.h"\n'
    (out / 'udp_source.h').write_text(prefix + function(init, 'SV_Init') +
        ''.join(function(main, name) for name in ('SVC_Status', 'SVC_Info', 'SV_ConnectionlessPacket',
                                                  'SV_PacketEvent', 'SV_RunFrame')) +
        function(active('src/PC/server_mp/sv_main_pc_mp.c'), 'SVC_RemoteCommand'))
    net = active('src/PC/win32/win_net.c')
    (out / 'udp_lan.h').write_text('static int numIP;\nstatic byte localIP[16][4];\n' + function(net, 'Sys_IsLANAddress'))
    objects = []
    for source_path, name in [(ROOT / 'tests/cod2x/udp_operations.c', 'fixture'),
                              (ROOT / 'src/PC/universal/dvar.c', 'dvar')]:
        obj = out / (target + '-' + name + '.o')
        result = subprocess.run([*flags, '-c', str(source_path), '-o', str(obj)], cwd=ROOT,
                                text=True, capture_output=True)
        (out / (target + '-' + name + '.compile.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            print(result.stdout + result.stderr)
            raise SystemExit('fixture compilation failed: ' + target)
        objects.append(str(obj))
    exe = out / target
    result = subprocess.run([flags[0], '-arch', 'arm64', '-fsanitize=address,undefined',
                             '-Wl,-dead_strip', *objects, '-o', str(exe)], capture_output=True, text=True)
    (out / (target + '-link.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout + result.stderr)
        raise SystemExit('fixture link failed: ' + target)
    result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True)
    (out / (target + '-result.log')).write_text(result.stdout + result.stderr)
    print(target + ': ' + result.stdout + result.stderr, flush=True)
    if result.returncode:
        failures.append(target)
if failures:
    raise SystemExit('Failed production UDP fixtures: ' + ', '.join(failures))
