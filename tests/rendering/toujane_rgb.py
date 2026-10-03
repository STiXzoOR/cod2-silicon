#!/usr/bin/env python3
"""Capture the stock Toujane DM intro and check world RGB (macOS SDK only).

The game and all output stay outside git. Run from the repository root:
python3 tests/rendering/toujane_rgb.py build-macos/cod2_macos --home /outside/git/test-home
"""
import argparse
from pathlib import Path
import subprocess
import time

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('binary', type=Path)
p.add_argument('--base', type=Path, default=Path.home() / 'Games/CoD2')
p.add_argument('--home', type=Path, required=True)
a = p.parse_args()
home = a.home.expanduser().resolve()
if home == root or root in home.parents:
    p.error('Game output must be outside the repository')
home.mkdir(parents=True, exist_ok=True)
shot = home / 'main/screenshots/ws15-rgb-regression.jpg'
shot.unlink(missing_ok=True)
args = [str(a.binary.resolve()), '+set', 'fs_basepath', str(a.base.expanduser().resolve()),
        '+set', 'fs_homepath', str(home), '+set', 'net_port', '28515',
        '+set', 'r_fullscreen', '0', '+set', 'r_mode', '1280x720',
        '+set', 'developer', '1', '+set', 'com_introPlayed', '1',
        '+set', 'com_maxfps', '125', '+set', 'dedicated', '0',
        '+set', 'sv_pure', '0', '+set', 'sv_maxclients', '1',
        '+set', 'g_gametype', 'dm', '+devmap', 'mp_toujane']
log = home / 'rgb-regression.log'
with log.open('wb') as output:
    game = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT, cwd=home)
    try:
        deadline = time.monotonic() + 90
        while 'Going from CS_PRIMED to CS_ACTIVE' not in log.read_text(errors='replace'):
            if game.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError(f'Map did not become active; see {log}')
            time.sleep(.1)
        game.stdin.write(b'screenshotJPEG ws15-rgb-regression\n')
        game.stdin.flush()
        while not shot.exists():
            if game.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError(f'Screenshot missing; see {log}')
            time.sleep(.1)
        subprocess.run(['swift', str(root / 'tests/rendering/toujane_rgb.swift'), str(shot)], check=True)
    finally:
        game.terminate()
        try:
            game.wait(timeout=3)
        except subprocess.TimeoutExpired:
            game.kill()
            game.wait()
print(f'PASS Toujane intro RGB: {shot}')
