#!/usr/bin/env python3
"""Real app/Apple-event/child-crash test, with synthetic game content only."""
import base64, json, os, pathlib, re, subprocess, time
root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(os.environ.get('COD2_LAUNCHER_TEST_OUTPUT', str(root/'output/ws25'))) / 'lifecycle.noindex';out.mkdir(parents=True, exist_ok=True)
fixture = out/'fixture'
for variant, flags in [(fixture, []), (out/'fixture-stock', ['-DLIFECYCLE_STOCK'])]:
    subprocess.run(['taskpolicy','-b','nice','-n','19','clang','-fobjc-arc','-mmacosx-version-min=26.0','-DCOD2_X64=1','-DCOD2_CODX=1',*flags,str(root/'tests/launcher/lifecycle.m'),str(root/'src/PC/qcommon/cod2x_url.c'),'-framework','AppKit','-o',str(variant)],check=True)
app = out/'Lifecycle.app'
subprocess.run([str(root/'scripts/build-launcher.sh'), str(out/'launcher')], check=True)
subprocess.run(['python3',str(root/'tools/cod2x/make_macos_app.py'),str(fixture),str(app),'--replace','--bundle-id','io.github.stixzoor.cod2silicon.ws25fixture','--launcher',str(out/'launcher/CoD2Launcher'),'--stock-engine',str(out/'fixture-stock')],check=True)
home = out/'home';home.mkdir(exist_ok=True)
game = home/'Games/CoD2/main';game.mkdir(parents=True,exist_ok=True)
for i in range(16):(game/f'iw_{i:02}.iwd').touch()
apphome = home/'Library/Application Support/CoD2 Silicon'
apphome.mkdir(parents=True, exist_ok=True)
(apphome/'main').mkdir(exist_ok=True)
(apphome/'main/config_mp.cfg').write_bytes(b'seta sensitivity "2"\r\n')
(apphome/'launcher-settings.json').write_text(json.dumps({'advanced': 'net_port 29972'}))
# Settings › Game › CoD2x files: a kept (synthetic) archive goes into the app home's main for CoD2x only.
kept = b'synthetic CoD2x archive'
(apphome/'cod2x').mkdir(exist_ok=True); (apphome/'cod2x/iw_CoD2x_01.iwd').write_bytes(kept)
placed = apphome/'main/iw_CoD2x_01.iwd'
env = {**os.environ,'CFFIXED_USER_HOME':str(home),'HOME':str(home),'COD2_SETUP_NONINTERACTIVE':'1','COD2_SETUP_CD_KEY':'000000000000000086D3','WS25_FIXTURE_HOME':str(apphome)}
exitfile = apphome/'exit-fixture'
exitfile.unlink(missing_ok=True)
log = out/'console.log'
with log.open('w') as stream:
    launcher = subprocess.Popen([str(app/'Contents/MacOS/CoD2Launcher'),'cod2x://connect%20localhost%3A28963%20password%20%22p%2Bquit%20%22'],stdout=stream,stderr=stream,env=env,cwd=out)
    child = None
    def wait(predicate):
        deadline=time.monotonic()+25
        while not predicate():
            assert launcher.poll() is None, f"launcher exit {launcher.returncode}: " + log.read_text(errors='replace')
            assert time.monotonic()<deadline, log.read_text(errors='replace')
            time.sleep(.05)
    try:
        wait(lambda: 'fixture ready' in log.read_text(errors='replace'))
        wait(lambda: 'fixture link localhost:28963' in log.read_text(errors='replace'))
        child=int(re.search(r'fixture ready (\d+)',log.read_text()).group(1))
        assert placed.read_bytes() == kept and not (game/'iw_CoD2x_01.iwd').exists(), 'CoD2x starts with the kept archive in the app home only'
        wait(lambda: (apphome/'fixture-arguments.json').is_file())
        arguments = json.loads((apphome/'fixture-arguments.json').read_text())
        assert 'sensitivity' not in arguments and 'com_maxfps' not in arguments
        assert 'r_mode' in arguments and 'net_port' in arguments
        active = (apphome/'main/players/active.txt').read_text().strip()
        assert active == 'Main'
        assert 'seta sensitivity "5.0"' in (apphome/f'main/players/{active}/config_mp.cfg').read_text()
        assert (apphome/'main/players/.studio-game-session.json').is_file()
        def policy(pid):return subprocess.check_output([str(fixture),'policy',str(pid)],text=True).strip()
        assert policy(launcher.pid)=='1' # accessory: the launcher has no Dock tile
        assert policy(child)=='0' # regular: the game owns the Dock
        for address in ['127.0.0.1:28960','localhost:28961']:
            subprocess.run([str(fixture),'event',str(launcher.pid),'cod2x://connect/'+address],check=True)
            wait(lambda: 'fixture link '+address in log.read_text(errors='replace'))
        assert log.read_text().count('fixture ready')==1
        # A launcher crash must not cause a duplicate game on relaunch.
        launcher.kill(); launcher.wait(timeout=5)
        launcher = subprocess.Popen([str(app/"Contents/MacOS/CoD2Launcher"), "--play"], stdout=stream, stderr=stream, env=env, cwd=out)
        wait(lambda: "reconnected to existing game" in log.read_text(errors="replace"))
        subprocess.run([str(fixture), "event", str(launcher.pid), "cod2x://connect/localhost:28962"], check=True)
        wait(lambda: "fixture link localhost:28962" in log.read_text(errors="replace"))
        assert log.read_text().count("fixture ready") == 1
        assert policy(launcher.pid) == "1"
        exitfile.touch()
        wait(lambda:'returned to launcher (game exit unavailable)' in log.read_text(errors='replace'))
        assert launcher.poll() is None and policy(launcher.pid)=='0'
        assert (apphome/'cod2_crash_fixture.txt').is_file()
        snapshots = [json.loads(path.read_text()) for path in (apphome/f'main/players/{active}/history').glob('*.json')]
        sessions = [entry for entry in snapshots if entry['source'] == 'gameSession']
        assert len(sessions) == 1
        assert b'seta sensitivity "5.0"' in base64.b64decode(sessions[0]['beforeData'])
        assert b'seta sensitivity "6"' in base64.b64decode(sessions[0]['data'])
        assert not (apphome/'main/players/.studio-game-session.json').exists()
        # Settings › Game version: Original 1.3 starts its own engine, which takes links like CoD2x's,
        # passworded ones included, never +connect or a password in argv, and is found again after a
        # launcher force-quit. The unknown server is asked for its version first (no answer).
        launcher.terminate(); launcher.wait(timeout=5)
        settings = json.loads((apphome/'launcher-settings.json').read_text()); settings['engine'] = 'original'
        (apphome/'launcher-settings.json').write_text(json.dumps(settings))
        (apphome/'fixture-arguments.json').unlink(); exitfile.unlink()
        loadout = apphome/f'main/players/{active}/config_mp.cfg'
        loadout.write_text(loadout.read_text() + 'seta cl_maxpackets "125"\n')
        launcher = subprocess.Popen([str(app/'Contents/MacOS/CoD2Launcher'), 'cod2x://connect%20localhost%3A28964%20password%20%22s3cret%2Bx%22'], stdout=stream, stderr=stream, env=env, cwd=out)
        wait(lambda: log.read_text(errors='replace').count('fixture ready') == 2 and (apphome/'fixture-arguments.json').is_file())
        wait(lambda: 'fixture link localhost:28964 password s3cret+x' in log.read_text(errors='replace'))
        stock = int(re.findall(r'fixture ready (\d+)', log.read_text())[-1])
        assert not placed.exists() and (apphome/'cod2x/iw_CoD2x_01.iwd').read_bytes() == kept, 'Original 1.3 starts without the CoD2x archive'
        assert subprocess.check_output(['ps', '-o', 'comm=', '-p', str(stock)], text=True).strip().endswith('/MacOS/cod2_macos_stock')
        arguments = json.loads((apphome/'fixture-arguments.json').read_text())
        assert '+connect' not in arguments and not any('s3cret' in a or 'password' in a.lower() for a in arguments), arguments
        launcher.kill(); launcher.wait(timeout=5)
        launcher = subprocess.Popen([str(app/'Contents/MacOS/CoD2Launcher'), '--play'], stdout=stream, stderr=stream, env=env, cwd=out)
        wait(lambda: log.read_text(errors='replace').count('reconnected to existing game') == 2)
        subprocess.run([str(fixture), 'event', str(launcher.pid), 'cod2x://connect/localhost:28965'], check=True)
        wait(lambda: 'fixture link localhost:28965' in log.read_text(errors='replace'))
        assert log.read_text(errors='replace').count('fixture ready') == 2
        exitfile.touch()
        wait(lambda: log.read_text(errors='replace').count('returned to launcher') == 2)
        assert launcher.poll() is None
        assert 'seta cl_maxpackets "30"' in loadout.read_text() and 'seta cl_maxpackets "125"' not in loadout.read_text()
        print('PASS: Original 1.3 starts cod2_macos_stock without the CoD2x archive, takes a passworded cold link and a live link as Apple events (never in argv) and is reattached after a force-quit')
        # Back to CoD2x: the 125 the original couldn't keep is in the loadout before the CoD2x engine starts,
        # and the original's own change (sensitivity 6) stays.
        launcher.terminate(); launcher.wait(timeout=5)
        settings['engine'] = 'cod2x'; (apphome/'launcher-settings.json').write_text(json.dumps(settings))
        exitfile.unlink()
        launcher = subprocess.Popen([str(app/'Contents/MacOS/CoD2Launcher'), '--play'], stdout=stream, stderr=stream, env=env, cwd=out)
        wait(lambda: log.read_text(errors='replace').count('fixture ready') == 3)
        cod2x = int(re.findall(r'fixture ready (\d+)', log.read_text())[-1])
        assert placed.read_bytes() == kept and sorted(p.name for p in game.iterdir()) == [f'iw_{i:02}.iwd' for i in range(16)]
        assert subprocess.check_output(['ps', '-o', 'comm=', '-p', str(cod2x)], text=True).strip().endswith('/MacOS/cod2_macos')
        text = loadout.read_text()
        assert 'seta cl_maxpackets "125"' in text and 'seta cl_maxpackets "30"' not in text and text.rstrip().endswith('seta sensitivity "6"'), text
        summaries = [json.loads(path.read_text())['summary'] for path in (apphome/f'main/players/{active}/history').glob('*.json')]
        assert summaries.count('Put back CoD2x values that Original 1.3 can’t keep') == 1, summaries
        print('PASS: the CoD2x value Original 1.3 wrote over (cl_maxpackets 125 to 30) is back before the next CoD2x game, the original\'s own change kept')
        print('PASS: launcher→nested child, Dock handoff, two live cod2x Apple events, no duplicate engine, launcher force-quit/reconnect, game crash→live launcher/Dock restoration/report')
        print('PASS: real launch migration and machine argv, personal config ownership, persisted before-session archive and exactly one snapshot after reattachment/exit')
    finally:
        launcher.terminate()
        try:launcher.wait(timeout=5)
        except subprocess.TimeoutExpired:launcher.kill();launcher.wait()
        if child is None:
            match = re.search(r'fixture ready (\d+)', log.read_text(errors='replace'))
            if match: child = int(match.group(1))
        for owned in {int(pid) for pid in re.findall(r"fixture ready (\d+)", log.read_text(errors="replace"))}:
            try:os.kill(owned,15)
            except ProcessLookupError:pass
        unregister='/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'
        for bundle in [app,app/'Contents/Helpers/CoD2 Game.app']:
            subprocess.run([unregister,'-u',str(bundle)],check=False)
