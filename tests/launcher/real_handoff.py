#!/usr/bin/env python3
"""Real-engine hand-offs through LaunchServices Play, Deploy and cold URLs."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import shutil
import subprocess
import time
import plistlib
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/macos'))
from bench_state import snapshot

def interrupted(signum, frame):
    raise KeyboardInterrupt('Owned hand-off test interrupted')

signal.signal(signal.SIGTERM, interrupted)

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('app', type=Path)
p.add_argument('--game', type=Path, required=True)
p.add_argument('--mac-binary', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--cycles', type=int, default=2)
a = p.parse_args()
app, out = a.app.resolve(), a.output.expanduser().resolve()
if out == ROOT or ROOT in out.parents or not 1 <= a.cycles <= 5:
    p.error('Use an external capture directory and 1–5 cycles')
out.mkdir(parents=True, exist_ok=False)
# An installed launcher may remain open. Give this packaged test copy its
# own identity so Cocoa activation cannot select that existing application.
test_app = out / 'Handoff.app'
shutil.copytree(app, test_app)
identifier = f'io.github.stixzoor.cod2silicon.handoff{os.getpid()}'
for bundle, bundle_id in ((test_app, identifier),
                          (test_app / 'Contents/Helpers/CoD2 Game.app', identifier + '.game')):
    plist = bundle / 'Contents/Info.plist'
    info = plistlib.loads(plist.read_bytes())
    info['CFBundleIdentifier'] = bundle_id
    plist.write_bytes(plistlib.dumps(info))
for bundle in (test_app / 'Contents/Helpers/CoD2 Game.app', test_app):
    subprocess.run(['codesign', '--force', '--sign', '-', str(bundle)], check=True)
app = test_app
probe, state_tool = out / 'deploy-probe.dylib', out / 'window-state'
subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', 'clang', '-dynamiclib', '-fobjc-arc', '-mmacosx-version-min=13.0',
                str(ROOT / 'tests/launcher/deploy_probe.m'), '-framework', 'AppKit', '-o', str(probe)], check=True)
subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', 'swiftc', str(ROOT / 'tests/platform/window_state.swift'), '-o', str(state_tool)], check=True)
unregister = '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'
results = []
try:
    for cycle in range(a.cycles):
        for route in ('play', 'deploy', 'url'):
            subprocess.run(['pgrep', '-fl', 'cod2_macos'], check=False)
            if subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
                raise RuntimeError('A game is already running; no process was changed')
            case = out / f'{route}-{cycle}'
            case.mkdir()
            before = snapshot(case, 'before')
            if before['screen_locked'] is not False:
                raise RuntimeError('Screen is locked or lock state is unknown; no game was launched')
            home = case / 'home'
            home.mkdir()
            log, cursor = case / 'console.log', case / 'cursor.json'
            environment = dict(os.environ, CFFIXED_USER_HOME=str(home), HOME=str(home),
                               COD2_SETUP_NONINTERACTIVE='1', COD2_SETUP_GAME_DIR=str(a.game.resolve()),
                               COD2_SETUP_MAC_BINARY=str(a.mac_binary.resolve()),
                               COD2_SETUP_CD_KEY='000000000000000086D3',
                               COD2_QA_WINDOW_TRACE=str(case / 'launcher-windows.jsonl'),
                               COD2_QA_CURSOR_PATH=str(cursor), DYLD_INSERT_LIBRARIES=str(probe))
            environment.pop('COD2_MAC_SHADER_CACHE', None)
            if route == 'deploy': environment['COD2_QA_DEPLOY'] = '1'
            forwarded = ['--', '+set', 'r_fullscreen', '1', '+set', 'r_borderless', '1',
                         '+set', 'r_mode', '1280x720', '+set', 'developer', '1',
                         '+set', 'com_introPlayed', '1', '+set', 'net_port', '29936']
            writer, launcher_pid, child = None, None, None
            log.touch()
            def owns_launcher(pid):
                try:
                    executable = subprocess.check_output(['ps', '-ww', '-p', str(pid), '-o', 'comm='], text=True).strip()
                    return executable == str(app / 'Contents/MacOS/CoD2Launcher')
                except subprocess.CalledProcessError:
                    return False
            def find_launcher():
                rows = subprocess.check_output(['ps', '-ww', '-A', '-o', 'pid=,comm='], text=True).splitlines()
                matches = [int(parts[0]) for row in rows if len(parts := row.strip().split(None, 1)) == 2
                           and parts[1] == str(app / 'Contents/MacOS/CoD2Launcher')]
                assert len(matches) <= 1, 'Multiple launchers use this private executable'
                return matches[0] if matches else None
            def owns_child(pid):
                try:
                    command = subprocess.check_output(['ps', '-p', str(pid), '-o', 'args='], text=True)
                    status = subprocess.check_output(['ps', '-p', str(pid), '-o', 'stat='], text=True).strip()
                except subprocess.CalledProcessError:
                    return False
                if status.startswith('Z'): return False
                if str(home) not in command or str(app / 'Contents/Helpers/CoD2 Game.app/Contents/MacOS/cod2_macos') not in command:
                    raise RuntimeError('Refusing to stop a PID without this launch\'s helper and private home')
                return True
            try:
                def wait_for(predicate, seconds=45):
                    deadline = time.monotonic() + seconds
                    while not predicate():
                        if time.monotonic() > deadline:
                            raise RuntimeError(f'Timed out or launcher exited; inspect {log}')
                        time.sleep(.1)
                fifo = case / 'stdin.fifo'
                os.mkfifo(fifo)
                writer = os.fdopen(os.open(fifo, os.O_RDWR | os.O_NONBLOCK), 'w', buffering=1)
                command = ['open', '-n', '-a', str(app), '--stdin', str(fifo),
                           '--stdout', str(log), '--stderr', str(log)]
                for key in ('CFFIXED_USER_HOME', 'HOME', 'COD2_SETUP_NONINTERACTIVE',
                            'COD2_SETUP_GAME_DIR', 'COD2_SETUP_MAC_BINARY', 'COD2_SETUP_CD_KEY',
                            'COD2_QA_CURSOR_PATH', 'COD2_QA_WINDOW_TRACE', 'COD2_QA_DEPLOY',
                            'DYLD_INSERT_LIBRARIES'):
                    if key in environment: command += ['--env', key + '=' + environment[key]]
                if route == 'url': command.append('cod2x://connect/127.0.0.1:29937')
                command += ['--args', *(['--play'] if route == 'play' else []), *forwarded]
                subprocess.run(['timeout', '-k', '5', '15', *command], check=True, timeout=22)
                wait_for(find_launcher, 10)
                launcher_pid = find_launcher()
                wait_for(lambda: 'Common Initialization Complete' in log.read_text(errors='replace'))
                child = int(re.search(r'game started \(pid (\d+)\)', log.read_text(errors='replace'))[1])
                launcher_pid = int(subprocess.check_output(['ps', '-p', str(child), '-o', 'ppid='], text=True))
                assert owns_launcher(launcher_pid), 'Game parent is not this test launcher'
                def state(pid):
                    return json.loads(subprocess.check_output([str(state_tool), str(pid)], text=True))
                wait_for(lambda: state(child)['active'] and cursor.exists(), 10)
                game_state = state(child)
                assert game_state['windows'], 'Active game has no visible window'
                assert json.loads(cursor.read_text()) == dict(system_cursor_hidden=True, relative_mouse=True)
                if route == 'deploy':
                    assert '[deploy-probe] invoked Deploy menu action' in log.read_text(errors='replace')
                if route == 'url':
                    wait_for(lambda: '127.0.0.1:29937' in log.read_text(errors='replace'), 10)
                subprocess.run(['screencapture', '-x', '-C', str(case / 'game.png')], check=True)
                writer.write('quit\n')
                writer.flush()
                wait_for(lambda: 'returned to launcher (game exit 0)' in log.read_text(errors='replace'), 15)
                def return_state():
                    value = state(launcher_pid)
                    (case / 'return-state.json').write_text(json.dumps(value, indent=2))
                    return value['active'] and bool(value['windows'])
                wait_for(return_state, 10)
                result = dict(route=route, cycle=cycle, game=game_state, launcher=state(launcher_pid),
                              system_cursor_hidden=True, relative_mouse=True, game_exit=0,
                              machine_state=dict(before=before, after=snapshot(case, 'after')))
                assert result['machine_state']['after']['screen_locked'] is False
                results.append(result)
                (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                print('PASS:', route, cycle, 'game focus, captured single cursor, launcher return', flush=True)
            finally:
                # Only PIDs from this launch's child record or its process group
                # are owned. Leave existing installed launchers untouched.
                if launcher_pid is None:
                    launcher_pid = find_launcher()
                if child is None:
                    match = re.search(r'game started \(pid (\d+)\)', log.read_text(errors='replace'))
                    if match:
                        child = int(match[1])
                        try:
                            parent = int(subprocess.check_output(['ps', '-p', str(child), '-o', 'ppid='], text=True))
                            if owns_launcher(parent): launcher_pid = parent
                        except subprocess.CalledProcessError: pass
                if child and owns_child(child):
                    try: os.kill(child, signal.SIGTERM)
                    except ProcessLookupError: pass
                    deadline = time.monotonic() + 5
                    while owns_child(child) and time.monotonic() < deadline: time.sleep(.1)
                    if owns_child(child):
                        try: os.kill(child, signal.SIGKILL)
                        except ProcessLookupError: pass
                if launcher_pid and owns_launcher(launcher_pid):
                    try: os.kill(launcher_pid, signal.SIGTERM)
                    except ProcessLookupError: pass
                if writer: writer.close()
finally:
    for bundle in (app, app / 'Contents/Helpers/CoD2 Game.app'):
        subprocess.run([unregister, '-u', str(bundle)], check=False)
