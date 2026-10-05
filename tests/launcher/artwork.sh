#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
out="$root/output/ws25/artwork-tests"
mkdir -p "$out"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-artwork.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
python3 "$root/tests/launcher/artwork_fixtures.py" "$scratch"
/usr/bin/xcrun swiftc -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O \
    "$root/launcher/MapArtwork.swift" "$root/tests/launcher/ArtworkTests.swift" \
    -framework AppKit -framework CryptoKit -lz -o "$out/ArtworkTests"
CFFIXED_USER_HOME="$scratch/home" "$out/ArtworkTests" "$scratch"
