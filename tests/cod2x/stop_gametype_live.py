#!/usr/bin/env python3
"""Bounded owned-data dedicated acceptance for a synthetic StopGameType callback."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--data', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--map', default='mp_toujane')
p.add_argument('--other-map', default='mp_carentan')
p.add_argument('--port', type=int, default=29967)
args = p.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
home = out / 'home'
game = home / 'ws81_stop'
script = game / 'maps/mp/gametypes/_callbacksetup.gsc'
script.parent.mkdir(parents=True)
original = None
for archive in sorted((args.data / 'main').glob('*.iwd')):
    with zipfile.ZipFile(archive) as z:
        if 'maps/mp/gametypes/_callbacksetup.gsc' in z.namelist():
            original = z.read('maps/mp/gametypes/_callbacksetup.gsc').decode('latin1')
if original is None:
    raise SystemExit('Owned data contains no callbacksetup script')
m = re.search(r'\bCodeCallback_StartGameType\s*\([^)]*\)\s*\{', original)
if not m:
    raise SystemExit('Cannot locate original callback startup')
original = original[:m.end()] + '\n level thread WS81_DriveTransitions();\n' + original[m.end():]
script.write_text(original + '''
CodeCallback_StopGameType(fromScript, complete, shutdown, source)
{
    println("WS81_STOP:" + fromScript + ":" + complete + ":" + shutdown + ":" + source);
    if (getcvarint("ws81_error")) randomint(0);
    return getcvarint("ws81_allow");
}
WS81_DriveTransitions()
{
    println("WS81_READY");
    for (;;)
    {
        wait 0.05;
        request = getcvar("ws81_request");
        if (request == "") continue;
        setcvar("ws81_request", "");
        if (request == "restart") map_restart(true);
        else if (request == "map") map("''' + args.map + '''", false);
        else if (request == "rotate") exitlevel(true);
    }
}
''')
binary = args.binary.resolve()
launch_command = [str(binary), '+set', 'fs_basepath', '"' + str(args.data.resolve()) + '"',
           '+set', 'fs_homepath', '"' + str(home) + '"', '+set', 'fs_game', 'ws81_stop',
           '+set', 'dedicated', '1', '+set', 'logfile', '2', '+set', 'sv_punkbuster', '0',
           '+set', 'developer', '1', '+set', 'developer_script', '1',
           '+set', 'net_ip', '127.0.0.1', '+set', 'net_port', str(args.port),
           '+set', 'g_gametype', 'dm', '+set', 'ws81_allow', '0',
           '+set', 'ws81_error', '0', '+map', args.map]
(out / 'run.json').write_text(json.dumps({'command': launch_command, 'binary_sha256':
    hashlib.sha256(binary.read_bytes()).hexdigest()}, indent=2))
receipts = []
with (out / 'console.log').open('w') as log:
    process = subprocess.Popen(launch_command, stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                               cwd=out, env=dict(os.environ), text=True, start_new_session=True)
    deadline = time.monotonic() + 150

    def console():
        return (out / 'console.log').read_text(errors='replace')

    def wait_for(predicate, description):
        until = min(deadline, time.monotonic() + 20)
        while time.monotonic() < until:
            text = console()
            if predicate(text):
                return text
            if process.poll() is not None:
                raise AssertionError('Engine exited before ' + description)
            time.sleep(0.1)
        raise AssertionError('Timed out waiting for ' + description)

    def send(text):
        assert process.poll() is None
        process.stdin.write(text + '\n')
        process.stdin.flush()

    def check_veto(command, marker):
        before = console()
        count = before.count(marker)
        initializations = before.count('------ Server Initialization ------')
        send(command)
        wait_for(lambda text: text.count(marker) == count + 1, marker)
        time.sleep(0.35)
        assert console().count('------ Server Initialization ------') == initializations
        receipts.append({'command': command, 'callback': marker, 'veto_kept_level': True})

    def reload_allowed(command, marker):
        # Each ask runs in a later server frame than the one before.
        time.sleep(0.2)
        before = console()
        ready = before.count('WS81_READY')
        count = before.count(marker)
        send('set ws81_allow 1')
        time.sleep(0.2)
        send(command)
        wait_for(lambda text: text.count('WS81_READY') == ready + 1, 'allowed ' + command)
        assert console().count(marker) == count + 1, marker
        send('set ws81_allow 0')
        time.sleep(0.2)
        receipts.append({'command': command, 'callback': marker, 'allowed': True})

    try:
        wait_for(lambda text: 'WS81_READY' in text, 'initial real script startup')
        # Console commands: asked when they run, not from a script.
        for command, source in [('map_restart', 'map_restart'), ('fast_restart', 'fast_restart'),
                                ('devmap ' + args.map, 'map'), ('map ' + args.map, 'map')]:
            check_veto(command, 'WS81_STOP:0:1:0:' + source)
            time.sleep(0.2)
        send('set sv_mapRotation "map ' + args.map + '"')
        check_veto('map_rotate', 'WS81_STOP:0:1:0:map_rotate')
        # Script requests: asked when the queued command runs. A block leaves the
        # level finished, so the next console map is still "from script".
        for request, source, complete in [('restart', 'fast_restart', 0), ('map', 'map', 1),
                                          ('rotate', 'map_rotate', 0)]:
            time.sleep(0.2)
            check_veto('set ws81_request ' + request, f'WS81_STOP:1:{complete}:0:' + source)
            reload_allowed('map ' + args.map, f'WS81_STOP:1:{complete}:0:map')
        before = console()
        ready = before.count('WS81_READY')
        stops = before.count('WS81_STOP:1:0:0:fast_restart')
        send('set ws81_allow 1')
        time.sleep(0.2)
        send('set ws81_request restart')
        wait_for(lambda text: text.count('WS81_READY') == ready + 1, 'allowed persistent script restart')
        assert console().count('WS81_STOP:1:0:0:fast_restart') == stops + 1
        receipts.append({'command': 'script map_restart(true)', 'allowed': True, 'callback_count': 1})
        time.sleep(0.2)
        before = console()
        shutdowns = before.count('----- Server Shutdown -----')
        ready = before.count('WS81_READY')
        send('set ws81_error 1')
        send('map ' + args.map)
        wait_for(lambda text: text.count('----- Server Shutdown -----') == shutdowns + 1, 'recoverable callback error teardown')
        assert 'RandomInt parm must be positive integer' in console()[len(before):]
        assert process.poll() is None
        send('sv_running')
        wait_for(lambda text: re.search(r'"sv_running"\s+is:\s*"0(?:\^7)?"', text[len(before):]), 'sv_running cleared after callback error')
        # Original shutdown removes operator commands, including map. Restart
        # the engine to test a fresh callback without adding registration policy.
        send('quit')
        process.wait(timeout=20)
        assert process.returncode == 0
        process = subprocess.Popen(launch_command, stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                                   cwd=out, env=dict(os.environ), text=True, start_new_session=True)
        wait_for(lambda text: text.count('WS81_READY') == ready + 1, 'fresh engine after callback error')
        check_veto('map ' + args.map, 'WS81_STOP:0:1:0:map')
        receipts.append({'command': 'StopGameType randomint(0), recover, fresh map and veto',
                         'server_torn_down': True, 'sv_running_cleared': True, 'fresh_engine_callback_checked': True})
        time.sleep(0.2)
        send('quit')
        process.wait(timeout=20)
        assert process.returncode == 0
        shutdowns = re.findall(r'WS81_STOP:[01]:[01]:1:shutdown', console())
        assert len(shutdowns) == 1
        receipts.append({'command': 'quit', 'integer_zero_ignored': True, 'callback_count': 1})
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        (out / 'receipt.json').write_text(json.dumps(receipts, indent=2))
print('real dedicated synthetic StopGameType acceptance:', len(receipts), 'cases passed')
