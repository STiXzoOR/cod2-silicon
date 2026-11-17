#!/bin/sh
# Original synthetic linked-raster GPU tests; no licensed cache is required.
set -eu
cd "$(dirname "$0")/../.."
if [ "$(uname -s)" != Darwin ] || ! command -v clang++ >/dev/null; then
    echo 'SKIP: Metal raster fixtures require macOS and clang++.'
    exit 0
fi
build=output/ws32-raster
mkdir -p "$build"
taskpolicy -b nice -n 19 clang++ -std=c++17 -Wall -Wextra -Werror \
    -Wno-deprecated-declarations -Wno-missing-field-initializers \
    -ffp-contract=off -fobjc-arc -fsanitize=address,undefined -Isrc/Mac/Metal \
    tests/metal/raster.mm src/Mac/Metal/arb_to_msl.cpp \
    -framework Foundation -framework Metal -framework OpenGL -o "$build/raster-test"
"$build/raster-test"
