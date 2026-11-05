#!/bin/sh
set -eu
mkdir -p build-macos/ws6-tests
clang -arch arm64 -std=c11 -ffp-contract=off -DCOD2_X64=1 -DGL_SILENCE_DEPRECATION \
 -Isrc -Isrc/headers -Wno-deprecated-non-prototype -Wno-incompatible-pointer-types \
 -Wno-typedef-redefinition -Wno-duplicate-decl-specifier -Wl,-dead_strip -framework OpenGL \
 -fsanitize=address,undefined tests/lp64/renderer/volume_upload.c \
 src/Mac/DirectX_9/CDirect3DTexture.c src/Mac/DirectX_9/CDirect3DSurface.c \
 src/Mac/DirectX_9/CDirect3DCubeTexture.c src/Mac/DirectX_9/CVertexArrays.c \
 "${COD2_OPENGL_PROVIDER_SOURCE:-src/Mac/DirectX_9/COpenGL.c}" \
 src/Mac/DirectX_9/MacOpenGLUtils.c -o build-macos/ws6-tests/volume_upload
build-macos/ws6-tests/volume_upload
