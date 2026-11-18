#!/bin/bash
# Makes the disk image people download: the app, a link to Applications and a laid-out window.
#
#   scripts/make-dmg.sh "<path>/CoD2 Unbound.app" <out.dmg>
#
# The image is built by dmgbuild (https://github.com/dmgbuild/dmgbuild), which is not part of this
# repository. It is used from PATH when installed (`pipx install dmgbuild`), otherwise through
# `uvx`, pinned to the version this layout was made with. Signing and notarizing the image is
# scripts/package-release.sh's job.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd -P)
if [[ $# -ne 2 || ! -d $1 ]]; then printf 'Usage: %s "<app>" <out.dmg>\n' "$0" >&2; exit 2; fi
app=$(cd "$1" && pwd -P)
out=$2
pinned=2.0.0
if command -v dmgbuild >/dev/null 2>&1; then tool=(dmgbuild)
elif command -v uvx >/dev/null 2>&1; then tool=(uvx --from "dmgbuild==$pinned" dmgbuild)
else
    printf 'The disk image needs dmgbuild: install it with "pipx install dmgbuild==%s", or install uv.\n' "$pinned" >&2
    exit 3
fi
rm -f "$out"
"${tool[@]}" -s "$root/tools/dmg/settings.py" -D app="$app" -D background="$root/tools/dmg/background.png" \
    "$(basename "$app" .app)" "$out"
