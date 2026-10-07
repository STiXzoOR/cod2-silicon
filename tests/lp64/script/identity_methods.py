#!/usr/bin/env python3
"""Real CoD2x admission, method resolution, compiler emission and VM boundary."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CLIENT = 'src/PC/server_mp/sv_client_mp.c'
PLAYER = 'src/PC/game_mp/g_client_script_cmd_mp.c'
MAIN = 'src/PC/game_mp/g_scr_main_mp.c'
spec = importlib.util.spec_from_file_location('host_protocol', ROOT / 'tests/cod2x/run_host_protocol.py')
host = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host)


def function(source, name):
    body = host.function(source, name)
    if re.match(r'\s*' + re.escape(name) + r'\(', body):
        start = source.index(body.rstrip())
        previous = source[:start].rstrip('\n').rsplit('\n', 1)[-1]
        body = previous + '\n' + body
    return body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline')
    parser.add_argument('--parser-ubsan', action='store_true',
                        help='also expose the retained yacc parser UBSan limitation')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = json.loads(args.compile_db.read_text())
    failures = []
    for target in ('cod2_macos', 'cod2_macos_ded'):
        row = next(r for r in rows if r['file'].endswith('/' + CLIENT) and
                   target + '.dir/' in (r.get('command') or ' '.join(r['arguments'])))
        original = row['file'][:-len('/' + CLIENT)]
        flags = row.get('arguments') or shlex.split(row['command'])
        flags = [v.replace(original, str(ROOT)) for v in flags[:flags.index('-o')]]
        flags = [v for v in flags if v not in ('-c', '-DNDEBUG')]
        flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer', '-I' + str(out)]

        def active(path):
            source = (subprocess.check_output(['git', 'show', args.baseline + ':' + path],
                                              cwd=ROOT, text=True) if args.baseline else
                      (ROOT / path).read_text())
            return subprocess.run([*flags, '-iquote', str((ROOT / path).parent), '-E', '-P',
                                   '-x', 'c', '-'], input=source, cwd=ROOT,
                                  text=True, capture_output=True, check=True).stdout

        client, player, main_source = active(CLIENT), active(PLAYER), active(MAIN)
        enabled = 'cod2xChallengeIdentities' in client
        implemented = 'SV_Cod2xClientHwid' in client
        generated = '#define WS69_HAS_CHALLENGE_METADATA ' + str(int(enabled)) + '\n'
        generated += '#define WS76_IDENTITY_IMPLEMENTED ' + str(int(implemented)) + '\n'
        generated += function(client, 'SV_ClientNumForClient')
        if enabled:
            start = client.index('typedef struct Cod2x_ChallengeIdentity')
            end = client.index('\nvoid SV_AuthorizeRequest', start)
            generated += client[start:end] + '\n'
        for name in ('SV_CloseDownload', 'SV_AuthorizeRequest',
                     'SV_AuthorizeIpPacket', 'SV_GetChallenge', 'SV_UserinfoChanged',
                     'SV_FreeClient', 'SV_DropClient', 'SV_DirectConnect', 'SV_FreeClientScriptPers', 'SV_FreeClients'):
            generated += function(client, name)
        discovery = active('src/PC/server_mp/sv_main_mp.c')
        for name in ('SVC_Info', 'SVC_Status', 'SVC_GameCompleteStatus'):
            generated += function(discovery, name)
        (out / 'host_protocol_source.h').write_text(generated)

        # Reuse WS69's external seams and cases; FreeClient itself stays production.
        fixture = (ROOT / 'tests/cod2x/test_host_protocol.c').read_text()
        fixture = fixture.replace('void SV_FreeClient(client_t *cl) { (void)cl; }', '')
        fixture = fixture.replace('++connects; return NULL;',
                                  '++connects; WS76_ConnectScript(n); return (char *)gameDenial;')
        fixture = fixture.replace('int main(void)', 'int WS69_protocol_fixture_main(void)')
        (out / 'host_fixture.h').write_text(fixture)
        lifecycle = active('src/PC/server_mp/sv_init_mp.c')
        commands = active('src/PC/server_mp/sv_ccmds_mp.c')
        map_change = ''
        if 'SV_Cod2xBeginMapChangeApproved' in commands:
            # Identity shutdown runs without a callback. WS81 separately tests
            # the real callback/VM; retain actual server guard bodies here.
            map_change = '''
qboolean GScr_Cod2xBeforeMapChange(qboolean shutdown, const char *source)
{ (void)shutdown; (void)source; return 1; }
qboolean GScr_Cod2xConsumeMapChange(const char *source, const char *command,
                                  const char *target, const char *marker)
{ (void)source; (void)command; (void)target; (void)marker; return 0; }
void GScr_Cod2xResetMapChange(void) {}
'''
            map_change += re.search(r'^static unsigned int cod2xMapChangeDepth;', commands, re.M)[0] + '\n'
            map_change += ''.join(function(commands, name) for name in
                                 ('SV_Cod2xBeginMapChangeApproved', 'SV_Cod2xBeginMapChange', 'SV_Cod2xEndMapChange'))
        (out / 'identity_shutdown_source.h').write_text(map_change + function(lifecycle, 'SV_Shutdown') +
                                                        function(lifecycle, 'SV_ChangeMaxClients'))

        table = re.search(r'static const BuiltinMethodDef player_methods\[\][\s\S]*?\n};', player)[0]
        names = re.findall(r'\(BuiltinMethod\)(\w+)', table)
        methods = ''.join('static unsigned int ' + n + '(void) { return 0; }\n' for n in names)
        methods += table + '\n' + function(player, 'Player_GetMethod')
        if 'Player_Cod2xGetMethod' in player:
            start = player.index('static gentity_t *PlayerCmd_Cod2xPlayer')
            end = player.index('static const BuiltinMethodDef player_methods', start)
            methods += function(player, 'PlayerCmd_GetPlayerEntity') + player[start:end]
        methods += function(main_source, 'Scr_GetMethod')
        (out / 'player_identity_source.h').write_text(methods)

        compiler = active('src/PC/script/scr_compiler.c')
        vm = active('src/PC/script/scr_vm.c')
        boundary = ''.join(function(compiler, n) for n in
                           ('EmitOpcode', 'EmitFunctionTableIndex', 'EmitBuiltinMethodOpcode'))
        boundary += ''.join(function(vm, n) for n in
                            ('VM_ReadU16', 'VM_PushValue', 'IncInParam', 'Scr_GetNumParam',
                             'Scr_AddBool', 'Scr_AddString', 'Scr_AddVector',
                             'VM_CandidateCompleteCall', 'VM_CandidateCallBuiltinMethod'))
        view = active('src/PC/game_mp/g_client_mp.c')
        boundary += ''.join(function(view, n) for n in
                            ('G_EntityForNum', 'G_Level', 'G_GetPlayerViewOrigin'))
        (out / 'script_identity_boundary.h').write_text(boundary)
        exe = out / target
        command = [*flags, '-DCOD2X_SCRIPT_IDENTITY_FIXTURE=1', '-Wl,-dead_strip',
                   str(ROOT / 'tests/lp64/game/script_api.c'),
                   str(ROOT / 'src/PC/universal/q_shared.c'), '-o', str(exe)]
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        (out / (target + '-compile.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            print(result.stderr[-7000:])
            raise SystemExit('fixture compilation failed: ' + target)
        result = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True)
        (out / (target + '-result.log')).write_text(result.stdout + result.stderr)
        print(target + ': ' + result.stdout + result.stderr, flush=True)
        if result.returncode:
            failures.append(target)
        parser_exe = out / (target + '-parser')
        # The retained yacc parser intentionally forms a stack-before-start
        # pointer. Its existing compiler fixture uses ASan; record that UBSan
        # limitation rather than silently weakening the method/metadata tests.
        parser_flags = [] if args.parser_ubsan else ['-fno-sanitize=undefined']
        result = subprocess.run([*flags, *parser_flags, '-Wl,-dead_strip',
                                 str(ROOT / 'tests/lp64/script/identity_parser.c'),
                                 '-o', str(parser_exe)], cwd=ROOT, text=True, capture_output=True)
        (out / (target + '-parser-compile.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            print(result.stderr[-5000:])
            raise SystemExit('parser fixture compilation failed: ' + target)
        result = subprocess.run([str(parser_exe)], cwd=ROOT, text=True, capture_output=True)
        (out / (target + '-parser-result.log')).write_text(result.stdout + result.stderr)
        print(target + ': ' + result.stdout + result.stderr, flush=True)
        if result.returncode:
            failures.append(target + '-parser')
    if failures:
        raise SystemExit('Failed fixtures: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
