#!/bin/sh
# Build the network-parser fuzz targets, replay every regression input, and run
# a short smoke campaign on each. Run from anywhere; needs a native build with a
# compile database (see CONTRIBUTING.md). On a shared Mac, prefix with
# `taskpolicy -b nice -n 19`.
#
#   tests/fuzz/run.sh [BUILD_DIR] [SMOKE_SECONDS]
#
# BUILD_DIR defaults to build-macos, SMOKE_SECONDS to 30. Set SMOKE_SECONDS=0 to
# replay regressions only. A crash makes a target exit non-zero and stops the
# script; the offending input is written under BUILD_DIR/fuzz/crashes/.
#
# Longer campaigns (minutes to hours), reproducible from their seed:
#   build-macos/fuzz/oob -seed=1 -max_total_time=3600 -max_len=8192 \
#       -artifact_prefix=build-macos/fuzz/crashes/ build-macos/fuzz/seeds/oob
# Minimise a crash, then add it under tests/fuzz/regressions/<target>/:
#   build-macos/fuzz/oob -minimize_crash=1 \
#       -artifact_prefix=build-macos/fuzz/crashes/ <crashing-input>
#
# On a toolchain whose clang ships libFuzzer (Homebrew LLVM, not Apple clang),
# build the same targets against it and fuzz with the upstream engine:
#   tests/fuzz/build.py --libfuzzer
#   build-macos/fuzz/oob -max_total_time=3600 build-macos/fuzz/seeds/oob
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=${1:-"$root/build-macos"}
smoke=${2:-30}
cd "$root"

if [ ! -f "$build/compile_commands.json" ]; then
    echo "No compile database at $build/compile_commands.json." >&2
    echo "Configure a native build first (see CONTRIBUTING.md)." >&2
    exit 2
fi

out="$build/fuzz"
seeds="$out/seeds"
crashes="$out/crashes"
mkdir -p "$crashes"

echo "== building targets =="
python3 tests/fuzz/build.py --build "$build"
python3 tests/fuzz/make_seeds.py "$seeds"

targets=$(cd tests/fuzz/targets && ls *.c | sed 's/\.c$//')

status=0
for target in $targets; do
    exe="$out/$target"
    [ -x "$exe" ] || { echo "missing target $exe" >&2; exit 2; }

    echo "== $target: regressions =="
    regdir="tests/fuzz/regressions/$target"
    if [ -d "$regdir" ] && [ -n "$(ls -A "$regdir" 2>/dev/null)" ]; then
        "$exe" "$regdir"/* || status=1
    else
        echo "  (none)"
    fi

    if [ "$smoke" -gt 0 ]; then
        echo "== $target: ${smoke}s smoke campaign =="
        maxlen=$(cat "$out/$target.max_len" 2>/dev/null || echo 4096)
        "$exe" -seed=1 -max_total_time="$smoke" -max_len="$maxlen" \
            -artifact_prefix="$crashes/$target-" "$seeds/$target" || status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "== all targets passed =="
else
    echo "== FAILURES: inspect $crashes ==" >&2
fi
exit "$status"
