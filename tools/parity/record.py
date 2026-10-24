#!/usr/bin/env python3
"""Record a bounded deterministic local-server run with an isolated homepath."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import zipfile

from compare import read_players, read_trace, validate_players


def sha256(path):
    with path.open('rb') as stream:
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def testclients_bootstrap(iwds):
    """Enable the installed DM script's dormant helper without storing game script text here."""
    def installed(member):
        for path in reversed(iwds):
            with zipfile.ZipFile(path) as archive:
                if member in archive.namelist():
                    return archive.read(member)
        raise ValueError(f'Missing {member} for test-client helper bootstrap')

    member = 'maps/mp/gametypes/dm.gsc'
    script = installed(member)
    helper = re.compile(rb'(?m)^([ \t]*)//[ \t]*(thread[ \t]+maps\\mp\\gametypes\\_teams::'
                        rb'addTestClients\(\);[ \t]*\r?)$')
    script, count = helper.subn(rb'\1\2', script)
    if count != 1:
        raise ValueError(f'Expected exactly one commented test-client helper call; found {count}')
    teams_member = 'maps/mp/gametypes/_teams.gsc'
    teams = installed(teams_member)
    # Use the gametype's normal selection callbacks directly. The dormant
    # helper relies on menu listeners that are not reliable for test clients.
    teams, count = re.subn(rb'\bthread TestClient\(team\);',
                           b'thread oracleTestClient(team);', teams)
    if count != 1:
        raise ValueError(f'Expected one test-client entry point; found {count}')
    teams += b'''
oracleTestClient(team)
{
    self endon("disconnect");
    wait 0.25;
    if(team == "allies")
        self [[level.allies]]();
    else
        self [[level.axis]]();
    wait 0.5;
    if(team == "allies")
        self [[level.weapon]]("thompson_mp");
    else
        self [[level.weapon]]("mp40_mp");
}
'''
    return [(member, script), (teams_member, teams)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='new directory; never reused')
    parser.add_argument('--frames', type=int, default=1000)
    parser.add_argument('--seed', type=int, default=12345)
    parser.add_argument('--map', default='mp_toujane')
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--port', type=int, default=28960)
    parser.add_argument('--dedicated', action='store_true', help='use with a headless dedicated server binary')
    parser.add_argument('--windowed', action='store_true', help='bounded 640x480 desktop QA using timeout -k')
    parser.add_argument('--players', action='store_true', help='record raw player fields in players.txt')
    parser.add_argument('--clients', type=int, metavar='N', help='request N test clients; requires --players')
    commands = parser.add_mutually_exclusive_group()
    commands.add_argument('--commands-record', action='store_true', help='record commands in usercmds.txt')
    commands.add_argument('--replay', type=Path, help='replay commands from this file')
    args = parser.parse_args()
    if args.frames < 1 or not 0 <= args.seed <= 2147483647 or args.timeout <= 0:
        parser.error('positive frames/timeout and a nonnegative int32 seed are required')
    if not 1024 <= args.port <= 65535:
        parser.error('port must be unprivileged and fit a UDP port')
    if not re.fullmatch(r'mp_[A-Za-z0-9_]+', args.map):
        parser.error('expected an mp_ map name')
    if args.clients is not None and (not args.players or not 1 <= args.clients <= 64):
        parser.error('--clients requires --players and a count from 1 to 64')
    replay = args.replay.resolve() if args.replay else None
    if replay is not None and (not replay.is_file() or not replay.stat().st_size):
        parser.error('--replay needs a nonempty command stream')
    binary, data, out = args.binary.resolve(), args.data.resolve(), args.output.resolve()
    iwds = sorted((data / 'main').glob('*.iwd'))
    if not binary.is_file() or not os.access(binary, os.X_OK) or not iwds:
        parser.error('need an executable binary and data/main/*.iwd')
    if any(c in str(data) + str(out) for c in '";+\n\r'):
        parser.error('paths cannot contain quotes, semicolons, + or newlines')
    bootstrap = testclients_bootstrap(iwds) if args.clients is not None else None
    out.mkdir(parents=True, exist_ok=False)
    home = out / 'home'
    home.mkdir()
    if bootstrap is not None:
        for member, script in bootstrap:
            target = home / 'main' / member
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(script)
    command = [str(binary), '+set', 'fs_basepath', f'"{data}"', '+set', 'fs_homepath', f'"{home}"',
               '+set', 'fs_game', '""', '+set', 'dedicated', '1' if args.dedicated else '0',
               '+set', 'logfile', '2', '+set', 'sv_punkbuster', '0',
               '+set', 'net_ip', '127.0.0.1',
               '+set', 'net_port', str(args.port),
               '+set', 'sv_fps', '20', '+set', 'fixedtime', '50',
               '+set', 'g_gametype', 'dm']
    if args.clients is not None:
        command += ['+set', 'scr_testclients', str(args.clients), '+set', 'sv_pure', '0']
    command += ['+map', args.map]
    if args.windowed:
        command[1:1] = ['+set', 'r_fullscreen', '0', '+set', 'r_mode', '640x480',
                        '+set', 'com_maxfps', '60']
        command = ['timeout', '-k', '5', str(args.timeout), *command]
    env = dict(os.environ, SYSDIFF_SEED=str(args.seed), SYSDIFF_MAXFRAMES=str(args.frames),
               SYSDIFF_STATEHASH=str(out / 'statehash.txt'), SYSDIFF_DUMP=str(out / 'entities.txt'))
    for name in ('SYSDIFF_PLAYERS', 'SYSDIFF_CLIENTS', 'SYSDIFF_USERCMDS', 'SYSDIFF_REPLAY'):
        env.pop(name, None)
    if args.players:
        env['SYSDIFF_PLAYERS'] = str(out / 'players.txt')
    if args.clients is not None:
        env['SYSDIFF_CLIENTS'] = str(args.clients)
    if args.commands_record:
        env['SYSDIFF_USERCMDS'] = str(out / 'usercmds.txt')
    if replay is not None:
        env['SYSDIFF_REPLAY'] = str(replay)
    metadata = dict(command=command, platform=platform.platform(), binary_sha256=sha256(binary),
                    seed=args.seed, frames=args.frames,
                    iwd_sha256={str(p.relative_to(data)): sha256(p) for p in iwds})
    if args.players:
        metadata['players'] = 'players.txt'
    if args.clients is not None:
        metadata['clients'] = args.clients
        metadata['testclients_bootstrap'] = [str(Path('home/main') / member) for member, _ in bootstrap]
    if args.commands_record:
        metadata['commands_record'] = 'usercmds.txt'
    if replay is not None:
        metadata['replay'] = str(replay)
    (out / 'run.json').write_text(json.dumps(metadata, indent=2) + '\n')
    with (out / 'console.log').open('w') as log:
        process = subprocess.Popen(command, env=env, cwd=out, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            code = process.wait(timeout=args.timeout + (6 if args.windowed else 0))
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            raise ValueError(f'Run interrupted or exceeded {args.timeout}s; inspect {out}')
    if code:
        raise ValueError(f'Engine exited {code}; inspect {out / "console.log"}')
    rows = read_trace(out / 'statehash.txt')
    if len(rows) != args.frames or not any(row[2] > 0 for row in rows):
        raise ValueError('Incomplete or empty-world trace; refusing a false pass')
    if not (out / 'entities.txt').is_file() or not (out / 'entities.txt').stat().st_size:
        raise ValueError('Missing final entity dump')
    if args.players:
        players = read_players(out / 'players.txt')
        validate_players(players, rows, out / 'players.txt', args.clients)
    if args.commands_record and (not (out / 'usercmds.txt').is_file()
                                or not (out / 'usercmds.txt').stat().st_size):
        raise ValueError('Missing command recording')
    print(f'Recorded {len(rows)} frames in {out}')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        sys.exit(str(error))
