#!/bin/sh
# Compile the original Icon Composer document without an Xcode project or GUI.
# Emits Assets.car (Liquid Glass icon for macOS 26+ in every appearance plus
# flattened renditions, and the launcher's brass/olive accent colour),
# "CoD2 Silicon.icns" and icon-info.plist.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    printf '%s\n' 'usage: compile-launcher-icon.sh OUTPUT_DIRECTORY [SOURCE_ICON]' >&2
    exit 2
fi
output=$1
source_icon=${2:-"$root/tools/cod2x/icon-source/CoD2 Silicon.icon"}
name=$(basename "$source_icon" .icon)
if ! actool=$(/usr/bin/xcrun --find actool 2>/dev/null); then
    printf '%s\n' 'actool is unavailable (it ships with Xcode, not the Command Line Tools).' >&2
    exit 3
fi
mkdir -p "$output"
"$actool" "$source_icon" "$root/launcher/Resources/Accent.xcassets" --compile "$output" --platform macosx \
    --minimum-deployment-target 13.0 --app-icon "$name" --accent-color AccentColor \
    --output-partial-info-plist "$output/icon-info.plist" \
    --output-format human-readable-text --warnings --errors
# Consumers merge icon-info.plist into Info.plist and copy Assets.car plus ICNS.
