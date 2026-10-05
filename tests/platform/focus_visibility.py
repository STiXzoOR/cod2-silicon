#!/usr/bin/env python3
"""Exercise an owned engine-only bundle using LaunchServices, without Accessibility."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('app', type=Path, help='a private engine-only test bundle')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--base', type=Path, default=Path.home() / 'Games/CoD2')
p.add_argument('--mode', choices=['exclusive', 'borderless'], required=True)
p.add_argument('--cycles', type=int, default=5)
p.add_argument('--sleep-wake', action='store_true', help='briefly sleep the display, then request user activity')
a = p.parse_args()
app, out = a.app.resolve(), a.output.expanduser().resolve()
if out == ROOT or ROOT in out.parents:
    p.error('Game captures must stay outside the repository')
if not (app / 'Contents/MacOS/cod2_macos').is_file() or not 1 <= a.cycles <= 20:
    p.error('Use an engine-only bundle and 1–20 focus cycles')
subprocess.run(['pgrep', '-fl', 'cod2_macos'], check=False)
if subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
    p.error('A game is already running; no process was changed')
out.mkdir(parents=True, exist_ok=False)
state_tool = out / 'window-state'
subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', 'swiftc',
                str(ROOT / 'tests/platform/window_state.swift'), '-o', str(state_tool)], check=True)
states = []
def snapshot(pid, label):
    value = json.loads(subprocess.check_output([str(state_tool), str(pid)], text=True))
    value['label'] = label
    states.append(value)
    print(label, value, flush=True)
    subprocess.run(['screencapture', '-x', '-C', str(out / (label + '.png'))], check=True)
    return value
desktop = snapshot(0, 'desktop')['display']
command = ['timeout', '-k', '5', '75', str(app / 'Contents/MacOS/cod2_macos'),
           '+set', 'fs_basepath', str(a.base.expanduser().resolve()), '+set', 'fs_homepath', str(out / 'home'),
           '+set', 'net_port', '29936', '+set', 'r_fullscreen', '1',
           '+set', 'r_borderless', str(int(a.mode == 'borderless')), '+set', 'r_mode', '1920x1080',
           '+set', 'developer', '1', '+set', 'com_introPlayed', '1', '+set', 'com_maxfps', '250']
with (out / 'console.log').open('w') as stream:
    game = subprocess.Popen(command, cwd=out, stdin=subprocess.PIPE, stdout=stream, stderr=stream,
                            text=True, start_new_session=True,
                            env=dict(os.environ, SDL_VIDEO_MAC_FULLSCREEN_SPACES='0'))
    try:
        deadline = time.monotonic() + 35
        while 'Common Initialization Complete' not in (out / 'console.log').read_text(errors='replace'):
            if game.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('Initialization failed')
            time.sleep(.1)
        children = subprocess.check_output(['pgrep', '-P', str(game.pid)], text=True).split()
        assert len(children) == 1
        pid = int(children[0])
        assert os.getpgid(pid) == game.pid
        def activate():
            subprocess.run(['open', '-a', str(app)], check=True)
            time.sleep(.8)
        def check_return(label):
            value = snapshot(pid, label)
            assert value['active'] and value['windows'], f'Missing active window: {label}'
            if a.mode == 'borderless':
                assert value['display'] == desktop
            else:
                assert value['display'] == [1920, 1080, 1920, 1080], 'Exclusive mode silently fell back'
            game.stdin.write('screenshotJPEG ' + label + '\n')
            game.stdin.flush()
        activate()
        check_return('start')
        for cycle in range(a.cycles):
            subprocess.run(['open', '-a', 'Finder'], check=True)
            time.sleep(.6)
            assert snapshot(pid, f'finder-{cycle}')['display'] == desktop
            activate()
            check_return(f'return-{cycle}')
        subprocess.run(['open', '-a', 'Mission Control'], check=True)
        time.sleep(.8)
        assert snapshot(pid, 'mission')['display'] == desktop, 'Exclusive display still captured while occluded'
        if a.mode == 'borderless':
            # Borderless does not change display modes, so its window remains
            # in Mission Control until the system overview is dismissed.
            subprocess.run(['open', '-a', 'Mission Control'], check=True)
            time.sleep(.6)
        # Opening the already active game does not dismiss Mission Control on
        # macOS. Activate another app first, then request the game again.
        subprocess.run(['open', '-a', 'Finder'], check=True)
        time.sleep(.6)
        activate()
        check_return('mission-return')
        if a.sleep_wake:
            subprocess.run(['pmset', 'displaysleepnow'], check=True)
            time.sleep(.5)
            subprocess.run(['caffeinate', '-u', '-t', '2'], check=True)
            time.sleep(2)
            activate()
            check_return('wake-return')
        game.stdin.write('quit\n')
        game.stdin.flush()
        game.wait(timeout=10)
        assert game.returncode == 0
    finally:
        if game.poll() is None:
            os.killpg(game.pid, signal.SIGTERM)
            try:
                game.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(game.pid, signal.SIGKILL)
                game.wait()
        subprocess.run(['open', '-a', 'Finder'], check=False)
        (out / 'states.json').write_text(json.dumps(states, indent=2) + '\n')
assert snapshot(0, 'released')['display'] == desktop
print('PASS: focus cycles, Mission Control, selected mode and desktop restoration')
