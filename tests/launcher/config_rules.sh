#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-rules.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors -j 2 \
    -target arm64-apple-macos26 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O \
    "$root/launcher/ConfigCatalog.swift" "$root/launcher/ConfigRules.swift" \
    "$root/tests/launcher/ConfigRulesTests.swift" -o "$scratch/ConfigRulesTests"
taskpolicy -b nice -n 19 "$scratch/ConfigRulesTests" "${1:-$root/launcher/Resources/ConfigRulesets.json}"
