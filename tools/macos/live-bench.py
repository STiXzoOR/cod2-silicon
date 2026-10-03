#!/usr/bin/env python3
"""Measure a local Toujane client with an optional, separately loaded observer."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('data', type=Path)
parser.add_argument('--binary', type=Path, default=ROOT / 'build-macos/cod2_macos')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--resolution', default='1920x1080')
parser.add_argument('--window-mode', choices=['windowed', 'fullscreen', 'borderless'], default='windowed')
parser.add_argument('--maxfps', type=int, default=333)
parser.add_argument('--seconds', type=float, default=15)
parser.add_argument('--gpu-sync', type=int, choices=range(4), default=None)
parser.add_argument('--view-pos', type=float, nargs=4, default=[-299, 1001, 121, 90],
                    metavar=('X', 'Y', 'Z', 'YAW'), help='eye position and yaw (cheats)')
parser.add_argument('--record', help='record a demo basename during the measurement')
parser.add_argument('--profile', choices=['sample', 'xctrace'])
parser.add_argument('--cpu-profile', action='store_true', help='in-process main-thread sampler; FPS is perturbed')
args = parser.parse_args()
if not re.fullmatch(r'[1-9][0-9]{2,4}x[1-9][0-9]{2,4}', args.resolution):
    parser.error('resolution must be WIDTHxHEIGHT')
if not 0 < args.seconds <= 60 or not 0 <= args.maxfps <= 1000:
    parser.error('seconds in (0,60] and maxfps in [0,1000] required')
if not all(math.isfinite(value) for value in args.view_pos):
    parser.error('view position must be finite')
if args.record and not re.fullmatch(r'[A-Za-z0-9_-]+', args.record):
    parser.error('record must be a simple demo basename')
for path in (args.data, args.binary, args.output):
    if any(char in str(path) for char in '\";+\r\n'):
        parser.error('paths cannot contain quotes, semicolons, plus signs, or newlines')
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
profile = out / 'home/raw/players/default'
profile.mkdir(parents=True)
full = int(args.window_mode != 'windowed')
borderless = int(args.window_mode == 'borderless')
(profile / 'config_mp.cfg').write_text(f'seta r_mode "{args.resolution}"\nseta r_fullscreen "{full}"\nseta r_borderless "{borderless}"\n')
probe = out / 'frame-probe.dylib'
flags = subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True).split()
subprocess.run(['clang', '-dynamiclib', '-O2', '-Wno-deprecated-declarations', *flags,
                str(ROOT / 'tools/macos/frame-probe.c'), str(ROOT / 'tools/macos/stack-probe.c'),
                '-framework', 'OpenGL', '-o', str(probe)], check=True)
env = dict(os.environ, DYLD_INSERT_LIBRARIES=str(probe), COD2_FRAME_CSV=str(out / 'frames.csv'), COD2_FRAME_SECONDS=str(args.seconds))
if args.cpu_profile:
    env['COD2_CPU_PROFILE'] = str(out / 'cpu-stacks.csv')
command = [str(args.binary.resolve()), '+set', 'fs_basepath', '"' + str(args.data.resolve()) + '"',
           '+set', 'fs_homepath', '"' + str(out / 'home') + '"',
           '+set', 'r_mode', args.resolution, '+set', 'r_fullscreen', str(full),
           '+set', 'r_borderless', str(borderless), '+set', 'r_swapInterval', '0',
           '+set', 'developer', '1', '+set', 'com_introPlayed', '1',
           '+set', 'com_maxfps', str(args.maxfps), '+set', 'cg_drawFPS', '1',
           '+set', 'sv_pure', '0', '+set', 'g_gametype', 'dm', '+set', 'sv_maxclients', '1',
           '+devmap', 'mp_toujane']
if args.gpu_sync is not None:
    command[1:1] = ['+set', 'r_gpuSync', str(args.gpu_sync)]
(out / 'launch.json').write_text(json.dumps(dict(command=command, metal_hud=env.get('MTL_HUD_ENABLED', '0')), indent=2) + '\n')
with (out / 'console.log').open('w') as stream:
    process = subprocess.Popen(command, env=env, stdin=subprocess.PIPE, stdout=stream,
                               stderr=stream, text=True, cwd=out, start_new_session=True)
    (out / 'client.pid').write_text(str(process.pid))
    def send(text):
        process.stdin.write(text + '\n')
        process.stdin.flush()
    def wait_for(text, timeout=60):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            log = (out / 'console.log').read_text(errors='replace')
            if text in log:
                return log
            if process.poll() is not None:
                raise RuntimeError(f'client exited {process.returncode}: {out}')
            time.sleep(.1)
        raise RuntimeError(f'timeout waiting for {text}: {out}')
    try:
        wait_for('Going from CS_PRIMED to CS_ACTIVE')
        send('sv_serverId')
        send('configstrings')
        log = wait_for('1250: serverinfo_dm')
        match = re.search(r'"sv_serverid" is: "(\d+)', log, re.I)
        if not match:
            raise RuntimeError('server ID not printed')
        server_id = match.group(1)
        for menu, response in [(4, 'close'), (1, 'allies'), (2, 'enfield_mp')]:
            send(f'cmd mr {server_id} {menu} {response}')
            time.sleep(1)
        send('developer 0')
        send('cg_drawFPS Simple')
        send('cg_drawFPS')
        for cvar in ['r_mode', 'r_fullscreen', 'r_borderless', 'r_gpuSync']:
            send(cvar)
        # Toujane mp_dm_spawn at -299 1001 61, yaw 90, from the
        # stock BSP entity lump. setviewpos takes eye height (origin + 60).
        send('setviewpos ' + ' '.join(str(value) for value in args.view_pos))
        time.sleep(5)
        send('viewpos')
        if args.profile:
            cmd = ['sample', str(process.pid), '3', '1', '-file', str(out / 'profile.txt')] if args.profile == 'sample' else [
                'xcrun', 'xctrace', 'record', '--template', 'Time Profiler', '--attach', str(process.pid),
                '--time-limit', '5s', '--output', str(out / 'profile.trace')]
            try:
                with (out / 'profile.log').open('w') as log:
                    subprocess.run(cmd, stdout=log, stderr=log, timeout=60)
            except subprocess.TimeoutExpired:
                (out / 'profile-timeout.txt').write_text('profiler did not finish in 60 seconds\n')
        if args.record:
            send('record ' + args.record)
            time.sleep(1)
        os.kill(process.pid, signal.SIGUSR2)
        wait_for('[frame-probe]')
        deadline = time.monotonic() + args.seconds + 10
        while not (out / 'frames.csv').exists():
            if process.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('frame observer failed to complete')
            time.sleep(.1)
        if args.record:
            send('stoprecord')
            time.sleep(1)
        send('screenshotJPEG ws13-live')
        time.sleep(1)
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
        process.wait()
with (out / 'frames.csv').open() as stream:
    rows = list(csv.DictReader(stream))
values = [float(row['interval_ms']) for row in rows]
if not values:
    raise RuntimeError('no frame samples')
ordered = sorted(values)
slow = ordered[-max(1, math.ceil(len(values) * .01)):]
result = dict(samples=len(values), fps=1000 / statistics.mean(values),
              one_percent_low=1000 / statistics.mean(slow),
              frame_ms_p99=ordered[math.ceil(len(values) * .99) - 1], frame_ms_max=max(values),
              milliseconds_histogram={str(value): sum(int(row['engine_ms']) == value for row in rows)
                                      for value in sorted({int(row['engine_ms']) for row in rows})},
              phases_ms={name: statistics.mean(float(row[name]) for row in rows)
                         for name in ['swap_ms', 'poll_ms', 'blit_ms', 'clear_ms', 'fence_ms']},
              poll_calls_per_frame=statistics.mean(int(row['poll_calls']) for row in rows),
              resolution=args.resolution, window_mode=args.window_mode, maxfps=args.maxfps,
              requested_view_pos=args.view_pos,
              cpu_profile=args.cpu_profile,
              shutdown='owned process killed; listen-server quit has a known hang')
log = (out / 'console.log').read_text(errors='replace')
match = re.search(r'\[frame-probe\] ([^\n]+)', log)
result['actual_presentation'] = match.group(1) if match else None
match = re.search(r'^\(([^)]+)\) : ([^\n]+)', log, re.M)
result['actual_view_pos'] = match.group(0) if match else None
result['presentation_changes'] = re.findall(r'\[frame-probe-change\] ([^\n]+)', log)
(out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
