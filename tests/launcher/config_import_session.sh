#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-import-session.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
sdk=$(/usr/bin/xcrun --show-sdk-path)
taskpolicy -b nice -n 19 bash "$root/scripts/build-launcher.sh" "$scratch/core" --test
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$sdk" -O -import-objc-header "$root/launcher/NativeSetup.h" \
    "$root/launcher/Core.swift" "$root/launcher/Presentation.swift" \
    "$root/launcher/ConfigDocument.swift" "$root/launcher/ConfigProfiles.swift" "$root/launcher/ConfigHistory.swift" \
    "$root/launcher/ConfigStudioSession.swift" "$root/launcher/ConfigCatalog.swift" \
    "$root/launcher/ConfigImportCatalogAdapter.swift" \
    "$root/launcher/ConfigImport.swift" "$root/launcher/ConfigImportSafety.swift" "$root/launcher/ConfigImportSession.swift" \
    "$root/tests/launcher/ConfigImportSessionTests.swift" \
    "$scratch/core/NativeSetup.o" "$scratch/core/NativeShaders.o" "$scratch/core/URL.o" \
    -framework AppKit -lz -o "$scratch/ConfigImportSessionTests"
COD2_IMPORT_SESSION_SCRATCH="$scratch" taskpolicy -b nice -n 19 "$scratch/ConfigImportSessionTests" "$root/launcher/Resources"
