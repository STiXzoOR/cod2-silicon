#!/bin/sh
set -eu
fixture_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_root=${1:-$(CDPATH= cd -- "$fixture_dir/../.." && pwd)}
build=${COD2_METAL_TEST_BUILD_DIR:-"$fixture_dir/build/retirement"}
mkdir -p "$build"

# Both compilation and execution stay at background priority. Link only this
# C translation unit; unused backend paths need no unrelated provider stubs.
taskpolicy -b nice -n 19 clang \
    -mmacosx-version-min=26.0 -DCOD2_X64=1 -ffp-contract=off -g -UNDEBUG \
    -std=c11 -fsanitize=address,undefined \
    -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
    -Wno-incompatible-pointer-types -Wno-deprecated-non-prototype \
    -Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION \
    -I"$source_root/src" -I"$source_root/src/headers" \
    -I"$source_root/src/Mac/Metal" \
    -Wl,-dead_strip "$fixture_dir/retire_payload.c" -o "$build/retire-payload-test"
taskpolicy -b nice -n 19 "$build/retire-payload-test"
