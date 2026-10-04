#!/usr/bin/env python3
"""Exercise real Cocoa setup with empty homes; never access a real CD key."""
import os
import pathlib
import plistlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="cod2-silicon-first-run-") as directory:
    scratch = pathlib.Path(directory)
    executable = scratch / "first-run"
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "clang", "-fobjc-arc",
                    "-mmacosx-version-min=13.0", "-DCOD2_X64=1", "-DCOD2_CODX=1", "-Isrc",
                    "tests/packaging/first_run.m", "src/platform/cod2x_native_shaders.m",
                    "-framework", "AppKit", "-o", str(executable)], cwd=ROOT, check=True)
    app = scratch / "CoD2 Silicon.app"
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "python3",
                    "tools/cod2x/make_macos_app.py", str(executable), str(app), "--engine-only"], cwd=ROOT, check=True)
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    assert "CoD2GameDirectory" not in info
    assert info["CFBundleIdentifier"] == "io.github.stixzoor.cod2silicon"
    assert info["CFBundleShortVersionString"] == "0.1.0"
    assert not (app / "Contents/Resources/tools").exists()
    home = scratch / "home"
    game = home / "Games/CoD2"
    (game / "main").mkdir(parents=True)
    for i in range(16):
        (game / f"main/iw_{i:02}.iwd").touch()
    old = home / "Library/Application Support/CoD2x Native"
    (old / "main").mkdir(parents=True)
    (old / "main/config_mp.cfg").write_text('// fake migration fixture\n')
    environment = {**os.environ, "CFFIXED_USER_HOME": str(home), "HOME": str(home),
                   "COD2_SETUP_NONINTERACTIVE": "1", "COD2_SETUP_CD_KEY": '000000000000000086D3'}
    command = [str(app / "Contents/MacOS/cod2_macos")]
    subprocess.run(command, env=environment, check=True)
    new = home / "Library/Application Support/CoD2 Silicon"
    assert (new / "main/config_mp.cfg").read_bytes() == (old / "main/config_mp.cfg").read_bytes()
    assert (new / "data-path.txt").read_text() == str(game)
    preferences = home / ".cod2/preferences"
    assert preferences.stat().st_mode & 0o777 == 0o600
    assert preferences.read_text() == "codkey=" + '000000000000000086D3' + "\n"
    del environment["COD2_SETUP_CD_KEY"]
    preferences.write_text("codkey=" + " " * 16 + "0000\n")
    assert subprocess.run(command, env=environment).returncode == 1
    environment["COD2_SETUP_CD_KEY"] = '000000000000000086D3'
    subprocess.run(command, env=environment, check=True)
    del environment["COD2_SETUP_CD_KEY"]
    moved = scratch / "remembered-game"
    game.rename(moved)
    (new / "data-path.txt").write_text(str(moved))
    subprocess.run(command, env=environment, check=True)
    (new / "main/config_mp.cfg").write_text('// changed after migration\n')
    (new / "data-path.txt").unlink()
    steam = home / "Library/Application Support/Steam/steamapps"
    steam.mkdir(parents=True)
    library = scratch / "Steam Library"
    steam_game = library / "steamapps/common/Call of Duty 2"
    steam_game.parent.mkdir(parents=True)
    moved.rename(steam_game)
    (steam / "libraryfolders.vdf").write_text(f'"libraryfolders" {{ "1" {{ "path" "{library}" }} }}')
    subprocess.run(command, env=environment, check=True)
    assert (new / "data-path.txt").read_text() == str(steam_game)
    assert (new / "main/config_mp.cfg").read_text() == '// changed after migration\n'
    environment["COD2_SETUP_GAME_DIR"] = str(scratch / "absent")
    assert subprocess.run(command, env=environment).returncode == 1
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    subprocess.run(["/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister",
                    "-u", str(app)], check=True)
    print("PASS: identity, no baked path/runtime Python, empty home, private fake key, auto/remembered/Steam data, migration once, invalid data")
