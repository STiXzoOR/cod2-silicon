#!/bin/bash
# Separate from client .app packaging. No data or converted shader input allowed.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
binary=${1:-"$root/build-macos/cod2_macos_ded"}
output=${2:-"$root/output/server-release"}
[[ $(uname -s) = Darwin && -x $binary ]] || { echo 'Need a built native macOS server' >&2; exit 1; }
[[ $(lipo -archs "$binary") = arm64 ]] || { echo 'Expected arm64 server' >&2; exit 1; }
# System libraries only: fail if a renderer/SDL or local build dependency slips in.
while IFS= read -r dependency; do
    case "$dependency" in /usr/lib/*|/System/Library/*) ;; *) echo "Non-system dependency: $dependency" >&2; exit 1;; esac
done < <(otool -L "$binary" | awk 'NR > 1 {print $1}')
mkdir -p "$output"
output=$(cd "$output" && pwd)
revision=$(git -C "$root" rev-parse --short HEAD)
name="cod2-silicon-server-macos-arm64-$revision"
archive="$output/$name.tar.gz"
[[ ! -e $archive ]] || { echo "Archive already exists: $archive" >&2; exit 1; }
stage=$(mktemp -d "$output/.server-stage.XXXXXX")
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/$name/bin" "$stage/$name/scripts/server" "$stage/$name/docs"
cp "$binary" "$stage/$name/bin/cod2_macos_ded"
cp "$root/LICENSE" "$stage/$name/LICENSE"
cp "$root/docs/server.md" "$stage/$name/docs/server.md"
cp "$root/scripts/server/cod2-silicon-server" "$root/scripts/server/server.cfg" \
    "$root/scripts/server/io.github.stixzoor.cod2silicon.server.plist" \
    "$root/scripts/server/io.github.stixzoor.cod2silicon.daemon.plist" "$stage/$name/scripts/server/"
cat > "$stage/$name/README.txt" <<'EOF'
CoD2 Silicon server: native arm64, stock 1.3.
Read docs/server.md before use. Supply your own licensed, patched game data.
Install with scripts/server/cod2-silicon-server using an absolute path to
bin/cod2_macos_ded and your data directory. The binary uses system libraries.
Validation report in the source checkout: docs/macos-port/reports/WS26-dedicated.md
EOF
COPYFILE_DISABLE=1 /usr/bin/tar -czf "$archive" -C "$stage" "$name"
shasum -a 256 "$archive" > "$archive.sha256"
echo "$archive"
