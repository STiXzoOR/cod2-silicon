#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-catalog.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O \
    "$root/launcher/ConfigCatalog.swift" "$root/tests/launcher/ConfigCatalogTests.swift" \
    -o "$scratch/ConfigCatalogTests"
"$scratch/ConfigCatalogTests" "${1:-$root/launcher/Resources/ConfigCatalog.json}"
