#!/usr/bin/env bash
# Full serial acceptance matrix. No installed bundle or user processes are changed.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo"
out=${COD2_VALIDATE_OUT:-"$repo/output/validate-333-$(date -u +%Y%m%dT%H%M%SZ)-$$"}
if [[ -z ${COD2_BINARY:-} ]]; then
    cmake -S . -B build-macos-codx -DCOD2_X64=ON \
        -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build-macos-codx --target cod2_macos -j12
fi
binary=${COD2_BINARY:-"$repo/build-macos-codx/cod2_macos"}
data=${COD2_DATA_DIR:-"$HOME/Games/CoD2"}
demo_data=${COD2_DEMO_DATA:-"$HOME/Library/Application Support/CoD2-native-ws18/bench-data"}
[[ -x $binary ]] || { echo "Missing Release CoD2x executable: $binary" >&2; exit 2; }
command -v timeout >/dev/null || { echo 'timeout is required; no packages were installed' >&2; exit 2; }
mkdir "$out"
if [[ ! -f "$demo_data/main/demos/ws18_bench.dm_1" ]]; then
    archived="$HOME/Projects/cod2-native-refs/evidence/ship/ws18/bench/live-1920x1080-333/home/main/demos/ws18_bench.dm_1"
    [[ -f $archived ]] || { echo "Missing WS18 demo: $archived" >&2; exit 2; }
    # The old alias points at the removed ship worktree. Leave it untouched.
    demo_data="$out/bench-data"
    python3 - "$data" "$demo_data" "$archived" <<'PY_ALIAS'
import sys
from pathlib import Path
source, alias, demo = map(Path, sys.argv[1:])
(alias / 'main/demos').mkdir(parents=True)
for iwd in (source / 'main').glob('*.iwd'):
    (alias / 'main' / iwd.name).symlink_to(iwd.resolve())
(alias / 'main/demos/ws18_bench.dm_1').symlink_to(demo.resolve())
PY_ALIAS
fi
app="$out/CoD2x WS19 Validation.app"
lsregister=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
live_out=""
cleanup() {
    if [[ -n $live_out ]]; then
        timeout -k 10 15 python3 "$repo/tools/macos/stop-owned-client.py" "$live_out" || true
    fi
    "$lsregister" -u "$app" >/dev/null 2>&1 || true
}
trap cleanup EXIT
python3 tools/cod2x/make_macos_app.py "$binary" "$app" \
    --bundle-id "org.opencod2.ws19.validation.$$" --game-dir "$data"
export COD2_MAC_SHADER_CACHE=${COD2_MAC_SHADER_CACHE:-"$HOME/Library/Application Support/CoD2x Native/shaders"}
export COD2_BINARY="$binary"
# Both modes allow a direct comparison. Spaces preserves the desktop mode.
for spaces in ${COD2_SPACES_MODES:-0}; do
    export SDL_VIDEO_MAC_FULLSCREEN_SPACES=$spaces
    for present in ${COD2_PRESENT_MODES:-1}; do
        for resolution in 1920x1080 2560x1440 3840x2160; do
            for cap in 333 0; do
                for repeat in 1 2 3; do
                    name="s${spaces}-p${present}-${resolution}-${cap}-${repeat}"
                    echo "Measuring $name (120s live combat plus timedemo)"
                    live_out="$out/live-$name"
                    timeout -k 10 210 python3 tools/macos/live-bench.py "$data" --app "$app" \
                        --output "$out/live-$name" --window-mode fullscreen --resolution "$resolution" \
                        --maxfps "$cap" --seconds 120 --combat --set r_presentMode "$present" > "$out/live-$name.log" 2>&1
                    timeout -k 10 190 python3 tools/macos/benchmark.py "$demo_data" ws18_bench \
                        --output "$out/demo-$name" --window-mode fullscreen --resolution "$resolution" \
                        --maxfps "$cap" --set r_presentMode "$present" > "$out/demo-$name.log" 2>&1
                done
            done
        done
    done
done
tools/macos/check-gamemode.sh "$app" "$out/gamemode"
python3 tools/macos/summarize-333.py "$out" ${COD2_ALLOW_LOCKED:+--allow-locked}
echo "Evidence: $out/summary.json and $out/gamemode/game-policy.log"
