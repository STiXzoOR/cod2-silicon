"""Drive grounded local combat; only the child launched here is stopped."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--seconds', type=float, default=600)
parser.add_argument('--primary', choices=['enfield_mp', 'sten_mp'], default='sten_mp')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
home = out / 'home'
home.mkdir()
command = [str(args.binary.resolve()), '+set', 'fs_basepath', str(Path.home() / 'Games/CoD2'),
           '+set', 'fs_homepath', f'"{home}"', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '1280x720',
           '+set', 'com_maxfps', '333', '+set', 'com_introPlayed', '1', '+set', 'developer', '1',
           '+set', 'sv_pure', '0', '+set', 'g_gametype', 'dm', '+set', 'net_port', '29018',
           '+set', 'scr_forcerespawn', '1',
           '+devmap', 'mp_toujane']
events = []
with (out / 'console.log').open('w') as log:
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=log, stderr=log,
                               text=True, cwd=out)
    def send(text):
        if process.poll() is not None:
            raise RuntimeError(f'client exited {process.returncode}')
        process.stdin.write(text + '\n')
        process.stdin.flush()
        events.append([time.monotonic(), text])
    def wait(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f'client exited {process.returncode}')
            time.sleep(min(.1, max(0, deadline - time.monotonic())))
    start = None
    try:
        deadline = time.monotonic() + 60
        while 'Going from CS_PRIMED to CS_ACTIVE' not in (out / 'console.log').read_text(errors='replace'):
            if time.monotonic() > deadline:
                raise RuntimeError('map did not activate')
            wait(.1)
        send('sv_serverId'); send('configstrings'); wait(1)
        text = (out / 'console.log').read_text(errors='replace')
        server = re.search(r'"sv_serverid" is: "(\d+)', text, re.I).group(1)
        for menu, response in [(4, 'close'), (1, 'allies'), (2, args.primary)]:
            send(f'cmd mr {server} {menu} {response}'); wait(1)
        send('noclip'); wait(1); send('cg_draw2d 1'); send('cg_drawGun 1')
        send('setviewpos 2569 2274 181 180'); wait(2); send('noclip'); send('god'); wait(2)
        send('developer 0'); send('record ws18_combat')
        start = time.monotonic()
        cycle = 0
        while time.monotonic() - start < args.seconds:
            for weapon in [args.primary, 'webley_mp']:
                send('-attack')
                if weapon != 'webley_mp':
                    send('weaponslot primary')
                else:
                    send('weaponslot primaryb')
                send('give ammo'); wait(1.5)
                send('screenshotJPEG ws18-' + weapon)
                for repeat in range(3):
                    send('+attack'); wait(1); send('-attack'); wait(.25)
                send('+reload'); wait(2); send('-reload')
                send('+forward'); wait(.4); send('-forward')
                send('+back'); wait(.4); send('-back')
            send('give ammo')
            send('+frag'); wait(.6); send('-frag'); wait(6)
            send('+smoke'); wait(.6); send('-smoke'); wait(8)
            send('screenshotJPEG ws18-smoke')
            send('setviewpos 2569 2274 181 180')
            cycle += 1
            print(f'combat cycle {cycle}, elapsed {time.monotonic() - start:.1f}s', flush=True)
        elapsed = time.monotonic() - start
        send('stoprecord'); wait(1); send('quit')
        process.wait(timeout=15)
        if process.returncode != 0:
            raise RuntimeError(f'quit failed: {process.returncode}')
        (out / 'results.json').write_text(json.dumps(dict(exit_code=process.returncode, primary=args.primary,
            combat_seconds=elapsed, cycles=cycle, events=events), indent=2) + '\n')
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
        (out / 'commands.json').write_text(json.dumps(events, indent=2) + '\n')
