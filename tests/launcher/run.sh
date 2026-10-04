#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
out="$root/output/ws25/unit"
mkdir -p "$out"
"$root/scripts/build-launcher.sh" "$out" --test
"$out/LauncherTests" "$root/tests/launcher/fixtures"
