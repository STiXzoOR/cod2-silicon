#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build=${COD2_METAL_TEST_BUILD_DIR:-build-macos/metal/resources}
mkdir -p "$build"
common='-mmacosx-version-min=26.0 -DCOD2_X64=1 -Isrc -Isrc/headers -ffp-contract=off -g -UNDEBUG -Wno-typedef-redefinition -Wno-duplicate-decl-specifier -Wno-incompatible-pointer-types -Wno-deprecated-non-prototype -Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION'
sanitizers=${COD2_RESOURCE_SANITIZERS--fsanitize=address,undefined}
taskpolicy -b nice -n 19 clang $common $sanitizers tests/metal/resource_formats.c \
    -x c src/Mac/Metal/resource_formats.m -o "$build/formats-test"
"$build/formats-test"
taskpolicy -b nice -n 19 clang $common $sanitizers -c tests/metal/resource_gl_buffers.c -o "$build/gl-buffers.o"
taskpolicy -b nice -n 19 clang $common $sanitizers -c tests/metal/resource_bridge.c -o "$build/bridge.o"
taskpolicy -b nice -n 19 clang $common $sanitizers -c src/Mac/DirectX_9/MacOpenGLUtils.c -o "$build/gl-formats.o"
taskpolicy -b nice -n 19 clang $common $sanitizers -fobjc-arc -c src/Mac/Metal/metal_resources.m -o "$build/resources.o"
taskpolicy -b nice -n 19 clang $common $sanitizers -c src/Mac/Metal/metal_backend.m -o "$build/backend.o"
taskpolicy -b nice -n 19 clang $common $sanitizers -fobjc-arc -Wl,-dead_strip \
    tests/metal/resources.m src/Mac/Metal/metal_gpu.m src/Mac/Metal/metal_core.m \
    src/Mac/Metal/metal_dispatch.m src/Mac/Metal/resource_formats.m \
    "$build/bridge.o" "$build/gl-buffers.o" "$build/gl-formats.o" "$build/resources.o" "$build/backend.o" \
    -framework Foundation -framework Metal -framework OpenGL -o "$build/test"
set +e
"$build/test"
resource_fixture_exit=$?
set -e
if [ "$resource_fixture_exit" -eq 77 ]; then exit 0; fi
exit "$resource_fixture_exit"
