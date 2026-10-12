#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-import.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors -j 2 \
    -target arm64-apple-macos13 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O \
    "$root/launcher/ConfigDocument.swift" "$root/launcher/ConfigImport.swift" \
    "$root/launcher/ConfigImportSafety.swift" "$root/launcher/WindowsProfiles.swift" \
    "$root/tests/launcher/ConfigImportTests.swift" -o "$scratch/ConfigImportTests"
COD2_IMPORT_TEST_SCRATCH="$scratch" taskpolicy -b nice -n 19 "$scratch/ConfigImportTests" "$@"
