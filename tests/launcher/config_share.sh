#!/bin/sh
# Share Your Setup without a window (ConfigShareTests.swift), built with the real launcher sources and
# run against the share service's stand-in on 127.0.0.1. No test reaches the network.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-share.XXXXXX")
standin=""
cleanup() { [ -n "$standin" ] && kill "$standin" 2>/dev/null; rm -rf "$scratch"; }
trap cleanup EXIT INT TERM
sdk=$(/usr/bin/xcrun --sdk macosx --show-sdk-path)
for object in "launcher/NativeSetup.m:NativeSetup" "src/platform/cod2x_native_shaders.m:NativeShaders"; do
    taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos26 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1 \
        -c "$root/${object%%:*}" -o "$scratch/${object##*:}.o"
done
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos26 -isysroot "$sdk" -DCOD2_X64=1 -DCOD2_CODX=1 \
    -c "$root/src/PC/qcommon/cod2x_url.c" -o "$scratch/URL.o"
sources=""
for file in "$root"/launcher/*.swift; do
    [ "${file##*/}" = Main.swift ] || sources="$sources $file"
done
# shellcheck disable=SC2086
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos26 -sdk "$sdk" -Onone -import-objc-header "$root/launcher/NativeSetup.h" \
    $sources "$root/tests/launcher/ConfigShareTests.swift" \
    "$scratch/NativeSetup.o" "$scratch/NativeShaders.o" "$scratch/URL.o" \
    -framework AppKit -framework SwiftUI -framework Network -lz -o "$scratch/ConfigShareTests"
cp "$root/launcher/Resources/ConfigCatalog.json" "$scratch/"
# The stand-in picks a free port on 127.0.0.1 and prints it; a slow reply waits 3 seconds.
python3 "$root/tools/config-directory/share_standin.py" --port 0 --delay 3 --record "$scratch/requests.jsonl" \
    > "$scratch/standin.out" 2> "$scratch/standin.log" &
standin=$!
address=""
for _ in $(seq 1 100); do
    address=$(sed -n 's/^share stand-in listening on //p' "$scratch/standin.out")
    [ -n "$address" ] && break
    sleep 0.1
done
[ -n "$address" ] || { echo "FAIL: the share stand-in did not start" >&2; cat "$scratch/standin.log" >&2; exit 1; }
mkdir -p "$scratch/home"
COD2_SHARE_URL="$address" COD2_SHARE_RECORD="$scratch/requests.jsonl" COD2_SHARE_TEST_HOME="$scratch/home" TMPDIR="$scratch" \
    taskpolicy -b nice -n 19 "$scratch/ConfigShareTests"
