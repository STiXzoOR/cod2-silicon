#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-chains.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
sdk=$(/usr/bin/xcrun --show-sdk-path)
optimization=-O
if [ "${CONFIG_CHAINS_DEBUG:-0}" = 1 ]; then optimization=-Onone; fi
for unit in NativeSetup NativeShaders URL; do
    case "$unit" in
        NativeSetup) source="$root/launcher/NativeSetup.m" ;;
        NativeShaders) source="$root/src/platform/cod2x_native_shaders.m" ;;
        URL) source="$root/src/PC/qcommon/cod2x_url.c" ;;
    esac
    taskpolicy -b nice -n 19 /usr/bin/xcrun clang -target arm64-apple-macos13 -isysroot "$sdk" -fobjc-arc -DCOD2_X64=1 -DCOD2_CODX=1 -c "$source" -o "$scratch/$unit.o"
done
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" "$optimization" -import-objc-header "$root/launcher/NativeSetup.h" \
    "$root/launcher/Core.swift" "$root/launcher/Presentation.swift" \
    "$root/launcher/ConfigDocument.swift" "$root/launcher/ConfigProfiles.swift" "$root/launcher/ConfigHistory.swift" "$root/launcher/ConfigStudioSession.swift" \
    "$root/launcher/ConfigImport.swift" "$root/launcher/ConfigImportSafety.swift" \
    "$root/launcher/ConfigChains.swift" "$root/tests/launcher/ConfigChainsTests.swift" \
    "$scratch/NativeSetup.o" "$scratch/NativeShaders.o" "$scratch/URL.o" \
    -framework AppKit -lz -o "$scratch/ConfigChainsTests"
TMPDIR="$scratch" taskpolicy -b nice -n 19 "$scratch/ConfigChainsTests" "$scratch"
python3 "$root/tests/launcher/engine_chains.py" "$scratch"
