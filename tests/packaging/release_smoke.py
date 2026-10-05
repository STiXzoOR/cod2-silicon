#!/usr/bin/env python3
"""Short owned-process menu/map/quit test with an isolated first-run home."""
import argparse
import json
import os
import pathlib
import plistlib
import signal
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("app", type=pathlib.Path)
parser.add_argument("--game", type=pathlib.Path, required=True)
parser.add_argument("--mac-binary", type=pathlib.Path, required=True)
parser.add_argument("--output", type=pathlib.Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
home = out / "home"
home.mkdir()
# The user specifically requested this inventory before each game launch.
while True:
    inventory = subprocess.run(["pgrep", "-x", "cod2_macos"], capture_output=True, text=True)
    safe_inventory = []
    running = []
    for line in inventory.stdout.splitlines():
        pid = line.split()[0]
        comm = subprocess.run(["ps", "-p", pid, "-o", "comm="], capture_output=True, text=True).stdout.strip()
        safe_inventory.append(f"{pid} {comm}")
        if pathlib.Path(comm).name == "cod2_macos":
            running.append(pid)
    (out / "process-inventory.txt").write_text("\n".join(safe_inventory) + "\n")
    if not running:
        break
    print("Another agent's game is running; polling again in 60 seconds.", flush=True)
    time.sleep(60)

environment = {**os.environ, "CFFIXED_USER_HOME": str(home), "HOME": str(home),
               "COD2_SETUP_NONINTERACTIVE": "1", "COD2_SETUP_GAME_DIR": str(args.game.resolve()),
               "COD2_SETUP_MAC_BINARY": str(args.mac_binary.resolve()),
               "COD2_SETUP_CD_KEY": '000000000000000086D3', "DYLD_PRINT_LIBRARIES": "1"}
environment.pop("COD2_MAC_SHADER_CACHE", None)
info = plistlib.loads((args.app.resolve() / "Contents/Info.plist").read_bytes())
launcher = info["CFBundleExecutable"] == "CoD2Launcher"
command = [str(args.app.resolve() / "Contents/MacOS" / info["CFBundleExecutable"])]
if launcher:
    command += ["--play", "--exit-after-game", "--"]
command += [
           "+set", "r_fullscreen", "0", "+set", "r_mode", "1280x720", "+set", "developer", "1",
           "+set", "sv_pure", "0", "+set", "net_port", "29021", "+set", "g_gametype", "dm"]
log = out / "console.log"
with log.open("w") as stream:
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=stream, stderr=stream,
                               text=True, env=environment, cwd=out, start_new_session=True)
    def send(text):
        process.stdin.write(text + "\n")
        process.stdin.flush()

    def wait_for(predicate, limit=45):
        deadline = time.monotonic() + limit
        while not predicate():
            if process.poll() is not None:
                raise RuntimeError(f"client exited {process.returncode}")
            if time.monotonic() > deadline:
                raise RuntimeError("client did not reach expected state")
            time.sleep(.1)

    try:
        wait_for(lambda: "Loading 'ui_mp/main.menu'" in log.read_text(errors="replace") and
                 "--- Common Initialization Complete ---" in log.read_text(errors="replace"))
        send("screenshotJPEG ws21-menu")
        time.sleep(1)
        send("devmap mp_toujane")
        wait_for(lambda: "Going from CS_PRIMED to CS_ACTIVE" in log.read_text(errors="replace"))
        send("screenshotJPEG ws21-toujane")
        screenshots = home / "Library/Application Support/CoD2 Silicon/main/screenshots"
        wait_for(lambda: len(list(screenshots.glob("*.jpg"))) >= 2, limit=10)
        send("quit")
        process.wait(timeout=20)
        assert process.returncode == 0
        if launcher:
            assert "returned to launcher (game exit 0)" in log.read_text(errors="replace")
        text = log.read_text(errors="replace")
        framework_lines = [line for line in text.splitlines() if "dyld" in line and "libSDL" in line]
        assert any("Contents/Frameworks/libSDL2" in line for line in framework_lines), framework_lines
        assert any("Contents/Frameworks/libSDL3" in line for line in framework_lines), framework_lines
        assert not any("/opt/homebrew" in line for line in framework_lines), framework_lines
        assert (home / ".cod2/preferences").stat().st_mode & 0o777 == 0o600
        results = {"exit_code": process.returncode, "menu": True, "devmap": "mp_toujane",
                   "screenshots": [str(p) for p in screenshots.glob("*.jpg")], "loaded_sdl": framework_lines, "launcher_returned": launcher and "returned to launcher (game exit 0)" in text}
        (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        print("PASS: empty-home menu, devmap mp_toujane, two JPEGs, scripted quit exit 0, launcher return, bundled SDL2/SDL3")
    finally:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        if process.poll() is None:
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        for bundle in [args.app.resolve(), args.app.resolve() / "Contents/Helpers/CoD2 Game.app"]:
            if bundle.exists():
                subprocess.run(["/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister",
                                "-u", str(bundle)], check=False)
