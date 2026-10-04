#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd -P)
# shellcheck source=scripts/macos-common.sh
source "$root/scripts/macos-common.sh"
version=0.1.0
build="$root/build/package/release"
mac_binary=${COD2_VALUES_BINARY:-}
stabs_binary=${COD2_STABS_BINARY:-}
while [[ $# -gt 0 ]]; do
    case $1 in
        --version) version=${2:?missing version}; shift 2 ;;
        --build-dir) build=${2:?missing directory}; shift 2 ;;
        --mac-binary) mac_binary=${2:?missing path}; shift 2 ;;
        --stabs-binary) stabs_binary=${2:?missing path}; shift 2 ;;
        *) printf 'Usage: %s [--version 0.1.0] [--build-dir DIR] [--mac-binary FILE] [--stabs-binary FILE]\n' "$0" >&2; exit 2 ;;
    esac
done
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then printf 'Version must be N.N.N.\n' >&2; exit 2; fi
mkdir -p "$build" "$root/dist"
build=$(cd "$build" && pwd -P)
app="$build/CoD2 Silicon.app"
build_app "$root" "$build" "$app" "$version" "$mac_binary" "$stabs_binary"
archive="CoD2-Silicon-$version-macos-arm64.zip"
/usr/bin/ditto -c -k --keepParent "$app" "$root/dist/$archive"
(cd "$root/dist" && /usr/bin/shasum -a 256 "$archive" > SHA256SUMS)
printf 'Created dist/%s and dist/SHA256SUMS (ad-hoc signed, not notarized).\n' "$archive"
