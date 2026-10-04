#!/usr/bin/env bash
# Run a short, owned Spaces-fullscreen app session and inspect Game Policy.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
app=${1:-"$repo/output/ws19/CoD2x WS19.app"}
out=${2:-"$repo/output/gamemode-$(date -u +%Y%m%dT%H%M%SZ)-$$"}
data=${COD2_DATA_DIR:-"$HOME/Games/CoD2"}
[[ -d $app && $app == *.app ]] || { echo "Missing test app: $app" >&2; exit 2; }
app=$(cd "$app" && pwd -P)
command -v timeout >/dev/null || { echo 'timeout is required; no packages were installed' >&2; exit 2; }
lsregister=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
# Only unregister the test bundle passed to this command. Do not use the installed app.
case "$app" in "$HOME/Applications/CoD2x Native.app") echo 'Use a worktree test bundle' >&2; exit 2;; esac
cleanup() {
    timeout -k 10 15 python3 "$repo/tools/macos/stop-owned-client.py" "$out" || true
    "$lsregister" -u "$app" >/dev/null 2>&1 || true
}
trap cleanup EXIT
started=$(date '+%Y-%m-%d %H:%M:%S')
SDL_VIDEO_MAC_FULLSCREEN_SPACES=1 timeout -k 10 55 python3 "$repo/tools/macos/live-bench.py" \
    "$data" --app "$app" --output "$out" --window-mode fullscreen \
    --resolution 1920x1080 --maxfps 333 --seconds 10 --combat
/usr/bin/log show --start "$started" --style compact \
    --predicate 'process == "gamepolicyd" OR process == "GamePolicyAgent"' > "$out/game-policy.log"
python3 - "$out" <<'PY'
import json, re, sys
from pathlib import Path
out = Path(sys.argv[1])
pid = int((out / 'observer.pid').read_text())
log = (out / 'game-policy.log').read_text()
owned = [line for line in log.splitlines() if f'pid={pid},' in line or f':{pid}]' in line]
identified = any('Found game' in line for line in owned)
states = re.findall(r'Game mode status is now ([^.]+)\.', log)
result = json.loads((out / 'results.json').read_text())
print(f'Owned game PID: {pid}; recognized by Game Policy: {identified}')
print('Screen locked:', result['machine_state']['before']['screen_locked'])
print('Actual window:', result['actual_presentation'])
print('Session Game Mode states:', ', '.join(states) if states else 'no state logged')
print('These states describe the system session; another game can affect them.')
print('Shutdown:', result['shutdown'])
for line in owned:
    if 'Found game' in line:
        print(line)
if result['machine_state']['before']['screen_locked']:
    print('Locked-screen evidence is not Game Mode acceptance. Repeat unlocked.')
if result['shutdown'] != 'quit':
    raise SystemExit('Game did not quit cleanly; inspect console.log')
PY
