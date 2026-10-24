#!/usr/bin/env python3
"""Instrument a private cod2engine 1.3 copy using this repository's own hooks.

Run inside the guest. The source clone is read only; destination and patch must
be new private paths. No reference code is embedded in this script.
"""
import argparse
import difflib
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)


def function_bounds(text, name):
    matches = list(re.finditer(r'\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', text))
    if len(matches) != 1:
        raise ValueError(f'Expected one definition of {name}, found {len(matches)}')
    start = matches[0].end()
    depth = 1
    for token in TOKEN.finditer(text, start):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return start, token.start()
    raise ValueError(f'Unclosed function {name}')


def instrument(source, destination, patch):
    if destination.exists() or patch.exists():
        raise ValueError('Destination and patch must be new paths')
    if source == destination or source in destination.parents:
        raise ValueError('Destination must be outside the reference source')
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
    changes = []

    def update(relative, transform):
        path = destination / relative
        before = path.read_text()
        after = transform(before)
        path.write_text(after)
        changes.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                           fromfile='a/' + relative, tofile='b/' + relative))

    def start_hook(text, name, code):
        start, _ = function_bounds(text, name)
        return text[:start] + '\n' + code + '\n' + text[start:]

    declarations = ('\nint Sys_DiffSeed(int);\nvoid Sys_StateHashFrame(void);\n'
                    'void Sys_DiffInputFrame(void);\nint Sys_DiffBotMove(void);\n'
                    'int Sys_DiffClientThink(int, const usercmd_t *);\n')

    def prelude(text):
        # Place declarations after the first include (which supplies usercmd_t).
        match = re.search(r'^#include[^\n]*\n', text, re.M)
        if not match:
            raise ValueError('Missing initial include')
        return text[:match.end()] + declarations + text[match.end():]

    update('src/server/sv_client_mp.cpp', lambda text: prelude(start_hook(
        text, 'SV_ClientThink', '    if (!Sys_DiffClientThink((int)(cl - svs.clients), cmd)) return;')))

    def server_hooks(text):
        text = start_hook(text, 'SV_BotUserMove', '    if (Sys_DiffBotMove()) return;')
        start, end = function_bounds(text, 'SV_RunFrame')
        body, count = re.subn(r'\bG_RunFrame\s*\([^;]*\)\s*;',
                              lambda match: 'Sys_DiffInputFrame();\n    ' + match.group(),
                              text[start:end])
        if count != 1:
            raise ValueError('Expected one game frame call in SV_RunFrame')
        text = text[:start] + body + text[end:]
        start, end = function_bounds(text, 'SV_Frame')
        body = text[start:end]
        body, count = re.subn(r'\bScr_SetLoading\s*\([^;]*\)\s*;',
                             lambda match: match.group() + '\n        Sys_StateHashFrame();', body)
        if count != 1:
            raise ValueError('Expected one loading-state call in SV_Frame')
        return prelude(text[:start] + body + text[end:])

    update('src/server/sv_main_mp.cpp', server_hooks)

    def common_hook(text):
        _, end = function_bounds(text, 'Com_Frame')
        return prelude(text[:end] + '\n    Sys_StateHashFrame();\n' + text[end:])

    update('src/qcommon/common.cpp', common_hook)
    update('src/game/g_main_mp.cpp', lambda text: prelude(start_hook(
        text, 'G_InitGame', '    randomSeed = Sys_DiffSeed(randomSeed);')))

    hook = subprocess.run(['unifdef', '-DCOD2_X64', str(ROOT / 'src/unix/sysdiff_statehash.c')],
                          capture_output=True, text=True)
    if hook.returncode not in (0, 1):
        raise ValueError(hook.stderr)
    own_code = hook.stdout.replace('#include "common_types.h"', '#include "../qcommon/qcommon.h"')
    own_code = re.sub(r'^extern (?:struct serverStatic_t|level_locals_t|gentity_t)[^\n]*\n',
                      '', own_code, flags=re.M)
    own_code = own_code.replace('_Exit(', '_exit(')
    workload = (ROOT / 'tools/oracle/live_input.inc').read_text()
    own_code, count = re.subn(
        r'            cmd\.angles\[1\] = .*?            cmd\.buttons = [^;]*;',
        lambda match: '\n'.join('            ' + line for line in workload.splitlines()),
        own_code, flags=re.S)
    if count != 1:
        raise ValueError('Expected one recording workload in our hook')
    path = destination / 'src/oracle/sysdiff_statehash.cpp'
    path.parent.mkdir()
    path.write_text(own_code)
    changes.extend(difflib.unified_diff([], own_code.splitlines(True), fromfile='/dev/null',
                                       tofile='b/src/oracle/sysdiff_statehash.cpp'))

    def make_hook(text):
        text, count = re.subn(r'^SRCS := \\$',
                             lambda match: match.group() + '\n\toracle/sysdiff_statehash.cpp \\',
                             text, flags=re.M)
        if count != 1:
            raise ValueError('Expected one Makefile source list')
        return text

    update('Makefile', make_hook)
    patch.write_text(''.join(changes))
    print(f'Instrumented private copy: {destination}; private patch: {patch}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='archive/lnxded-1.3')
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--patch', type=Path, required=True)
    args = parser.parse_args()
    instrument(args.source.resolve(), args.destination.resolve(), args.patch.resolve())
