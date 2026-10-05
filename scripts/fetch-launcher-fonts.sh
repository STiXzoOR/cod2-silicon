#!/bin/sh
# Refresh the launcher's two SIL OFL 1.1 typefaces from the official google/fonts
# repository. The files are committed; this script documents and verifies them.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
destination="$root/launcher/Resources/Fonts"
commit=9710da1eacb3be272583c3224dcb70f9da6eadbb
base="https://raw.githubusercontent.com/google/fonts/$commit/ofl"
check_only=0
if [ "${1:-}" = --check ]; then check_only=1; fi

# upstream path | local name | SHA-256
manifest='bigshouldersstencildisplay/BigShouldersStencilDisplay%5Bwght%5D.ttf|BigShouldersStencilDisplay.ttf|93bbefb9f3a497fcfeec5d1ac85c4a947a32b8e5bcaf3b7b5efff52f1b11b0e4
bigshouldersstencildisplay/OFL.txt|OFL-BigShouldersStencilDisplay.txt|338f9c050f19daeda1d597243faf79f3a3d437c338af58cb7047617d0ce08771
courierprime/CourierPrime-Regular.ttf|CourierPrime-Regular.ttf|72f793376f8e2841656bf21d77a5de010f2929bd6956a22ee848ad0c7eb978af
courierprime/CourierPrime-Bold.ttf|CourierPrime-Bold.ttf|ff1f38786c849d1c41fa8e447960abdb2bd75fdfb0cfcdeb524fad65a5af3638
courierprime/OFL.txt|OFL-CourierPrime.txt|9a755af092b494944c99f471be6fddd19b006a448fefdc4717e4ee0aa09a97b0'

mkdir -p "$destination"
status=0
printf '%s\n' "$manifest" | while IFS='|' read -r path name digest; do
    target="$destination/$name"
    if [ "$check_only" -eq 0 ]; then
        temporary="$target.download"
        /usr/bin/curl -fsSL --proto '=https' --tlsv1.2 -o "$temporary" "$base/$path"
        actual=$(/usr/bin/shasum -a 256 "$temporary" | /usr/bin/cut -d ' ' -f 1)
        if [ "$actual" != "$digest" ]; then
            rm -f "$temporary"
            printf 'Checksum mismatch for %s: %s\n' "$name" "$actual" >&2
            exit 1
        fi
        mv "$temporary" "$target"
    fi
    actual=$(/usr/bin/shasum -a 256 "$target" | /usr/bin/cut -d ' ' -f 1)
    if [ "$actual" != "$digest" ]; then
        printf 'Checksum mismatch for %s\n' "$target" >&2
        exit 1
    fi
done || status=$?
if [ "$status" -ne 0 ]; then exit "$status"; fi
printf 'Launcher fonts verified (google/fonts %s).\n' "$commit"
