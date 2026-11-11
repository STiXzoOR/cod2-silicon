#!/bin/sh
# Discover/import and Servers ready-check view stores, built with the real launcher sources.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-discover.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
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
    $sources "$root/tests/launcher/DirectoryFixture.swift" "$root/tests/launcher/ConfigDiscoverTests.swift" \
    "$scratch/NativeSetup.o" "$scratch/NativeShaders.o" "$scratch/URL.o" \
    -framework AppKit -framework SwiftUI -framework Network -lz -o "$scratch/ConfigDiscoverTests"
cp "$root/launcher/Resources/ConfigCatalog.json" "$root/launcher/Resources/ConfigRulesets.json" "$scratch/"
TMPDIR="$scratch" taskpolicy -b nice -n 19 "$scratch/ConfigDiscoverTests"
