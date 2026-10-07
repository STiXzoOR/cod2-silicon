#!/usr/bin/env python3
"""Exercise real native admission/auth/discovery with engine ABI and flags."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CLIENT = 'src/PC/server_mp/sv_client_mp.c'
MAIN = 'src/PC/server_mp/sv_main_mp.c'


def function(source, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;{]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError('missing production function: ' + name)
    depth = 0
    for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', source[match.start():]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError('unterminated production function: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', help='characterize this Git commit')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = json.loads(args.compile_db.read_text())
    failures = []
    for target in ('cod2_macos', 'cod2_macos_ded'):
        row = next(row for row in rows if row['file'].endswith('/' + CLIENT) and
                   target + '.dir/' in (row.get('command') or ' '.join(row['arguments'])))
        original = row['file'][:-len('/' + CLIENT)]
        flags = row.get('arguments') or shlex.split(row['command'])
        flags = [x.replace(original, str(ROOT)) for x in flags[:flags.index('-o')]]
        flags = [x for x in flags if x not in ('-DNDEBUG', '-c')]
        flags += ['-UNDEBUG', '-DCOD2X_HOST_REVISION=0', '-fsanitize=address,undefined',
                  '-fno-omit-frame-pointer', '-I' + str(out)]

        def active(path):
            source = (subprocess.check_output(['git', 'show', args.baseline + ':' + path],
                                              cwd=ROOT, text=True) if args.baseline else
                      (ROOT / path).read_text())
            return subprocess.run([*flags, '-iquote', str((ROOT / path).parent),
                                   '-E', '-P', '-x', 'c', '-'], input=source, cwd=ROOT,
                                  text=True, capture_output=True, check=True).stdout

        client, main = active(CLIENT), active(MAIN)
        generated = '#define WS69_HAS_CHALLENGE_METADATA ' + str(int('cod2xChallengeIdentities' in client)) + '\n'
        if 'cod2xChallengeIdentities' in client:
            generated += re.search(r'typedef struct Cod2x_ChallengeIdentity\s*\{[\s\S]*?\}\s*Cod2x_ChallengeIdentity;', client)[0] + '\n'
            generated += re.search(r'static Cod2x_ChallengeIdentity cod2xChallengeIdentities\[[^;]*;', client)[0] + '\n'
            for name in ('SV_Cod2xChallengeDigest', 'SV_Cod2xStoreChallengeDigest'):
                generated += function(client, name)
        for name in ('SV_AuthorizeRequest', 'SV_AuthorizeIpPacket', 'SV_GetChallenge', 'SV_UserinfoChanged', 'SV_DirectConnect'):
            generated += function(client, name)
        for name in ('SVC_Info', 'SVC_Status', 'SVC_GameCompleteStatus'):
            generated += function(main, name)
        (out / 'host_protocol_source.h').write_text(generated)
        exe = out / target
        command = [*flags, '-Wl,-dead_strip', str(ROOT / 'tests/cod2x/test_host_protocol.c'),
                   str(ROOT / 'src/PC/universal/q_shared.c'), '-o', str(exe)]
        log = out / (target + '-compile.log')
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        log.write_text(result.stdout + result.stderr)
        if result.returncode:
            print(log.read_text()[-4000:])
            raise SystemExit('fixture compilation failed: ' + target)
        result = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True)
        (out / (target + '-result.log')).write_text(result.stdout + result.stderr)
        print(target + ': ' + result.stdout + result.stderr, flush=True)
        if result.returncode:
            failures.append(target)
    if failures:
        raise SystemExit('Failed production fixtures: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
