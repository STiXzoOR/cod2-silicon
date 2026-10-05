#!/usr/bin/env python3
"""Real app/Apple-event/child-crash test, with synthetic game content only."""
import os, pathlib, re, subprocess, time
root = pathlib.Path(__file__).resolve().parents[2]
out = root/'output/ws25/lifecycle';out.mkdir(parents=True, exist_ok=True)
fixture = out/'fixture'
subprocess.run(['taskpolicy','-b','nice','-n','19','clang','-fobjc-arc','-mmacosx-version-min=13.0','-DCOD2_X64=1','-DCOD2_CODX=1',str(root/'tests/launcher/lifecycle.m'),str(root/'src/PC/qcommon/cod2x_url.c'),'-framework','AppKit','-o',str(fixture)],check=True)
app = out/'Lifecycle.app'
subprocess.run(['python3',str(root/'tools/cod2x/make_macos_app.py'),str(fixture),str(app),'--replace','--bundle-id','io.github.stixzoor.cod2silicon.ws25fixture','--launcher',str(root/'output/ws25/launcher/CoD2Launcher')],check=True)
home = out/'home';home.mkdir(exist_ok=True)
game = home/'Games/CoD2/main';game.mkdir(parents=True,exist_ok=True)
for i in range(16):(game/f'iw_{i:02}.iwd').touch()
apphome = home/'Library/Application Support/CoD2 Silicon'
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
        print('PASS: launcher→nested child, Dock handoff, two live cod2x Apple events, no duplicate engine, launcher force-quit/reconnect, game crash→live launcher/Dock restoration/report')
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
