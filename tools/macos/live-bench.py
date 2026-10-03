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
parser.add_argument('--app', type=Path, help='launch a bundle through LaunchServices, with an owned stdin FIFO')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--resolution', default='1920x1080')
parser.add_argument('--window-mode', choices=['windowed', 'fullscreen', 'borderless'], default='windowed')
parser.add_argument('--maxfps', type=int, default=333)
parser.add_argument('--seconds', type=float, default=15)
parser.add_argument('--gpu-sync', type=int, choices=range(4), default=None)
parser.add_argument('--view-pos', type=float, nargs=4, default=[-299, 1001, 121, 90],
                    metavar=('X', 'Y', 'Z', 'YAW'), help='eye position and yaw (cheats)')
parser.add_argument('--record', help='record a demo basename during the measurement')
parser.add_argument('--combat', action='store_true', help='fire, reload, move and throw grenades throughout capture')
parser.add_argument('--profile', choices=['sample', 'xctrace'])
parser.add_argument('--cpu-profile', action='store_true', help='in-process main-thread sampler; FPS is perturbed')
args = parser.parse_args()
if args.app:
    if args.app.suffix != '.app' or not args.app.is_dir():
        parser.error('--app must name an existing .app')
    args.binary = args.app / 'Contents/MacOS/cod2_macos'
if not re.fullmatch(r'[1-9][0-9]{2,4}x[1-9][0-9]{2,4}', args.resolution):
    parser.error('resolution must be WIDTHxHEIGHT')
if not 0 < args.seconds <= 120 or not 0 <= args.maxfps <= 1000:
    parser.error('seconds in (0,120] and maxfps in [0,1000] required')
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
                '-framework', 'OpenGL', '-framework', 'CoreGraphics', '-o', str(probe)], check=True)
env = dict(os.environ, DYLD_INSERT_LIBRARIES=str(probe), COD2_FRAME_CSV=str(out / 'frames.csv'), COD2_FRAME_SECONDS=str(args.seconds))
env['COD2_FRAME_PID'] = str(out / 'observer.pid')
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
    if args.app:
        fifo = out / 'stdin.fifo'
        os.mkfifo(fifo)
        writer = os.fdopen(os.open(fifo, os.O_RDWR | os.O_NONBLOCK), 'w', buffering=1)
        launch = ['open', '-n', '-a', str(args.app.resolve()), '--stdin', str(fifo),
                  '--stdout', str(out / 'console.log'), '--stderr', str(out / 'console.log')]
        for name in ['DYLD_INSERT_LIBRARIES', 'COD2_FRAME_CSV', 'COD2_FRAME_SECONDS', 'COD2_FRAME_PID', 'COD2_CPU_PROFILE', 'SDL_VIDEO_MAC_FULLSCREEN_SPACES', 'MTL_HUD_ENABLED', 'COD2_MAC_SHADER_CACHE', 'COD2_D3D_PROG']:
            if name in env:
                launch += ['--env', name + '=' + env[name]]
        subprocess.run([*launch, '--args', *command[1:]], check=True, timeout=15)
        deadline = time.monotonic() + 15
        while not (out / 'observer.pid').exists():
            if time.monotonic() > deadline:
                writer.close()
                raise RuntimeError('LaunchServices did not start the observer; no unowned process was killed')
            time.sleep(.1)
        class OwnedApp:
            pid = int((out / 'observer.pid').read_text())
            stdin = writer
            returncode = None
            def poll(self):
                try:
                    os.kill(self.pid, 0)
                    return None
                except ProcessLookupError:
                    return -1
            def wait(self):
                self.stdin.close()
        process = OwnedApp()
    else:
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
        if not (out / 'observer.pid').exists() or int((out / 'observer.pid').read_text()) != process.pid:
            raise RuntimeError('frame observer was not loaded into the client; --binary must be the native executable')
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
        if args.combat:
            send('god')
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
        os.kill(process.pid, signal.SIGUSR1)
        wait_for('[frame-probe]')
        deadline = time.monotonic() + args.seconds + 10
        started = time.monotonic()
        actions = [(0, 'weaponslot primary'), (.1, '+attack'), (1.1, '-attack'),
                   (2, '+attack'), (3, '-attack'), (4, '+reload'), (6, '-reload'),
                   (7, '+forward'), (7.5, '-forward'), (8, '+back'), (8.5, '-back'),
                   (9, 'give ammo'), (10, '+frag'), (10.6, '-frag'),
                   (16, '+smoke'), (16.6, '-smoke')]
        action = 0
        while not (out / 'frames.csv').exists():
            if process.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('frame observer failed to complete')
            if args.combat:
                cycle, index = divmod(action, len(actions))
                if time.monotonic() - started >= cycle * 30 + actions[index][0]:
                    send(actions[index][1])
                    action += 1
            time.sleep(.1)
        if args.combat:
            for button in ['attack', 'reload', 'forward', 'back', 'frag', 'smoke']:
                send('-' + button)
        if args.record:
            send('stoprecord')
            time.sleep(1)
        send('screenshotJPEG ws13-live')
        time.sleep(1)
    finally:
        shutdown = 'already exited'
        if process.poll() is None:
            try:
                send('quit')
            except BrokenPipeError:
                pass
            deadline = time.monotonic() + 10
            while process.poll() is None and time.monotonic() < deadline:
                time.sleep(.1)
            shutdown = 'quit'
            if process.poll() is None:
                if args.app:
                    os.kill(process.pid, signal.SIGKILL)
                else:
                    os.killpg(process.pid, signal.SIGKILL)
                shutdown = 'owned process killed after quit timeout'
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
              main_thread_cpu_ms=statistics.mean(float(row['cpu_ms']) for row in rows),
              upload_calls=sum(int(row['upload_calls']) for row in rows),
              program_calls=sum(int(row['program_calls']) for row in rows),
              buffer_calls=sum(int(row['buffer_calls']) for row in rows),
              poll_calls_per_frame=statistics.mean(int(row['poll_calls']) for row in rows),
              resolution=args.resolution, window_mode=args.window_mode, maxfps=args.maxfps,
              requested_view_pos=args.view_pos,
              cpu_profile=args.cpu_profile,
              combat=args.combat, measured_seconds=sum(values) / 1000,
              app=str(args.app.resolve()) if args.app else None,
              shutdown=shutdown)
log = (out / 'console.log').read_text(errors='replace')
match = re.search(r'\[frame-probe\] ([^\n]+)', log)
result['actual_presentation'] = match.group(1) if match else None
match = re.search(r'\[frame-probe-display\] ([^\n]+)', log)
result['actual_display'] = match.group(1) if match else None
match = re.search(r'^\(([^)]+)\) : ([^\n]+)', log, re.M)
result['actual_view_pos'] = match.group(0) if match else None
result['presentation_changes'] = re.findall(r'\[frame-probe-change\] ([^\n]+)', log)
(out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
