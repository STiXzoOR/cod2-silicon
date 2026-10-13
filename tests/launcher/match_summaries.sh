#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
out=${COD2_MATCH_TEST_OUTPUT:?set COD2_MATCH_TEST_OUTPUT to a private .noindex directory}
mkdir -p "$out"
sdk=$(/usr/bin/xcrun --show-sdk-path)
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos26 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/launcher/NativeSetup.m" -o "$out/NativeSetup.o"
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos26 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/src/platform/cod2x_native_shaders.m" -o "$out/NativeShaders.o"
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos26 -isysroot "$sdk" -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/src/PC/qcommon/cod2x_url.c" -o "$out/URL.o"
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos26 -sdk "$sdk" -O -import-objc-header "$root/launcher/NativeSetup.h" \
    "$root/launcher/Core.swift" "$root/launcher/Presentation.swift" "$root/launcher/MatchSummaries.swift" "$root/tests/launcher/MatchSummariesTests.swift" \
    "$out/NativeSetup.o" "$out/NativeShaders.o" "$out/URL.o" -framework AppKit -lz -o "$out/MatchSummariesTests"
taskpolicy -b nice -n 19 "$out/MatchSummariesTests" "$out/reader-home"
