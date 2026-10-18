#!/bin/sh
# Usage: run_dispatch_entrypoints.sh [source-root [output-directory]]
set -eu
fixture_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=${1:-$(CDPATH= cd -- "$fixture_dir/../.." && pwd)}
build=${2:-${COD2_METAL_TEST_BUILD_DIR:-"$repo/build-macos/metal/dispatch"}}
mkdir -p "$build"

# Compile complete, unmodified production files. Dead stripping excludes the
# unrelated renderer implementation and its platform dependencies.
for unit in CDirect3D CDirect3DDevice; do
    taskpolicy -b nice -n 19 clang -mmacosx-version-min=26.0 -std=c11 \
        -DCOD2_X64=1 -I"$repo/src" -I"$repo/src/headers" -ffp-contract=off \
        -ffunction-sections -fdata-sections -g -UNDEBUG \
        -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
        -Wno-incompatible-pointer-types -Wno-deprecated-non-prototype \
        -Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION \
        -c "$repo/src/Mac/DirectX_9/$unit.c" -o "$build/$unit.o"
done
taskpolicy -b nice -n 19 clang -mmacosx-version-min=26.0 -std=c11 \
    -DCOD2_X64=1 -I"$repo/src" -I"$repo/src/headers" -ffp-contract=off \
    -g -UNDEBUG -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
    -Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION -Wl,-dead_strip \
    "$fixture_dir/dispatch_entrypoints.c" "$build/CDirect3D.o" \
    "$build/CDirect3DDevice.o" -o "$build/dispatch-entrypoints-test"
taskpolicy -b nice -n 19 "$build/dispatch-entrypoints-test"
