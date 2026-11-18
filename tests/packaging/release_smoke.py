#!/usr/bin/env python3
"""Start every engine in a packaged app once: both games load a map, both servers answer.

    release_smoke.py "<CoD2 Unbound.app>" --game <game folder> --output <new folder>

For each game (CoD2x and the original 1.3): start it from the packaged app with
an empty private home, wait for the menu, load mp_toujane, wait until the map is
being played, quit, and require exit 0. For each dedicated server: start it on
127.0.0.1, ask its status, require mp_toujane, stop it. One PASS or FAIL line
per engine; exit 1 when any failed.

The games open a window on this Mac. Nothing leaves it: CoD2x's services are
pointed at a stand-in, the servers are not announced, and the home, with a
made-up CD key, is a new folder under --output. The first start of each game
compiles its shaders there, so allow a few minutes.
"""
import argparse
import os
import pathlib
import signal
import socket
import subprocess
import sys
import time

# A CoD2x engine asks CoD2x's own services at every start; a test points it at a local stand-in.
os.environ.setdefault('COD2X_TEST_SERVICE_URL', 'http://127.0.0.1:9')
os.environ.setdefault('COD2X_TEST_UPDATER_ADDRESS', '127.0.0.1:9')
os.environ.setdefault('COD2X_TEST_UPDATER_URL', 'http://127.0.0.1:9/api/updater/check')

ENGINES = (('game-cod2x', 'cod2_macos'), ('game-original', 'cod2_macos_stock'),
           ('server-cod2x', 'cod2_macos_ded'), ('server-original', 'cod2_macos_ded_stock'))
MAP = 'mp_toujane'
LSREGISTER = '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'


class Failed(Exception):
    pass


def stop(process):
    """Ends the process group this test started, and only that."""
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
            raise Failed(f'the engine reported an error before {what}')
        if process.poll() is not None:
            raise Failed(f'it exited with {process.returncode} before {what}')
        if time.monotonic() > deadline:
            raise Failed(f'{what} did not happen within {limit} seconds')
        time.sleep(0.2)


def environment(home, game):
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


def play(binary, game, out, port):
    """Menu, map, quit."""
    log = out / 'console.log'
    home = out / 'game'
    # A new player is asked to make a profile before any map loads; the test brings one.
    (home / 'main/players/default').mkdir(parents=True)
    (home / 'main/players/active.txt').write_text('default')
    for path in (game, home):
        if any(c in str(path) for c in '"+;\n\r'):
            raise Failed(f'{path} has a quote, plus sign or semicolon in it, which the game cannot read')
    command = [str(binary), '+set', 'fs_basepath', f'"{game}"', '+set', 'fs_homepath', f'"{home}"', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '1280x720', '+set', 'developer', '1',
               '+set', 'com_introPlayed', '1', '+set', 'sv_pure', '0', '+set', 'net_port', str(port), '+set', 'g_gametype', 'dm']
    with log.open('w') as stream:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=stream, stderr=stream, text=True,
                                   env=environment(out / 'home', game), cwd=out, start_new_session=True)
        try:
            wait_for(process, log, '--- Common Initialization Complete ---', 300, 'the menu')
            process.stdin.write(f'devmap {MAP}\n')
            process.stdin.flush()
            wait_for(process, log, 'Going from CS_PRIMED to CS_ACTIVE', 240, f'{MAP} being played')
            process.stdin.write('quit\n')
            process.stdin.flush()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                raise Failed('it did not quit within 30 seconds of being asked')
            if process.returncode != 0:
                raise Failed(f'it quit with {process.returncode}, not 0')
        finally:
            stop(process)
    return f'menu, {MAP} played, quit with exit 0'


def ask(port, command):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as link:
        link.settimeout(1)
        link.sendto(b'\xff\xff\xff\xff' + command.encode(), ('127.0.0.1', port))
        try:
            return link.recv(16384)[4:].decode('latin-1')
        except OSError:
            return ''


def serve(binary, game, out, port):
    """Start, status, stop."""
    log = out / 'console.log'
    home = out / 'server'
    (home / 'main').mkdir(parents=True)
    (home / 'main/server.cfg').write_text(f'set sv_hostname "release smoke"\nset g_gametype dm\nset sv_maxclients 8\nmap {MAP}\n')
    for path in (game, home):
        if any(c in str(path) for c in '"+;\n\r'):
            raise Failed(f'{path} has a quote, plus sign or semicolon in it, which the game cannot read')
    command = [str(binary), '+set', 'fs_basepath', f'"{game}"', '+set', 'fs_homepath', f'"{home}"', '+set', 'net_ip', '127.0.0.1',
               '+set', 'net_port', str(port), '+set', 'dedicated', '1', '+exec', 'server.cfg']
    with log.open('w') as stream:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stream, stderr=stream,
                                   env=environment(out / 'home', game), cwd=home, start_new_session=True)
        try:
            deadline = time.monotonic() + 120
            status = ''
            while f'\\mapname\\{MAP}' not in status:
                if process.poll() is not None:
                    raise Failed(f'it exited with {process.returncode} before answering')
                if time.monotonic() > deadline:
                    raise Failed('it did not answer getstatus with the map within 120 seconds')
                time.sleep(0.5)
                status = ask(port, 'getstatus')
        finally:
            stop(process)
    return f'answers getstatus with {MAP}, stopped'


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('app', type=pathlib.Path)
    parser.add_argument('--game', type=pathlib.Path, required=True, help='the Call of Duty 2 folder (holds main/)')
    parser.add_argument('--output', type=pathlib.Path, required=True, help='a folder that does not exist yet')
    parser.add_argument('--only', help='comma-separated: ' + ', '.join(name for name, _ in ENGINES))
    parser.add_argument('--port', type=int, default=29021, help='first of four private UDP ports')
    args = parser.parse_args()
    app, game, out = args.app.resolve(), args.game.resolve(), args.output.resolve()
    bundle = app / 'Contents/Helpers/CoD2 Game.app'
    chosen = set(args.only.split(',')) if args.only else {name for name, _ in ENGINES}
    if unknown := chosen - {name for name, _ in ENGINES}:
        parser.error(f'no engine named {", ".join(sorted(unknown))}')
    installed = (pathlib.Path('/Applications'), pathlib.Path.home() / 'Applications')
    if any(folder in app.parents for folder in installed):
        parser.error('give a freshly packaged app, not an installed one: this test starts and unregisters what it is given')
    if not (game / 'main').is_dir():
        parser.error(f'{game} has no main folder')
    if not 1024 < args.port < 65500:
        parser.error('use a private unprivileged port')
    # Never beside a game somebody is playing: the owner's own game is not ours to disturb or to measure against.
    busy = subprocess.run(['pgrep', '-x', 'cod2_macos|cod2_macos_stock'], capture_output=True, text=True).stdout.split()
    if busy and chosen & {'game-cod2x', 'game-original'}:
        print(f'FAIL: a game is already running (process {", ".join(busy)}); nothing was started', flush=True)
        return 1
    out.mkdir(parents=True, exist_ok=False)
    failed = 0
    try:
        for index, (name, executable) in enumerate(ENGINES):
            if name not in chosen:
                continue
            binary = bundle / 'Contents/MacOS' / executable
            folder = out / name
            folder.mkdir()
            try:
                if not binary.is_file():
                    raise Failed(f'{binary} is not in the app')
                done = (play if name.startswith('game') else serve)(binary, game, folder, args.port + index)
                print(f'PASS: {name}: {done}', flush=True)
            except Failed as reason:
                failed += 1
                print(f'FAIL: {name}: {reason} (log: {folder / "console.log"})', flush=True)
    finally:
        # Starting an app's executable can register the bundle; leave no test app registered.
        for registered in (app, bundle):
            subprocess.run([LSREGISTER, '-u', str(registered)], check=False, capture_output=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
