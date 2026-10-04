#!/bin/sh
# Preview the real system-rendered icon appearances with Xcode's Icon Composer.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../../.." && pwd)
output=${1:-"$root/output/ws25/screens-v2/icons"}
developer_dir=$(xcode-select -p)
tool="$developer_dir/../Applications/Icon Composer.app/Contents/Executables/ictool"
if [ ! -x "$tool" ]; then
    printf '%s\n' 'Icon Composer ictool is missing from the selected Xcode installation.' >&2
    exit 2
fi
mkdir -p "$output"
for concept in CoD2Silicon Helmet DogTag; do
    for appearance in Default Dark ClearLight ClearDark TintedLight TintedDark; do
        "$tool" "$root/tools/cod2x/icon-source/$concept.icon" \
            --export-image --output-file "$output/$concept-$appearance.png" \
            --platform macOS --rendition "$appearance" \
            --width 1024 --height 1024 --scale 1 --design-generation 26
    done
done
