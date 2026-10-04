#!/usr/bin/env python3
"""Exercise actual LaunchServices URL launch in a new scratch home."""
import os, pathlib, re, subprocess, time
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'output/ws25/lifecycle';app=out/'Lifecycle.app';fixture=out/'fixture'
home=out/'cold-home';home.mkdir(exist_ok=True)
game=home/'Games/CoD2/main';game.mkdir(parents=True,exist_ok=True)
for i in range(16):(game/f'iw_{i:02}.iwd').touch()
apphome=home/'Library/Application Support/CoD2 Silicon'
exitfile=apphome/'exit-fixture';exitfile.unlink(missing_ok=True)
log=out/'cold-console.log';log.write_text('')
environment={'CFFIXED_USER_HOME':str(home),'HOME':str(home),'COD2_SETUP_NONINTERACTIVE':'1','COD2_SETUP_CD_KEY':'000000000000000086D3','WS25_FIXTURE_HOME':str(apphome)}
command=['open','-n','-a',str(app),'--stdout',str(log),'--stderr',str(log)]
for key,value in environment.items():command += ['--env',key+'='+value]
command += ['cod2x://connect%20localhost%3A28966%20password%20%22p%2Bquit%20%22']
launcher=None;child=None
try:
    subprocess.run(command,check=True)
    def wait(predicate):
        deadline=time.monotonic()+30
        while not predicate():
            assert time.monotonic()<deadline,log.read_text(errors='replace')
            time.sleep(.05)
    wait(lambda:'fixture link localhost:28966' in log.read_text(errors='replace'))
    child=int(re.search(r'fixture ready (\d+)',log.read_text()).group(1))
    launcher=int(subprocess.check_output([str(fixture),'find','io.github.stixzoor.cod2silicon.ws25fixture'],text=True).strip())
    assert subprocess.check_output([str(fixture),'policy',str(launcher)],text=True).strip()=='1'
    arguments=subprocess.check_output(['ps','-p',str(child),'-o','args='],text=True)
    assert 'p+quit' not in arguments and ' password ' not in arguments
    exitfile.touch()
    wait(lambda:'returned to launcher (game exit 37)' in log.read_text(errors='replace'))
    assert subprocess.check_output([str(fixture),'policy',str(launcher)],text=True).strip()=='0'
    print('PASS: actual cold LaunchServices cod2x delivery, scratch onboarding, native-valid plus password deferred without argv exposure, game→launcher')
finally:
    for pid in [launcher,child]:
        if pid:
            try:os.kill(pid,15)
            except ProcessLookupError:pass
    for bundle in [app,app/'Contents/Helpers/CoD2 Game.app']:
        subprocess.run(['/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister','-u',str(bundle)],check=False)
