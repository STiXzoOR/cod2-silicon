#!/usr/bin/env python3
"""Licensed-data live check. Private output contains screenshots/logs: never commit it."""
import os, subprocess, time, socket, json, re, shutil, signal
from pathlib import Path
import argparse
root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description='Two native clients, private homes, bounded stock-server soak')
parser.add_argument('--server', type=Path, required=True)
parser.add_argument('--client', type=Path, action='append', required=True)
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--shader-cache', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--port', type=int, required=True)
parser.add_argument('--duration', type=int, default=900)
parser.add_argument('--test-key', action='store_true', help='synthetic LAN-only test key, no retail key read')
parser.add_argument('--codx-iwd', type=Path, help='optional owned iw_CoD2x_01.iwd for the second client')
options = parser.parse_args()
if not shutil.which('timeout') or not shutil.which('leaks'):
    parser.error('timeout and the macOS leaks tool are required; install nothing automatically')
if len(options.client) != 2 or not 1024 < options.port < 65533 or options.duration < 90:
    parser.error('exactly two clients, port 1025..65532 and duration >=90 required')
for binary in [options.server, *options.client]:
    if not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error('missing executable')
if not (options.data / 'main/iw_00.iwd').is_file():
    parser.error('missing owned game data')
if options.codx_iwd and (options.codx_iwd.name != 'iw_CoD2x_01.iwd' or not options.codx_iwd.is_file()):
    parser.error('expected an owned iw_CoD2x_01.iwd')
data = options.data.resolve()
port = options.port
out = options.output.resolve()
for path in [data, out, options.shader_cache.resolve()]:
    if any((c in str(path) for c in '";+\n\r')):
        parser.error('unsupported engine path characters')
for candidate in range(port, port + 3):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as check:
        check.bind(('127.0.0.1', candidate))
os.umask(0o077)
out.mkdir(parents=True, exist_ok=False)
key = '000000000000000086D3' if options.test_key else next((line.split('=', 1)[1].strip() for line in (Path.home() / '.cod2/preferences').read_text().splitlines() if line.startswith('codkey=')))
home = out / 'server'
(home / 'main').mkdir(parents=True)
password = os.urandom(16).hex()
(home / 'main/server.cfg').write_text((root / 'scripts/server/server.cfg').read_text() + f'\nset rcon_password "{password}"\nset scr_dm_timelimit 0\nset sv_mapRotationCurrent "gametype dm map mp_carentan"\n')
processes = []
logs = []

def interrupted(signum, frame):
    raise KeyboardInterrupt

signal.signal(signal.SIGTERM, interrupted)

def launch(name, binary, args, env):
    log = out / (name + '.log')
    stream = log.open('w')
    logs.append(stream)
    p = subprocess.Popen(['timeout', '-k', '10', str(options.duration + 150), str(binary), *args], stdin=subprocess.PIPE, stdout=stream, stderr=stream, text=True, cwd=out, env=env)
    processes.append(p)
    time.sleep(0.1)
    children = subprocess.check_output(['pgrep', '-P', str(p.pid)], text=True).split()
    return dict(name=name, p=p, pid=int(children[0]), log=log, opened=0)

def send(c, text):
    c['p'].stdin.write(text + '\n')
    c['p'].stdin.flush()

def query(command):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(3)
        s.sendto(b'\xff' * 4 + command.encode() + b'\n', ('127.0.0.1', port))
        return s.recv(65535).decode(errors='replace')

def rcon(cmd):
    return query('rcon ' + password + ' ' + cmd)

def content(c):
    return c['log'].read_text(errors='replace')

def wait(predicate, seconds=70):
    until = time.monotonic() + seconds
    while not predicate():
        if time.monotonic() > until:
            raise RuntimeError('readiness timeout')
        time.sleep(0.3)
try:
    server = launch('server', options.server.resolve(), ['+set', 'fs_basepath', '"' + str(data) + '"', '+set', 'fs_homepath', '"' + str(home) + '"', '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', str(port), '+set', 'dedicated', '1', '+exec', 'server.cfg', '+map', 'mp_toujane'], dict(os.environ, COD2_SERVER_TICK_CSV=str(out / 'ticks.csv')))
    wait(lambda: 'Working directory:' in content(server))
    print('server ready', flush=True)
    (out / 'status-initial.txt').write_text(query('getstatus ws26'))
    time.sleep(0.6)
    assert 'Invalid password' in query('rcon invalid status')
    time.sleep(0.6)
    assert 'map:' in rcon('status')
    clients = []
    for i, build in enumerate(options.client):
        ch = out / ('client' + str(i))
        (ch / '.cod2').mkdir(parents=True)
        (ch / '.cod2/preferences').write_text('codkey=' + key + '\n')
        if i == 1 and options.codx_iwd:
            (ch / 'game/main').mkdir(parents=True)
            shutil.copy2(options.codx_iwd, ch / 'game/main/iw_CoD2x_01.iwd')
        env = dict(os.environ, HOME=str(ch), CFFIXED_USER_HOME=str(ch), COD2_SETUP_NONINTERACTIVE='1', COD2_SETUP_GAME_DIR=str(data), COD2_SETUP_CD_KEY=key, COD2_MAC_SHADER_CACHE=str(options.shader_cache.resolve()))
        args = ['+set', 'fs_basepath', '"' + str(data) + '"', '+set', 'fs_homepath', '"' + str(ch / 'game') + '"', '+set', 'net_port', str(port + 1 + i), '+set', 'r_fullscreen', '0', '+set', 'r_mode', '640x480', '+set', 'com_maxfps', '60', '+set', 'developer', '1', '+set', 'cl_showServerCommands', '1', '+set', 'com_introPlayed', '1', '+set', 'com_recommendedSet', '1', '+set', 'name', 'ws26-' + str(i), '+set', 'cl_allowDownload', '0', '+connect', '127.0.0.1:' + str(port)]
        c = launch('client' + str(i), build.resolve(), args, env)
        clients.append(c)
        wait(lambda: 'CL_InitCGame:' in content(c) or c['p'].poll() is not None)
        if c['p'].poll() is not None:
            raise RuntimeError('client exited before cgame')
        time.sleep(2)
        send(c, 'configstrings')
        send(c, 'sv_serverid')
    print('two clients initialized', flush=True)
    start = time.monotonic()
    actions = set()
    samples = []
    last_sample = 0
    while time.monotonic() - start < options.duration:
        elapsed = time.monotonic() - start
        for c in clients:
            if c['p'].poll() is not None:
                raise RuntimeError(c['name'] + ' exited')
            text = content(c)
            menus = dict(((int(n), v) for n, v in re.findall('^\\s*(12[4-9]\\d|13\\d\\d): (\\S+)', text, re.M)))
            ids = re.findall('"sv_serverid" is: "(\\d+)', text)
            opened = re.findall("script menu '([^']+)'", text)
            if len(opened) > c['opened']:
                send(c, 'sv_serverid')
                time.sleep(0.1)
                ids = re.findall('"sv_serverid" is: "(\\d+)', content(c))
                for menu in opened[c['opened']:]:
                    idx = next((n for n, v in menus.items() if v == menu), None)
                    if idx is None:
                        send(c, 'configstrings')
                        # The previous map's table can still be in the log.
                        # Retry this request after the new table arrives.
                        break
                    choice = 'close' if menu.startswith('serverinfo_') else 'allies' if menu.startswith('team_') else {'weapon_british': 'enfield_mp', 'weapon_american': 'm1garand_mp', 'weapon_russian': 'mosin_nagant_mp', 'weapon_german': 'kar98k_mp'}.get(menu)
                    if idx is not None and ids and choice:
                        send(c, f'cmd mr {ids[-1]} {idx - 1246} {choice}')
                        c['opened'] += 1
                    else:
                        raise RuntimeError('unsupported menu request: ' + menu)
            cycle = int(elapsed) % 30
            if cycle == 5:
                send(c, '+forward')
                send(c, '+attack')
            if cycle == 10:
                send(c, '-forward')
                send(c, '-attack')
                send(c, '+reload')
            if cycle == 12:
                send(c, '-reload')
                send(c, '+right')
            if cycle == 15:
                send(c, '-right')
                send(c, 'toggleads')
            if cycle == 17:
                send(c, 'toggleads')
                send(c, 'viewpos')
            if cycle == 20:
                send(c, 'configstrings')
            if cycle == 22:
                send(c, 'screenshotJPEG ws26-' + str(int(elapsed)))
        for threshold, cmd in [(options.duration * 4 / 15, 'map_restart'), (options.duration * 8 / 15, 'map_rotate'), (options.duration * 12 / 15, 'set sv_fps 40')]:
            if elapsed > threshold and threshold not in actions:
                print('server action', cmd, flush=True)
                (out / ('rcon-' + str(threshold) + '.txt')).write_text(rcon(cmd))
                actions.add(threshold)
                for c in clients:
                    send(c, 'configstrings')
                    send(c, 'sv_serverid')
        if elapsed - last_sample > 30:
            values = subprocess.check_output(['ps', '-p', str(server['pid']), '-o', 'pcpu=,rss='], text=True).strip()
            status = query('getstatus ws26')
            assert 'ws26-0' in status and 'ws26-1' in status, 'lost connected client'
            if elapsed > options.duration * 8 / 15 + 20:
                assert '\\mapname\\mp_carentan' in status, 'rotation did not reach Carentan'
            for c in clients:
                assert 'ERROR: Server connection timed out' not in content(c), 'client timed out'
            samples.append(dict(seconds=round(elapsed, 1), server=values, status=status))
            (out / 'samples.json').write_text(json.dumps(samples, indent=2))
            print('sample', round(elapsed), values, flush=True)
            last_sample = elapsed
        time.sleep(1)
    assert all((content(c).count('CL_InitCGame:') >= 3 for c in clients)), 'client did not reload both maps'
    assert all((content(c).count("script menu 'weapon_") >= 3 for c in clients)), 'client did not select a weapon on every map'
    (out / 'status-final.txt').write_text(query('getstatus ws26'))
    for c in clients:
        send(c, 'viewpos')
        send(c, 'quit')
    time.sleep(2)
    with (out / 'leaks.log').open('w') as stream:
        leakcode = subprocess.run(['leaks', str(server['pid'])], stdout=stream, stderr=stream).returncode
    send(server, 'quit')
    exits = [p.wait(timeout=25) for p in processes]
    (out / 'result.json').write_text(json.dumps(dict(exits=exits, soak_seconds=options.duration, leaks_code=leakcode, actions=sorted(actions)), indent=2))
    assert exits == [0, 0, 0], 'unclean engine exit'
    assert leakcode == 0, 'leak check failed; inspect leaks.log'
    print('complete', exits, flush=True)
finally:
    for p in processes:
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(timeout=15)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
    for stream in logs:
        stream.close()
