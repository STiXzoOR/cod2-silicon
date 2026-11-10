#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_work=$(mktemp -d "${TMPDIR:-/tmp}/cod2x-native.XXXXXX")
trap '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -u "$test_work/URLProbe.app"; rm -rf "$test_work"' EXIT HUP INT TERM
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
    src/platform/cod2x_native_shaders.m src/PC/qcommon/cod2x_url.c \
    -framework AppKit -framework Foundation -o "$test_work/url_native"
"$test_work/url_native"
mkdir -p "$test_work/game/main" "$test_work/reports"
index=0
while [ "$index" -lt 16 ]; do
    touch "$test_work/game/main/$(printf 'iw_%02d.iwd' "$index")"
    index=$((index + 1))
done
python3 tools/cod2x/make_macos_app.py "$test_work/url_native" "$test_work/URLProbe.app" --engine-only --game-dir "$test_work/game"
plutil -lint "$test_work/URLProbe.app/Contents/Info.plist"
expected_game=$(cd "$test_work/game" && pwd -P)
mkdir -p "$test_work/home"
CFFIXED_USER_HOME="$test_work/home" COD2_SETUP_NONINTERACTIVE=1 COD2_SETUP_CD_KEY=000000000000000086D3 \
    WS10_EXPECT_GAME="$expected_game" "$test_work/URLProbe.app/Contents/MacOS/cod2_macos"
codesign --verify --deep --strict "$test_work/URLProbe.app"
compile -Wno-unused-function -Wno-typedef-redefinition -Wno-duplicate-decl-specifier -Isrc -Isrc/headers \
    tests/cod2x/test_native_freeze.c src/platform/cod2x_native.c src/PC/qcommon/crash_handler.c \
    src/PC/qcommon/cod2x_layer_start.c src/PC/qcommon/cod2x_protocol.c src/PC/qcommon/cod2x_report.c \
    src/PC/qcommon/cod2x_layer_stop.c -lcurl -o "$test_work/freeze"
"$test_work/freeze" "$test_work/reports" > "$test_work/freeze.log" 2>&1
python3 - "$test_work/freeze" "$test_work/reports" "$test_work/freeze.log" <<'PY'
import pathlib, resource, signal, subprocess, sys
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
log = pathlib.Path(sys.argv[3]).read_text()
for field in ('=== COD2X FREEZE ===', 'PC=0x', 'SP=0x', 'FP=0x', 'LR=0x', 'X0 =0x', 'X28=0x',
              '=== STACK (backtrace) ===', 'Freeze report saved to',
              'watchdog: heartbeat, disable, 12s stall and shutdown passed'):
    assert field in log, field
result = subprocess.run([sys.argv[1], sys.argv[2], '--crash'], capture_output=True, text=True)
assert result.returncode == -signal.SIGABRT, (result.returncode, result.stderr)
for field in ('=== COD2X CRASH ===', 'Signal:       6 SIGABRT (abort)', 'PC=0x', 'SP=0x', 'FP=0x', 'LR=0x',
              'X0 =0x', 'X28=0x', '=== STACK (backtrace) ===', 'raise', 'Crash report saved to'):
    assert field in result.stderr, field
crash = list(pathlib.Path(sys.argv[2]).glob('cod2x_crash.*.log'))
assert crash
# Faults outside CoD2x-layer code go to the receiver, never into a CoD2x report.
received = (pathlib.Path(sys.argv[2]) / 'receiver.txt').read_text()
assert crash[0].name + '|SIGABRT' in received, received
assert not list(pathlib.Path(sys.argv[2]).glob('*.pending.json'))
print('cod2x native watchdog and crash reports passed')
PY
# A build that asks first: with COD2X_REPORT_CONSENT_ENV defined, nothing reaches CoD2x's
# services until the variable is "1", but a CoD2x-layer fault still leaves its pending report.
mkdir -p "$test_work/consent"
compile -Wno-unused-function -Wno-typedef-redefinition -Wno-duplicate-decl-specifier -Isrc -Isrc/headers \
    '-DCOD2X_REPORT_CONSENT_ENV="COD2X_TEST_REPORT_CONSENT"' \
    tests/cod2x/test_native_freeze.c src/platform/cod2x_native.c src/PC/qcommon/crash_handler.c \
    src/PC/qcommon/cod2x_layer_start.c src/PC/qcommon/cod2x_protocol.c src/PC/qcommon/cod2x_report.c \
    src/PC/qcommon/cod2x_layer_stop.c -lcurl -o "$test_work/consent-probe"
python3 - "$test_work/consent-probe" "$test_work/consent" <<'PY'
import http.server, os, pathlib, resource, subprocess, sys, threading
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
probe, folder = sys.argv[1], pathlib.Path(sys.argv[2])
requests = []
class StandIn(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length') or 0))
        requests.append(self.path)
        self.send_response(200)
        self.send_header('Content-Length', '2')
        self.end_headers()
        self.wfile.write(b'{}')
    def log_message(self, *args):
        pass
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), StandIn)
threading.Thread(target=server.serve_forever, daemon=True).start()
base = dict(os.environ, COD2X_TEST_SERVICE_URL='http://127.0.0.1:%d' % server.server_address[1])
base.pop('COD2X_TEST_REPORT_CONSENT', None)
def run(mode, consent):
    env = dict(base, COD2X_TEST_REPORT_CONSENT='1') if consent else base
    return subprocess.run([probe, str(folder), mode], env=env, capture_output=True, text=True, timeout=60)
assert run('--error', False).returncode == 0 and requests == [], requests
crash = run('--layer-crash', False)
assert crash.returncode < 0 and 'Crash report saved to' in crash.stderr, (crash.returncode, crash.stderr[-400:])
assert len(list(folder.glob('cod2x_crash.*.log.pending.json'))) == 1 and requests == [], requests
assert not (folder / 'receiver.txt').exists()
assert run('--error', True).returncode == 0 and requests == ['/api/telemetry/cod2x-errors'], requests
server.shutdown()
print('cod2x report consent: nothing sent without it, pending kept, sent with it')
PY
