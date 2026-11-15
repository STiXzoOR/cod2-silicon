#!/usr/bin/env python3
"""Compare what two servers send a joining player: this port's server and the original.

    wire.py record --client <cod2_macos> --game <game folder> --output <new folder>
                   (--ours <cod2_macos_ded> | --original <cod2_lnxded>) [--port 29031] [--spawn]
    wire.py compare <ours folder> <original folder>

`record` starts one server on 127.0.0.1 with a fixed server.cfg, joins it with the
original-1.3 game of this port as the test client, and keeps what the client
printed while reading the server's packets (cl_shownet 4: every field of every
baseline and snapshot) and its list of game strings. `compare` puts two recordings
side by side, with the parts that depend on the clock taken out.

The original server is the 2005 Linux one, run in the `cod2-orig-linux` container.
It reports to Activision's master whatever it is told, so the container is started
with those names pointed at itself and its port published on this Mac only.
The game opens a window. Nothing leaves this Mac.
"""
import argparse
import os
import pathlib
import re
import signal
import subprocess
import sys
import time

# A CoD2x engine asks CoD2x's own services at every start; a test points it at a local stand-in.
os.environ.setdefault('COD2X_TEST_SERVICE_URL', 'http://127.0.0.1:9')
os.environ.setdefault('COD2X_TEST_UPDATER_ADDRESS', '127.0.0.1:9')
os.environ.setdefault('COD2X_TEST_UPDATER_URL', 'http://127.0.0.1:9/api/updater/check')

MAP = 'mp_toujane'
SERVER_CFG = f'''set sv_hostname "wire"
set g_gametype dm
set sv_maxclients 8
set sv_pure 1
set sv_fps 20
set g_password ""
set rcon_password ""
set scr_dm_timelimit 0
set scr_dm_scorelimit 0
map {MAP}
'''
ACTIVISION = ('cod2master.activision.com', 'cod2authorize.activision.com', 'cod2update.activision.com')


class Failed(Exception):
    pass


def stop(process):
    for sent in (signal.SIGTERM, signal.SIGKILL):
        if process.poll() is not None:
            return
        try:
            os.killpg(process.pid, sent)
        except ProcessLookupError:
            return
        try:
            process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            pass


def wait_for(process, log, text, limit, what):
    deadline = time.monotonic() + limit
    while text not in (seen := log.read_text(errors='replace')):
        if 'Error during initialization' in seen or 'Sys_Error' in seen:
            raise Failed(f'{what}: the engine reported an error (log: {log})')
        if process.poll() is not None:
            raise Failed(f'{what}: it exited with {process.returncode} first (log: {log})')
        if time.monotonic() > deadline:
            raise Failed(f'{what} did not happen within {limit} seconds (log: {log})')
        time.sleep(0.2)


def private_home(home, game):
    home.mkdir()
    # A made-up key with a valid checksum, in this private home only: the game refuses to start
    # without one, and a server on this Mac's own network never asks the key server about it.
    (home / '.cod2').mkdir(mode=0o700)
    preferences = home / '.cod2/preferences'
    preferences.write_text('codkey=000000000000000086D3\n')
    preferences.chmod(0o600)
    env = {**os.environ, 'HOME': str(home), 'CFFIXED_USER_HOME': str(home), 'COD2_SETUP_NONINTERACTIVE': '1',
           'COD2_SETUP_GAME_DIR': str(game), 'COD2_SETUP_CD_KEY': '000000000000000086D3'}
    env.pop('COD2_MAC_SHADER_CACHE', None)
    return env


def start_ours(binary, game, out, port):
    home = out / 'server'
    (home / 'main').mkdir(parents=True)
    (home / 'main/server.cfg').write_text(SERVER_CFG)
    command = [str(binary), '+set', 'fs_basepath', f'"{game}"', '+set', 'fs_homepath', f'"{home}"', '+set', 'net_ip', '127.0.0.1',
               '+set', 'net_port', str(port), '+set', 'dedicated', '1', '+exec', 'server.cfg']
    stream = (out / 'server.log').open('w')
    return subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stream, stderr=stream,
                            env=private_home(out / 'server-home', game), cwd=home, start_new_session=True), None


def start_original(binary, game, out, port):
    home = out / 'server'
    (home / 'main').mkdir(parents=True)
    (home / 'main/server.cfg').write_text(SERVER_CFG)
    if not os.access(binary, os.X_OK):
        raise Failed(f'{binary} is not executable; use an executable copy')
    name = f'cod2-wire-{port}'
    command = ['docker', 'run', '--rm', '--name', name, '--platform', 'linux/386']
    for host in ACTIVISION:
        command += ['--add-host', f'{host}:127.0.0.1']
    command += ['-p', f'127.0.0.1:{port}:{port}/udp', '-v', f'{game}:/cod2:ro', '-v', f'{binary.parent}:/cod2bin:ro',
                '-v', f'{home}:/wire', 'cod2-orig-linux', f'/cod2bin/{binary.name}',
                '+set', 'fs_basepath', '/cod2', '+set', 'fs_homepath', '/wire', '+set', 'net_port', str(port), '+exec', 'server.cfg']
    stream = (out / 'server.log').open('w')
    return subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stream, stderr=stream, start_new_session=True), name


def record(args):
    client, game, out = args.client.resolve(), args.game.resolve(), args.output.resolve()
    if not (game / 'main').is_dir():
        raise Failed(f'{game} has no main folder')
    if subprocess.run(['pgrep', '-x', 'cod2_macos|cod2_macos_stock'], capture_output=True, text=True).stdout.split():
        raise Failed('a game is already running; nothing was started')
    out.mkdir(parents=True, exist_ok=False)
    server, container = (start_ours(args.ours.resolve(), game, out, args.port) if args.ours
                         else start_original(args.original.resolve(), game, out, args.port))
    game_process = None
    try:
        wait_for(server, out / 'server.log', 'Server: ' + MAP, 180, 'the server loading the map')
        time.sleep(3)
        log = out / 'client.log'
        client_home = out / 'client'
        client_home.mkdir()
        command = [str(client), '+set', 'fs_basepath', f'"{game}"', '+set', 'fs_homepath', f'"{client_home}"',
                   '+set', 'net_port', str(args.port + 1), '+set', 'r_fullscreen', '0', '+set', 'r_mode', '1280x720', '+set', 'com_introPlayed', '1',
                   '+set', 'name', 'wire', '+set', 'cl_shownet', '4', '+connect', f'127.0.0.1:{args.port}']
        with log.open('w') as stream:
            game_process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=stream, stderr=stream, text=True,
                                            env=private_home(out / 'home', game), cwd=out, start_new_session=True)
            wait_for(game_process, log, 'svc_snapshot', 420, 'the first snapshot reaching the client')
            if args.spawn:
                # What the team and weapon menus send: "mr <server id> <menu index> <response>".
                # 16 is a fresh server's id; 1 and 2 are the team and Allied weapon menus of the stock scripts.
                time.sleep(3)
                for line in ('cmd mr 16 1 allies', 'cmd mr 16 2 enfield_mp'):
                    game_process.stdin.write(line + '\n')
                    game_process.stdin.flush()
                    time.sleep(2)
            time.sleep(args.seconds)
            for line in ('set cl_shownet 0', 'configstrings', 'quit'):
                game_process.stdin.write(line + '\n')
                game_process.stdin.flush()
                time.sleep(1)
            try:
                game_process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                raise Failed('the client did not quit')
    finally:
        if game_process:
            stop(game_process)
        if container:
            subprocess.run(['docker', 'stop', '-t', '2', container], capture_output=True, check=False)
        stop(server)
    print(f'recorded {out / "client.log"} ({(out / "client.log").stat().st_size} bytes)')


# Fields that hold a time on the server's clock: compared by presence, not value.
CLOCK = {'commandTime', 'weaponTime', 'weaponDelay', 'legsTimer', 'torsoTimer', 'pm_time', 'pos.trTime', 'apos.trTime',
         'time', 'time2'}
# What a spawn picks at random: the spawn point and the soldier's model and head.
RANDOM = re.compile(r'^(origin\[\d\]|delta_angles\[\d\]|modelindex|attachModelIndex\[\d\]|pos\.trBase\[\d\]|apos\.trBase\[\d\])$')


def read(folder):
    """A recording as (game strings, baseline entities, per-snapshot field lists)."""
    text = (pathlib.Path(folder) / 'client.log').read_text(errors='replace')
    lines = text.split('\n')
    strings = {}
    for line in lines:
        found = re.match(r'^ {0,3}(\d{1,4}): (.*)$', line)
        if found and not re.match(r'^\s*\d+:\s*(#|playerstate|packet|svc_|END)', line):
            strings[int(found.group(1))] = found.group(2)
    start = next(n for n, line in enumerate(lines) if 'svc_gamestate' in line)
    # The numbers before each colon are bit positions in the packet; they are not part of the content.
    baseline = re.sub(r'\b\d+: ', '', lines[start + 1]).split()
    snapshots = []
    for message in re.split(r'\n-{18}\n', text):
        if 'svc_snapshot' not in message:
            continue
        message = re.sub(r'(?m)(^| )\s*\d+:', ' ', message)
        fields = [(name, value) for name, value in re.findall(r'([A-Za-z_][A-Za-z_0-9.\[\]]*):(-?\d+)', message)]
        snapshots.append((fields, re.findall(r'PS_[A-Z_]+', message), message.count('svc_serverCommand')))
    return strings, baseline, snapshots


def compare(args):
    ours, original = read(args.ours_folder), read(args.original_folder)
    findings = 0
    differing = [n for n in sorted(set(ours[0]) | set(original[0])) if ours[0].get(n) != original[0].get(n)]
    print(f'game strings: {len(ours[0])} and {len(original[0])}, {len(differing)} differ')
    for n in differing:
        findings += 1
        print(f'  {n}: ours {ours[0].get(n)!r}\n  {n}: original {original[0].get(n)!r}')
    same = ours[1] == original[1]
    findings += not same
    print(f'baselines: {"identical" if same else "DIFFER"} ({len(ours[1])} and {len(original[1])} fields)')

    def content(snapshots):
        """Snapshots that carry more than the clock, with clock fields reduced to their names."""
        kept = []
        for fields, stats, commands in snapshots:
            items = [name if name in CLOCK else f'{name}:{value}' for name, value in fields]
            if set(items) - {'commandTime'} or stats:
                kept.append((items, stats, commands))
        return kept
    a, b = content(ours[2]), content(original[2])
    print(f'snapshots: {len(ours[2])} and {len(original[2])}; with content beyond the clock: {len(a)} and {len(b)}')
    findings += len(a) != len(b)
    identical = random_only = 0
    for index, (x, y) in enumerate(zip(a, b)):
        if x == y:
            identical += 1
            continue
        only_ours = [item for item in x[0] if item not in y[0]]
        only_original = [item for item in y[0] if item not in x[0]]
        if x[1:] == y[1:] and all(RANDOM.match(item.split(':')[0]) for item in only_ours + only_original):
            random_only += 1
            print(f'  snapshot {index}: differs only in what a spawn picks at random: {only_ours} / {only_original}')
        else:
            findings += 1
            print(f'  snapshot {index}: ours only {only_ours} {x[1:]} / original only {only_original} {y[1:]}')
    print(f'identical: {identical}; random picks only: {random_only}; findings: {findings}')
    return 1 if findings else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest='command', required=True)
    rec = commands.add_parser('record')
    rec.add_argument('--client', type=pathlib.Path, required=True, help="this port's original-1.3 game")
    rec.add_argument('--game', type=pathlib.Path, required=True)
    rec.add_argument('--output', type=pathlib.Path, required=True)
    which = rec.add_mutually_exclusive_group(required=True)
    which.add_argument('--ours', type=pathlib.Path, help="this port's original-1.3 dedicated server")
    which.add_argument('--original', type=pathlib.Path, help='an executable copy of the original cod2_lnxded')
    rec.add_argument('--port', type=int, default=29031)
    rec.add_argument('--seconds', type=float, default=12, help='how long to stay after entering the game')
    rec.add_argument('--spawn', action='store_true', help='join the Allies with a rifle, so a living player is recorded')
    cmp = commands.add_parser('compare')
    cmp.add_argument('ours_folder', type=pathlib.Path)
    cmp.add_argument('original_folder', type=pathlib.Path)
    args = parser.parse_args()
    if args.command == 'compare':
        return compare(args)
    try:
        record(args)
    except Failed as reason:
        print(f'FAIL: {reason}', flush=True)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
