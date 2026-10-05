#!/usr/bin/env python3
"""Sweep fixed Toujane cameras; keep all game captures outside the repository."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('binary', type=Path)
p.add_argument('--base', type=Path, default=Path.home() / 'Games/CoD2')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--port', type=int, default=29936)
a = p.parse_args()
out = a.output.expanduser().resolve()
if out == ROOT or ROOT in out.parents:
    p.error('Captures must stay outside the repository')
if not 1024 <= a.port <= 65535:
    p.error('Use a private unprivileged port')
subprocess.run(['pgrep', '-fl', 'cod2_macos'], check=False)
if subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
    p.error('A game is already running; no process was changed')
if any(c in str(path) for path in (out, a.base) for c in '";\r\n'):
    p.error('Paths must be safe console arguments')
out.mkdir(parents=True, exist_ok=False)
analyzer = out / 'visibility-luma'
subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', 'swiftc',
                str(ROOT / 'tests/rendering/visibility_luma.swift'), '-o', str(analyzer)], check=True)
request = out / 'draw.request'
env = dict(os.environ, COD2_MAC_DRAW_TRACE=str(request))
command = ['timeout', '-k', '5', '110', str(a.binary.resolve()),
           '+set', 'fs_basepath', str(a.base.expanduser().resolve()),
           '+set', 'fs_homepath', str(out / 'home'), '+set', 'net_port', str(a.port),
           '+set', 'r_fullscreen', '0', '+set', 'r_mode', '1280x720',
           '+set', 'developer', '1', '+set', 'com_introPlayed', '1',
           '+set', 'com_maxfps', '125', '+set', 'sv_pure', '0',
           '+set', 'sv_maxclients', '1', '+set', 'g_gametype', 'dm', '+devmap', 'mp_toujane']
log = out / 'console.log'
frames = []
with log.open('w') as stream:
    game = subprocess.Popen(command, env=env, cwd=out, stdin=subprocess.PIPE,
                            stdout=stream, stderr=stream, text=True, start_new_session=True)
    def send(command):
        game.stdin.write(command + '\n')
        game.stdin.flush()
    def wait_for(predicate, seconds=35):
        deadline = time.monotonic() + seconds
        while not predicate():
            if game.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError(f'Game exited or timed out; inspect {log}')
            time.sleep(.025)
    try:
        wait_for(lambda: 'Going from CS_PRIMED to CS_ACTIVE' in log.read_text(errors='replace'))
        send('sv_serverId')
        send('configstrings')
        wait_for(lambda: '1250: serverinfo_dm' in log.read_text(errors='replace'))
        server = re.search(r'"sv_serverid" is: "(\d+)', log.read_text(errors='replace'), re.I).group(1)
        for menu, response in [(4, 'close'), (1, 'allies'), (2, 'enfield_mp')]:
            send(f'cmd mr {server} {menu} {response}')
            time.sleep(.6)
        for command in ['noclip', 'cg_nopredict 1', 'bg_bobMax 0', 'cg_drawGun 0', 'cg_draw2D 0', 'developer 0']:
            send(command)
        time.sleep(2)
        # Keep user mouse motion out of the automated camera while rendering.
        subprocess.run(['open', '-a', 'Finder'], check=True)
        # Eye heights above the street and roof, with visible ground below.
        for pose, position in enumerate([(2569, 2274, 181), (3051, 2178, 220)]):
            for pitch in range(10, 21, 5):
                previous = None
                for yaw in range(170, 191, 2):
                    name = f'floor-{pose}-{pitch}-{yaw}'
                    send('setviewpos ' + ' '.join(map(str, (*position, yaw, pitch))))
                    time.sleep(.35)
                    trace = out / (name + '.jsonl')
                    # The renderer removes each request as soon as it reads it.
                    # Publish atomically so it cannot consume an empty file.
                    pending = out / 'draw.pending'
                    pending.write_text(str(trace) + '\n')
                    pending.replace(request)
                    shot = out / 'home/main/screenshots' / (name + '.jpg')
                    send('screenshotJPEG ' + name)
                    wait_for(lambda: shot.exists() and trace.exists() and not request.exists(), 10)
                    time.sleep(.03)
                    draws = [json.loads(row) for row in trace.read_text().splitlines()]
                    world = [row for row in draws if not row['2d']]
                    assert world, 'No world draws in the captured frame'
                    origin = world[0]['origin']
                    assert origin and all(abs(actual - expected) < 2 for actual, expected in zip(origin[:2], position[:2])), \
                        f'Camera did not settle: requested {position}, rendered {origin}'
                    if previous:
                        assert all(abs(actual - expected) < 1 for actual, expected in zip(origin, previous['view_origin'])), \
                            'Camera position changed between adjacent angles'
                    luma = json.loads(subprocess.check_output([str(analyzer), str(shot)], text=True))
                    frame = dict(pose=pose, requested_eye=position, yaw=yaw, pitch=pitch,
                                 view_origin=origin,
                                 screenshot=str(shot), world_draws=len(world),
                                 world_primitives=sum(row['primitives'] for row in world), **luma)
                    frame['flags'] = []
                    if previous:
                        for metric in ['floor_luma', 'world_draws', 'world_primitives']:
                            if previous[metric] > 0 and frame[metric] < previous[metric] * .5:
                                frame['flags'].append(metric + ' fell by over 50%')
                    frames.append(frame)
                    previous = frame
                    print(name, round(frame['floor_luma'], 2), len(world), frame['flags'], flush=True)
        send('quit')
        game.wait(timeout=10)
        if game.returncode:
            raise RuntimeError(f'Game exited {game.returncode}')
    finally:
        if game.poll() is None:
            # Only this command's process group is owned by the sweep.
            os.killpg(game.pid, signal.SIGTERM)
            try:
                game.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(game.pid, signal.SIGKILL)
                game.wait()
result = dict(frames=frames, flagged=sum(bool(row['flags']) for row in frames), shutdown='quit',
              note='World draws are 3D batches, not individual BSP surfaces; flags need visual review.')
(out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print(f"Captured {len(frames)} frames; {result['flagged']} abrupt-change candidates")
