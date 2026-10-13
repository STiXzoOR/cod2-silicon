#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-input-devices.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
python3 "$root/tests/launcher/engine_keymap.py" "$scratch"
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors -j 2 \
    -target arm64-apple-macos26 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O \
    "$root/launcher/ConfigDocument.swift" "$root/launcher/InputDevices.swift" \
    "$root/tests/launcher/InputDevicesTests.swift" "$root/tests/launcher/InputDevicesTestMain.swift" \
    -o "$scratch/InputDevicesTests"
TMPDIR="$scratch" taskpolicy -b nice -n 19 "$scratch/InputDevicesTests" "$scratch/engine-keymap.tsv" "$@"
