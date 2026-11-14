#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-checker.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos26 -sdk "$(/usr/bin/xcrun --sdk macosx --show-sdk-path)" -O -parse-as-library \
    "$root/launcher/ConfigDocument.swift" "$root/launcher/ConfigImport.swift" "$root/launcher/ConfigImportSafety.swift" \
    "$root/launcher/ConfigCatalog.swift" "$root/launcher/ConfigImportCatalogAdapter.swift" "$root/launcher/ConfigRules.swift" \
    "$root/launcher/ConfigChecker.swift" "$root/tests/launcher/ConfigCheckerTests.swift" \
    -o "$scratch/ConfigCheckerTests"
"$scratch/ConfigCheckerTests" "$root/launcher/Resources"
