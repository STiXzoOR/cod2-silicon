#!/bin/sh
# Real GL pixel checks; only synthetic triangles in a private CGL framebuffer.
set -eu
mkdir -p build-macos/ws6-tests
clang -arch arm64 -std=c11 -DCOD2_X64=1 -DGL_SILENCE_DEPRECATION \
    -Isrc -Isrc/headers -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
    -fsanitize=address,undefined -framework OpenGL tests/lp64/renderer/shader_raster.c \
    -o build-macos/ws6-tests/shader_raster
build-macos/ws6-tests/shader_raster
clang -arch arm64 -std=c11 -DCOD2_X64=1 -DCOD2_APPLE_SDK=1 -DGL_SILENCE_DEPRECATION \
    -Isrc -Isrc/headers -Wno-typedef-redefinition -Wno-duplicate-decl-specifier \
    -Wno-incompatible-pointer-types -Wno-deprecated-non-prototype -Wl,-dead_strip \
    -fsanitize=address,undefined -framework OpenGL tests/lp64/renderer/saved_screen.c \
    -o build-macos/ws6-tests/saved_screen
build-macos/ws6-tests/saved_screen
