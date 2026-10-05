#!/usr/bin/env python3
"""Open every runtime-loaded menu in the front end and a local listen game.

Uses stdin console commands and the opt-in COD2_UI_SWEEP diagnostics. No menu
names or scripts are extracted into the repository. Captures/logs stay outside
git. Refuses to launch while another cod2_macos is running; only terminates its
own timeout process group. Not a benchmark.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
# Keep every handled name accounted for, including commands that cannot be
# exercised safely in a short windowed run with a private writable home.
SKIPS = {
    'Quit': 'terminates the process',
    'RunMod': 'changes the mod and restarts the renderer',
    'JoinServer': 'requires an external selected server',
    'StartServer': 'covered separately by loading mp_toujane',
    'playMovie': 'requires a selected licensed ROQ, absent from the test install',
    'startSingleplayer': 'launches a separate application',
}
ARGUMENTS = {
    'ServerSort': '0',
    'update': 'ui_GetName',
    'openMenuOnDvar': 'ui_sweep_match 1 main',
    'openMenuOnDvarNot': 'ui_sweep_match 0 main',
    'closeMenuOnDvar': 'ui_sweep_match 1 main',
    'closeMenuOnDvarNot': 'ui_sweep_match 0 main',
}


def handled_names():
    source = (ROOT / 'src/PC/ui_mp/ui_main_mp.c').read_text()
    source = source[source.index('void UI_RunMenuScript(const char **args)\n{'):]
    source = source[:source.index('\nvoid ', 1)]
    return list(dict.fromkeys(re.findall(r'I_stricmp\(name, "([^"]+)"\)', source)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('binary', type=Path)
    p.add_argument('--base', type=Path, default=Path.home() / 'Games/CoD2')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--port', type=int, default=29938)
    a = p.parse_args()
    out = a.output.expanduser().resolve()
    if out == ROOT or ROOT in out.parents:
        p.error('All captures and game output must stay outside the repository')
    if not 1024 <= a.port <= 65535:
        p.error('Use a private unprivileged port')
    if any(c in str(path) for path in (out, a.base) for c in '";\r\n'):
        p.error('Paths must be safe console arguments')
    subprocess.run(['pgrep', '-fl', 'cod2_macos'], check=False)
    if subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
        p.error('Another game is running; no process was changed')
    out.mkdir(parents=True, exist_ok=False)
    log = out / 'console.log'
    home = out / 'home'
    result = dict(menus={}, scripts=[], skipped=SKIPS, completed=False)
    command = ['timeout', '-k', '5', '180', str(a.binary.resolve()),
               '+set', 'fs_basepath', str(a.base.expanduser().resolve()),
               '+set', 'fs_homepath', str(home), '+set', 'net_port', str(a.port),
               '+set', 'r_fullscreen', '0', '+set', 'r_mode', '640x480',
               '+set', 'developer', '1', '+set', 'com_introPlayed', '1',
               '+set', 'com_maxfps', '60', '+set', 'dedicated', '0',
               '+set', 'sv_pure', '0', '+set', 'sv_maxclients', '1',
               '+set', 'g_gametype', 'dm', '+set', 'ui_netSource', '0']
    with log.open('w') as stream:
        game = subprocess.Popen(command, cwd=out, env=dict(os.environ, COD2_UI_SWEEP='1'),
                                stdin=subprocess.PIPE, stdout=stream, stderr=stream,
                                text=True, start_new_session=True)

        def text():
            return log.read_text(errors='replace')

        def healthy():
            if game.poll() is not None:
                raise RuntimeError(f'Game exited {game.returncode}; inspect {log}')
            if re.search(r'Sys_Error|Fatal error|FATAL ERROR|signal=(?:6|10|11)', text()):
                raise RuntimeError(f'Fatal diagnostic; inspect {log}')

        def wait(predicate, seconds=15):
            deadline = time.monotonic() + seconds
            while not predicate():
                healthy()
                if time.monotonic() > deadline:
                    raise RuntimeError(f'Command timed out; inspect {log}')
                time.sleep(.025)
            healthy()

        def send(command):
            healthy()
            game.stdin.write(command + '\n')
            game.stdin.flush()

        def ack(command, marker, seconds=15):
            start = len(text())
            send(command)
            wait(lambda: marker in text()[start:], seconds)

        def sweep(context):
            start = len(text())
            send('ui_sweep list')
            wait(lambda: '[ui-sweep] listed ' in text()[start:])
            names = re.findall(r'^\[ui-sweep\] menu (.+)$', text()[start:], re.M)
            count = int(re.search(r'\[ui-sweep\] listed (\d+)', text()[start:])[1])
            assert len(names) == count and len(set(names)) == count, 'Incomplete/duplicate menu inventory'
            assert count >= 59, f'Expected at least 59 loaded menus, found {count}'
            result['menus'][context] = []
            for i, name in enumerate(names):
                if not re.fullmatch(r'[\w.-]+', name):
                    raise RuntimeError('Menu name is not a safe console argument')
                ack('ui_sweep closeall', '[ui-sweep] closed all')
                ack('openmenu ' + name, f'[ui-sweep] open {name} 1')
                # Let the onOpen commands and one rendered frame finish.
                time.sleep(.12)
                label = f'ws38-{context}-{i:02d}-{name}'
                shot = home / 'main/screenshots' / (label + '.jpg')
                send('screenshotJPEG ' + label)
                wait(lambda: shot.exists() and shot.stat().st_size > 0)
                ack('closemenu ' + name, f'[ui-sweep] close {name}')
                result['menus'][context].append(dict(name=name, screenshot=str(shot)))
                print(f'{context}: {i + 1}/{count} {name}', flush=True)

        try:
            wait(lambda: 'UI_Init' in text() or 'menus loaded' in text(), 40)
            sweep('main')
            # Harmless preconditions; profile commands never target a user's
            # profile because all names are empty and fs_homepath is fresh.
            send('set ui_sweep_match 1; set ui_playerProfileNameNew ""; set ui_playerProfileSelected ""')
            for name in handled_names():
                if name in SKIPS:
                    continue
                script = name + (' ' + ARGUMENTS[name] if name in ARGUMENTS else '')
                command = 'ui_sweep script ' + script
                if name == 'resetDefaults':
                    command += '; set r_fullscreen 0; set r_mode 640x480; set ui_netSource 0; set com_maxfps 60'
                ack(command, f'[ui-sweep] script {name} done', 30)
                time.sleep(.15)
                healthy()
                result['scripts'].append(name)
            for subcommand in ['ui_SetName', 'ui_setRate', 'ui_mousePitch']:
                ack('ui_sweep script update ' + subcommand, '[ui-sweep] script update done')
            ack('ui_sweep script loadGameInfo', '[ui-sweep] script loadGameInfo done')
            ack('ui_sweep script loadArenas', '[ui-sweep] script loadArenas done')
            ack('ui_sweep map mp_toujane', '[ui-sweep] selected map mp_toujane')
            start = len(text())
            send('set dedicated 0; set ui_dedicated 0; set sv_pure 0; set sv_maxclients 1; ui_sweep script StartServer')
            wait(lambda: 'Going from CS_PRIMED to CS_ACTIVE' in text()[start:], 50)
            result['scripts'].append('StartServer')
            sweep('ingame')
            healthy()
            result['completed'] = True
            # The test owns this short process. Avoid the known unrelated
            # listen-server script-shutdown hang after acceptance checks.
        except Exception as error:
            result['failure'] = str(error)
            raise
        finally:
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
            if game.poll() is None:
                os.killpg(game.pid, signal.SIGTERM)
                try:
                    game.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(game.pid, signal.SIGKILL)
                    game.wait()
    print(f"PASS menus { {k: len(v) for k, v in result['menus'].items()} }; {len(result['scripts'])} script commands; {out}")


if __name__ == '__main__':
    main()
