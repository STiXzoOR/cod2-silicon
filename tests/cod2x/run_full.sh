#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/cod2x-full.XXXXXX)
trap 'rm -rf "$work"' EXIT HUP INT TERM
flags="-std=gnu11 -g -ffp-contract=off -DCOD2_X64=1 -DCOD2_CODX=1 -Isrc/headers -Isrc -I. -Isrc/PC/qcommon -Isrc/platform -Wno-duplicate-decl-specifier -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-deprecated-non-prototype -Wno-pointer-to-int-cast"
if [ "${COD2X_SANITIZERS:-0}" = 1 ]; then
    flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer"
fi
sh tests/cod2x/run.sh
clang $flags -Wno-deprecated-declarations tests/cod2x/test_cod2x.c tests/cod2x/test_identity_sdk.c \
    src/PC/qcommon/cod2x_identity.c src/PC/qcommon/cod2x_protocol.c \
    src/stubs/iokit_stubs.c src/stubs/carbon_stubs.c -framework IOKit \
    -framework CoreFoundation -framework OpenGL -o "$work/identity-engine-stubs"
"$work/identity-engine-stubs"
for test in policy features mouse url dvar_pool info; do
    case "$test" in
        policy) source=src/PC/qcommon/cod2x_policy.c ;;
        features) source= ;;
        mouse) source=src/platform/cod2x_native_mouse.c ;;
        url) source=src/PC/qcommon/cod2x_url.c ;;
        dvar_pool) source=src/PC/universal/dvar.c ;;
        info) source=src/PC/universal/q_shared.c ;;
    esac
    clang $flags -Wl,-dead_strip "tests/cod2x/test_$test.c" $source -o "$work/$test"
    "$work/$test"
done
clang $flags tests/cod2x/test_features_engine.c src/PC/qcommon/cod2x_features.c -o "$work/features-engine"
"$work/features-engine"
clang $flags -Wl,-dead_strip tests/cod2x/test_radar_engine.c -o "$work/radar-engine"
"$work/radar-engine"
clang $flags tests/cod2x/test_pose.c -o "$work/pose"
"$work/pose"
clang $flags tests/cod2x/test_demo.c src/PC/qcommon/cod2x_demo.c -lcurl -o "$work/demo"
"$work/demo"
clang $flags -Wl,-dead_strip tests/cod2x/test_demo_playback.c src/PC/qcommon/cod2x_demo.c -lcurl -o "$work/playback"
"$work/playback"
clang $flags -I"$(sdl2-config --prefix)/include" $(sdl2-config --cflags) -Wl,-dead_strip \
    -DSDL_GetRelativeMouseMode=Test_GetRelativeMouseMode -DSDL_SetRelativeMouseMode=Test_SetRelativeMouseMode \
    -DSDL_SetWindowGrab=Test_SetWindowGrab -DSDL_GetWindowFlags=Test_GetWindowFlags \
    -DSDL_IsTextInputActive=Test_IsTextInputActive -DSDL_StartTextInput=Test_StartTextInput \
    -DSDL_PollEvent=Test_PollEvent tests/cod2x/test_mouse_input.c src/unix/linux_input.c \
    src/platform/cod2x_native_mouse.c -o "$work/input"
"$work/input"
clang $flags -fobjc-arc tests/cod2x/test_url_native.m src/platform/cod2x_native_macos.m \
    src/PC/qcommon/cod2x_url.c -framework AppKit -framework Foundation -o "$work/native-url"
"$work/native-url"
mkdir -p "$work/game/main"
python3 tools/cod2x/make_macos_app.py "$work/native-url" "$work/URLProbe.app" --game-dir "$work/game"
plutil -lint "$work/URLProbe.app/Contents/Info.plist"
expected_game=$(cd "$work/game" && pwd -P)
WS10_EXPECT_GAME="$expected_game" "$work/URLProbe.app/Contents/MacOS/cod2_macos"
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/cod2x -p test_extract_iwd.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/cod2x/test_demo_https.py
if [ "${COD2X_SANITIZERS:-0}" != 1 ]; then
    clang $flags -Wl,-dead_strip tests/cod2x/test_native_freeze.c src/platform/cod2x_native.c \
        src/PC/qcommon/crash_handler.c -o "$work/freeze"
    "$work/freeze" "$work" > "$work/freeze.log" 2>&1
    python3 - "$work" <<'PY'
from pathlib import Path
import resource
import stat
import subprocess
import sys
directory = Path(sys.argv[1])
freeze = list(directory.glob("cod2_freeze_*.txt"))
assert len(freeze) == 1
assert "pc=" in freeze[0].read_text() and "x28=" in freeze[0].read_text()
assert stat.S_IMODE(freeze[0].stat().st_mode) == 0o600
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
crash = subprocess.run([str(directory / "freeze"), str(directory), "--crash"], capture_output=True)
assert crash.returncode != 0
reports = list(directory.glob("cod2_crash_*.txt"))
assert len(reports) == 1 and "pc=" in reports[0].read_text() and "x28=" in reports[0].read_text()
assert stat.S_IMODE(reports[0].stat().st_mode) == 0o600
print("CoD2x watchdog heartbeat/disable/stall and native crash report: passed")
PY
fi
