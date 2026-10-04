#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd -P)
# shellcheck source=scripts/macos-common.sh
source "$root/scripts/macos-common.sh"
prefix="$HOME/Applications"
build="$root/build/package/install"
mac_binary=${COD2_VALUES_BINARY:-}
stabs_binary=${COD2_STABS_BINARY:-}
while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix) prefix=${2:?missing directory}; shift 2 ;;
        --build-dir) build=${2:?missing directory}; shift 2 ;;
        --mac-binary) mac_binary=${2:?missing path}; shift 2 ;;
        --stabs-binary) stabs_binary=${2:?missing path}; shift 2 ;;
        *) printf 'Usage: %s [--prefix DIR] [--build-dir DIR] [--mac-binary FILE --stabs-binary FILE]\n(the two reference binaries are optional and only verify the committed typed-data snapshot)\n' "$0" >&2; exit 2 ;;
    esac
done
check_prerequisites
mkdir -p "$prefix" "$build"
prefix=$(cd "$prefix" && pwd -P)
build=$(cd "$build" && pwd -P)
build_app "$root" "$build" "$prefix/CoD2 Silicon.app" 0.1.0 "$mac_binary" "$stabs_binary"
printf 'Installed %s/CoD2 Silicon.app. Open it to choose your game data and enter your CD key.\n' "$prefix"
