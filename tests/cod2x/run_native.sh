#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_work=$(mktemp -d "${TMPDIR:-/tmp}/cod2x-native.XXXXXX")
trap 'rm -rf "$test_work"' EXIT HUP INT TERM
compile() {
    clang -std=c11 -Wall -Wextra -Werror -DCOD2_X64=1 -DCOD2_CODX=1 "$@"
}
compile -Isrc/PC/qcommon tests/cod2x/test_url.c src/PC/qcommon/cod2x_url.c -o "$test_work/url"
"$test_work/url"
compile -Isrc/platform tests/cod2x/test_mouse.c src/platform/cod2x_native_mouse.c -o "$test_work/mouse"
"$test_work/mouse"
compile -Wno-unused-function -Wno-unused-but-set-variable -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
    -I"$(sdl2-config --prefix)/include" $(sdl2-config --cflags) -Isrc -Isrc/headers \
    -DSDL_PollEvent=Test_PollEvent -DSDL_GetWindowFlags=Test_GetWindowFlags \
    -DSDL_GetRelativeMouseMode=Test_GetRelativeMouseMode -DSDL_SetRelativeMouseMode=Test_SetRelativeMouseMode \
    -DSDL_SetWindowGrab=Test_SetWindowGrab -DSDL_IsTextInputActive=Test_IsTextInputActive \
    -DSDL_StartTextInput=Test_StartTextInput tests/cod2x/test_mouse_input.c \
    src/unix/linux_input.c src/platform/cod2x_native_mouse.c -o "$test_work/input"
"$test_work/input"
compile -fobjc-arc -Isrc tests/cod2x/test_url_native.m src/platform/cod2x_native_macos.m \
    src/PC/qcommon/cod2x_url.c -framework AppKit -framework Foundation -o "$test_work/url_native"
"$test_work/url_native"
mkdir -p "$test_work/game/main" "$test_work/reports"
index=0
while [ "$index" -lt 16 ]; do
    touch "$test_work/game/main/$(printf 'iw_%02d.iwd' "$index")"
    index=$((index + 1))
done
python3 tools/cod2x/make_macos_app.py "$test_work/url_native" "$test_work/URLProbe.app" --game-dir "$test_work/game"
plutil -lint "$test_work/URLProbe.app/Contents/Info.plist"
expected_game=$(cd "$test_work/game" && pwd -P)
WS10_EXPECT_GAME="$expected_game" "$test_work/URLProbe.app/Contents/MacOS/cod2_macos"
compile -Wno-unused-function -Wno-typedef-redefinition -Wno-duplicate-decl-specifier -Isrc -Isrc/headers \
    tests/cod2x/test_native_freeze.c src/platform/cod2x_native.c src/PC/qcommon/crash_handler.c -o "$test_work/freeze"
"$test_work/freeze" "$test_work/reports" > "$test_work/freeze.log" 2>&1
python3 - "$test_work/freeze" "$test_work/reports" "$test_work/freeze.log" <<'PY'
import pathlib, resource, signal, subprocess, sys
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
log = pathlib.Path(sys.argv[3]).read_text()
for field in ('pc=', 'sp=', 'fp=', 'lr=', 'x0', 'x28', 'backtrace', 'END FREEZE REPORT',
              'watchdog: heartbeat, disable, 12s stall and shutdown passed'):
    assert field in log, field
result = subprocess.run([sys.argv[1], sys.argv[2], '--crash'], capture_output=True, text=True)
assert result.returncode == -signal.SIGABRT, (result.returncode, result.stderr)
for field in ('pc=', 'sp=', 'fp=', 'lr=', 'x0', 'x28', 'backtrace', 'raise'):
    assert field in result.stderr, field
assert list(pathlib.Path(sys.argv[2]).glob('cod2_crash_*.txt'))
print('cod2x native watchdog and crash reports passed')
PY
