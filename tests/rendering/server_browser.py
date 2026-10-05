#!/usr/bin/env python3
"""Refresh real masters and trace Join Server hover using engine menu actions."""
import argparse
import csv
import json
import math
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
    p.error('Game captures must stay outside the repository')
if not 1024 <= a.port <= 65535 or any(c in str(out) + str(a.base) for c in '";\r\n'):
    p.error('Use a private port and safe console paths')
subprocess.run(['pgrep', '-fl', 'cod2_macos'], check=False)
if subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
    p.error('A game is already running; no process was changed')
out.mkdir(parents=True, exist_ok=False)
probe = out / 'ui-probe.dylib'
flags = subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True).split()
subprocess.run(['clang', '-dynamiclib', '-O2', '-DCOD2_X64=1', '-DCOD2_CODX=1',
                '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'src/headers'),
                '-Wno-typedef-redefinition', '-Wno-duplicate-decl-specifier', *flags,
                str(ROOT / 'tests/platform/ui_probe.c'), '-o', str(probe)], check=True)
command = ['timeout', '-k', '5', '50', str(a.binary.resolve()),
           '+set', 'fs_basepath', str(a.base.expanduser().resolve()),
           '+set', 'fs_homepath', str(out / 'home'), '+set', 'net_port', str(a.port),
           '+set', 'r_fullscreen', '0', '+set', 'r_mode', '1920x1080',
           '+set', 'developer', '1', '+set', 'com_introPlayed', '1',
           '+set', 'com_maxfps', '250', '+set', 'ui_netSource', '1']
log = out / 'console.log'
with log.open('w') as stream:
    game = subprocess.Popen(command, env=dict(os.environ, DYLD_INSERT_LIBRARIES=str(probe)),
                            cwd=out, stdin=subprocess.PIPE, stdout=stream, stderr=stream,
                            text=True, start_new_session=True)
    def send(command):
        game.stdin.write(command + '\n')
        game.stdin.flush()
    def wait_for(predicate, seconds=30):
        deadline = time.monotonic() + seconds
        while not predicate():
            if game.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError(f'Game exited or timed out; inspect {log}')
            time.sleep(.1)
    try:
        wait_for(lambda: 'Common Initialization Complete' in log.read_text(errors='replace'))
        time.sleep(1)  # The main menu is opened on its first UI frame.
        send('qa_join')
        send('qa_menu_script RefreshServers')
        wait_for(lambda: 'servers listed in browser' in log.read_text(errors='replace'), 20)
        send('qa_ui_state')
        send('screenshotJPEG join-refreshed')
        time.sleep(1)
        send('qa_hover')
        send('qa_ui_trace hover.csv')
        time.sleep(3)
        send('screenshotJPEG join-hover')
        time.sleep(1)
        send('quit')
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
rows = list(csv.DictReader((out / 'hover.csv').open()))
assert len(rows) >= 100, 'Incomplete presentation trace'
assert all(int(row['flags']) & 2 for row in rows), 'Refresh List did not hold focus'
assert len({(row['cursor_x'], row['cursor_y']) for row in rows}) == 1, 'Cursor moved during hover'
# Compare the rendered RGBA against the original Mac 1.3 integer phase. This
# rejects both a constant colour and the former /22 rapid pulse.
max_error = 0
for row in rows:
    intensity = 1 - .2 * (math.sin(int(row['time']) // 75) * .5 + .5)
    for channel in 'rgba':
        expected = float(row['focus_' + channel]) * intensity
        max_error = max(max_error, abs(float(row[channel]) - expected))
assert max_error < .00001, f'Unexpected pulse cadence or colour: {max_error}'
assert min(float(row['a']) for row in rows) >= .79, 'Unexpected text transparency'
match = re.search(r'\[ui-probe\] displayed=(\d+) refresh=(\d+) players=(\d+) gametypes=(\d+)',
                  log.read_text(errors='replace'))
assert match and int(match[1]) > 0 and int(match[4]) > 0, 'Empty live browser'
result = dict(displayed=int(match[1]), players=int(match[3]), game_types=int(match[4]),
              hover_frames=len(rows), focus_flags=sorted({int(row['flags']) for row in rows}),
              pulse_divisor_ms=75, pulse_max_error=max_error, shutdown='quit',
              note='Menu actions and colours sampled at each swap; this is not a latency benchmark.')
(out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
