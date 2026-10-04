#!/bin/sh
# Preview the real system-rendered icon appearances with Xcode's Icon Composer.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../../.." && pwd)
output=${1:-"$root/output/ws28/icon"}
size=${2:-1024}
developer_dir=$(xcode-select -p)
tool="$developer_dir/../Applications/Icon Composer.app/Contents/Executables/ictool"
if [ ! -x "$tool" ]; then
    printf '%s\n' 'Icon Composer ictool is missing from the selected Xcode installation.' >&2
    exit 2
fi
mkdir -p "$output"
for appearance in Default Dark ClearLight ClearDark TintedLight TintedDark; do
    "$tool" "$root/tools/cod2x/icon-source/CoD2 Silicon.icon" \
        --export-image --output-file "$output/CoD2 Silicon-$appearance.png" \
        --platform macOS --rendition "$appearance" \
        --width "$size" --height "$size" --scale 1 >/dev/null
done
printf 'Rendered six appearances to %s\n' "$output"
