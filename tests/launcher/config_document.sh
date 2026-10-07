#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-config-document.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O \
    "$root/launcher/ConfigDocument.swift" "$root/tests/launcher/ConfigDocumentTests.swift" \
    -o "$scratch/ConfigDocumentTests"
"$scratch/ConfigDocumentTests" "$@"
