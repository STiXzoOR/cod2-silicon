#!/bin/sh
# Redraws background.png and background@2x.png: background.py writes the SVG, headless Chrome
# renders it at 660 by 420 and at twice that.
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
design=${1:-dusk}
chrome="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/cod2-dmg.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
python3 "$here/background.py" "$design" "$scratch/background.svg"
for scale in 1 2; do
    out="$here/background.png"; [ "$scale" = 1 ] || out="$here/background@2x.png"
    "$chrome" --headless=new --disable-gpu --hide-scrollbars --allow-file-access-from-files \
        --force-device-scale-factor="$scale" --window-size=660,420 --virtual-time-budget=3000 \
        --screenshot="$out" "file://$scratch/background.svg" >/dev/null 2>&1
done
