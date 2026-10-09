#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-profiles.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
sdk=$(/usr/bin/xcrun --show-sdk-path)
optimization=-O
if [ "${CONFIG_PROFILES_DEBUG:-0}" = 1 ]; then optimization=-Onone; fi
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos13 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/launcher/NativeSetup.m" -o "$scratch/NativeSetup.o"
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos13 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/src/platform/cod2x_native_shaders.m" -o "$scratch/NativeShaders.o"
taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos13 -isysroot "$sdk" -DCOD2_X64=1 -DCOD2_CODX=1 -c "$root/src/PC/qcommon/cod2x_url.c" -o "$scratch/URL.o"
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" "$optimization" -import-objc-header "$root/launcher/NativeSetup.h" \
    "$root/launcher/Core.swift" "$root/launcher/Presentation.swift" \
    "$root/launcher/ConfigDocument.swift" "$root/launcher/ConfigProfiles.swift" "$root/launcher/ConfigHistory.swift" \
    "$root/tests/launcher/ConfigProfilesTests.swift" \
    "$scratch/NativeSetup.o" "$scratch/NativeShaders.o" "$scratch/URL.o" \
    -framework AppKit -lz -o "$scratch/ConfigProfilesTests"
TMPDIR="$scratch" taskpolicy -b nice -n 19 "$scratch/ConfigProfilesTests" "$@"
