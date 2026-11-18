#!/bin/sh
# WS32 independent provider gate: synthetic fixtures always run; local cache skips.
set -eu
cd "$(dirname "$0")/../.."
if [ "$(uname -s)" != Darwin ] || ! command -v clang++ >/dev/null; then
    echo 'SKIP shaders provider: macOS Metal and developer compiler are required.'
    exit 0
fi
sh tests/metal/raster.sh
sh tests/metal/fixed_run.sh
build=build-macos/metal
mkdir -p "$build"
taskpolicy -b nice -n 19 clang++ -std=c++17 -Wall -Wextra -Werror \
    -Wno-deprecated-declarations -Werror=unguarded-availability \
    -Werror=unguarded-availability-new -ffp-contract=off -fobjc-arc -Isrc/Mac/Metal \
    tests/metal/provider.mm src/Mac/Metal/shader_provider.mm \
    src/Mac/Metal/shader_setup.mm src/Mac/Metal/shader_archive.mm \
    src/Mac/Metal/shader_fixed.cpp src/Mac/Metal/arb_to_msl.cpp \
    -framework Foundation -framework Metal -o "$build/provider-test"
cache=${1:-${COD2_MAC_SHADER_CACHE:-"$HOME/Library/Application Support/CoD2 Unbound/shaders"}}
destination=${2:-"$HOME/Library/Application Support/CoD2-native-ws32/provider"}
set +e
"$build/provider-test" "$cache" "$destination"
status=$?
set -e
if [ "$status" -eq 77 ]; then exit 0; fi
if [ "$status" -ne 0 ]; then exit "$status"; fi
"$build/provider-test" "$cache" "$destination" --cross-process
