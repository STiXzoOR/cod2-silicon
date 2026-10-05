#!/bin/sh
# Run from the repository root. Synthetic assets only; no game or GL context.
set -eu
mkdir -p build-macos/ws6-tests
for name in gl_buffers gl_state material_disk font_disk commands static_model_cache shader_arguments shader_cache shader_options bitmap_channels lighting_lookup wavelet_loading gamma_mapping fog_mapping dpvs_planes portal_visibility volume_image_upload static_lighting outdoor_mapping code_samplers render_targets pixel_centers viewport_constants model_bounds; do
    clang -arch arm64 -std=c11 -ffp-contract=off -DCOD2_X64=1 -DGL_SILENCE_DEPRECATION \
        -Isrc -Isrc/headers -Wno-deprecated-non-prototype \
        -Wno-incompatible-pointer-types -Wno-typedef-redefinition \
        -Wno-duplicate-decl-specifier -Wl,-dead_strip \
        -fsanitize=address,undefined "tests/lp64/renderer/$name.c" \
        -o "build-macos/ws6-tests/$name"
    "build-macos/ws6-tests/$name"
done
