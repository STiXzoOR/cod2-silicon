#!/bin/sh
# Signs a catalogue's index.json with the maintainer's key and writes index.sig and index.pub beside it.
#
#     sh sign-index.sh <catalogue folder> <private key file>
#
# The key file is created on first use, readable by its owner only. Keep it outside every repository
# and back it up: the launcher trusts the catalogue through its public half, so a lost key means a
# launcher update before the catalogue can change again.
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
root=$(CDPATH= cd -- "$1" && pwd)
key=$2
case "$key" in "$root"/*) echo "Keep the key outside the catalogue folder." >&2; exit 1 ;; esac
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-directory-sign.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
/usr/bin/xcrun swiftc -swift-version 6 -target "$(uname -m)-apple-macos13" -sdk "$(/usr/bin/xcrun --show-sdk-path)" -O -parse-as-library \
    "$here/Sign.swift" -o "$scratch/sign"
if [ ! -e "$key" ]; then
    mkdir -p "$(dirname "$key")"
    "$scratch/sign" create "$key"
    echo "Created a new signing key at $key. Back it up."
fi
"$scratch/sign" public "$key" "$scratch/public"
"$scratch/sign" sign "$root/index.json" "$key" "$root/index.sig"
"$scratch/sign" verify "$root/index.json" "$scratch/public" "$root/index.sig"
/usr/bin/base64 -i "$scratch/public" > "$root/index.pub"
echo "Signed $root/index.json. Public key: $(/bin/cat "$root/index.pub")"
