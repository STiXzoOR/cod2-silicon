#!/bin/sh
# Compile original Icon Composer sources without an Xcode project or GUI.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    printf '%s\n' 'usage: compile-launcher-icon.sh OUTPUT_DIRECTORY [SOURCE_ICON]' >&2
    exit 2
fi
output=$1
source_icon=${2:-"$root/tools/cod2x/icon-source/CoD2Silicon.icon"}
name=$(basename "$source_icon" .icon)
mkdir -p "$output"
xcrun actool "$source_icon" --compile "$output" --platform macosx \
    --minimum-deployment-target 13.0 --app-icon "$name" \
    --output-partial-info-plist "$output/icon-info.plist" \
    --output-format human-readable-text --warnings --errors
# Consumers merge icon-info.plist into Info.plist and copy Assets.car plus ICNS.
