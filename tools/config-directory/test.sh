#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
root=${1:-$(CDPATH= cd -- "$here/../.." && pwd)}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-directory-tools.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
sh "$here/build-audit.sh" "$root" "$scratch"
TMPDIR="$scratch" COD2_DIRECTORY_AUDIT="$scratch/audit" taskpolicy -b nice -n 19 python3 "$here/test_generate.py"
taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -j 1 \
    -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -target arm64-apple-macos13 -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O -parse-as-library \
    "$here/SignatureToolTests.swift" -o "$scratch/SignatureToolTests"
taskpolicy -b nice -n 19 "$scratch/SignatureToolTests" "$scratch"
taskpolicy -b nice -n 19 python3 "$here/generate.py" --root "$here" \
    --base-url https://fixtures.invalid/ --revision 1 --audit-binary "$scratch/audit" --check
