#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build=${COD2_METAL_TEST_BUILD_DIR:-build-macos/metal}
mkdir -p "$build"
python3 tools/metal/add_dispatch.py --check
taskpolicy -b nice -n 19 clang -mmacosx-version-min=26.0 -ffp-contract=off -DCOD2_X64=1 -Isrc/Mac/Metal -std=c11 \
  -fsanitize=address,undefined -g -UNDEBUG -x c tests/metal/core.c \
  src/Mac/Metal/metal_core.m src/Mac/Metal/metal_dispatch.m -framework CoreFoundation -o "$build/core-test"
"$build/core-test"
