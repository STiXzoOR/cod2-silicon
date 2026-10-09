#!/usr/bin/env python3
"""Reference-contract fixtures from active engine sources; no game or display needed."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', help='Read production sources from a local Git revision')
p.add_argument('--test', action='append', choices=['tokenizer', 'dvars', 'registrations', 'profiles', 'commands', 'language'])
a = p.parse_args()
out = a.build.resolve() / ('fidelity-baseline' if a.baseline else 'fidelity-tests')
out.mkdir(parents=True, exist_ok=True)
entries = json.loads((a.build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/src/PC/qcommon/cmd.c') and 'cod2_macos.dir' in e['command'])
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags = [f for f in flags if f != '-DNDEBUG'] + ['-UNDEBUG', '-I' + str(out),
    '-I' + str(ROOT / 'src/PC/qcommon'), '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
cache = {}


def source(path):
    return subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()


def active(path):
    if path not in cache:
        cache[path] = subprocess.run([*flags, '-E', '-P', '-iquote', str((ROOT / path).parent), '-x', 'c', '-'],
            input=source(path), cwd=ROOT, capture_output=True, check=True).stdout.decode()
    return cache[path]


def function(path, name):
    text = active(path)
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    if not match:
        raise ValueError('Missing function ' + name)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.start() + token.end()] + '\n'
    raise ValueError('Unterminated function ' + name)


def declaration(path, name):
    return re.search(r'^(?!extern\b)[\w *]+\b' + name + r'\s*(?:\[[^]]*\])?\s*(?:=[\s\S]*?)?;', active(path), re.M)[0] + '\n'


failures = []


def run(name, generated, sources=()):
    if a.test and name not in a.test:
        return
    (out / (name + '_source.h')).write_text(generated)
    extra = []
    for path in sources:
        copy = out / Path(path).name
        copy.write_bytes(source(path))
        extra.append(str(copy))
    exe = out / name
    result = subprocess.run([*flags, '-Wl,-dead_strip', str(ROOT / 'tests/fidelity' / (name + '.c')),
        *extra, '-o', str(exe)], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
    result = subprocess.run([str(exe)], cwd=ROOT)
    if result.returncode:
        failures.append(name)


run('tokenizer', function('src/PC/qcommon/cmd.c', 'Cmd_TokenizeStringInternal'), ['src/PC/qcommon/cod2x_policy.c'])
run('dvars', ''.join(function('src/PC/qcommon/dvar_cmds.c', n) for n in
    ['Dvar_GetCombinedString_impl', 'Dvar_RegisterBool_f', 'Dvar_RegisterInt_f', 'Dvar_RegisterFloat_f']),
    ['src/PC/universal/dvar.c'])
names = ['r_lodBias', 'r_lodScale', 'r_lightTweakAmbient', 'r_lightTweakDiffuseFraction',
    'r_lightTweakSunLight', 'r_lightTweakAmbientColor', 'r_lightTweakSunColor',
    'r_lightTweakSunDiffuseColor', 'r_lightTweakSunDirection', 'cl_maxpackets',
    'loc_language', 'motd', 'cl_profileTextHeight',
    'activeAction', 'cl_anglespeedkey', 'cl_avidemo', 'cl_bypassMouseInput',
    'cl_connectTimeout', 'cl_forceavidemo', 'cl_freezeDemo', 'cl_noprint',
    'cl_serverStatusResendTime', 'cl_showSend', 'cl_showServerCommands',
    'cl_showTimeDelta', 'cl_showmouserate', 'cl_shownet', 'cl_shownuments',
    'cl_talking', 'cl_timeout', 'fx_cull', 'fx_sort', 'nextdemo']
calls = []
for path in ['src/PC/stringed/stringed_hooks.c', 'src/PC/gfx_d3d/r_dvars.c', 'src/PC/client_mp/cl_main_mp.c']:
    for match in re.finditer(r'(?:ri\.)?Dvar_Register\w+\([^;]*\);', active(path)):
        if any('"' + name + '"' in match[0] for name in names):
            calls.append(match[0].replace('ri.', '').replace('_mac(', '('))
run('registrations', 'static void RegisterCorrections(void) {\nunion { int i; float f; } flt_max = { 0x7f7fffff };\n' + '\n'.join(calls) + '\n}\n',
    ['src/PC/universal/dvar.c'])
path = 'src/PC/stringed/stringed_hooks.c'
run('language', declaration(path, 'g_languages') + ''.join(function(path, n) for n in
    ['SEH_GetLanguageName', 'SEH_GetLanguageIndexForName', 'SEH_VerifyLanguageSelection',
     'SEH_InitLanguage', 'SEH_StringEd_SetLanguageStrings', 'SEH_UpdateLanguageInfo']),
    ['src/PC/universal/dvar.c'])
run('profiles', function('src/PC/universal/com_files.c', 'FS_BuildOSPath_Internal') +
    function('src/PC/universal/com_files.c', 'FS_BuildOSPath') +
    ''.join(function('src/PC/qcommon/com_playerprofile.c', n) for n in ['Com_PlayerProfileExists',
        'Com_BuildPlayerProfilePathInternal', 'Com_BuildPlayerProfilePath', 'Com_BuildPlayerProfilePathForPlayer',
        'Com_SetPlayerProfileAndExec', 'Com_NewPlayerProfile', 'Com_DeletePlayerProfile',
        'Com_ChangePlayerProfile', 'Com_InitPlayerProfiles']), ['src/PC/qcommon/cod2x_policy.c'])
table = declaration('src/PC/cgame_mp/cg_consolecmds_mp.c', 'commandsList')
handlers = sorted(set(re.findall(r',\s*(CG_\w+)\s*}', table)) - {'CG_QuickMessage_f'})
run('commands', '\n'.join('void ' + n + '(void) { ++handlerCalls; }' for n in handlers) + '\n' +
    function('src/PC/cgame_mp/cg_consolecmds_mp.c', 'CG_QuickMessage_f') + table +
    ''.join(function('src/PC/cgame_mp/cg_consolecmds_mp.c', n) for n in ['CG_IsConsoleCommandName', 'CG_ConsoleCommand', 'CG_InitConsoleCommands']))
print('FAIL ' + ', '.join(failures) if failures else 'PASS all selected fidelity fixtures', flush=True)
raise SystemExit(bool(failures))
