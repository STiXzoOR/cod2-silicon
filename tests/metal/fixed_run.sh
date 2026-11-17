#!/bin/sh
# Original offscreen fixtures; never use the licensed cache or xcrun metal.
set -eu
cd "$(dirname "$0")/../.."
if ! command -v clang++ >/dev/null; then
    echo 'SKIP fixed shaders: clang++ is unavailable.'
    exit 0
fi
run_compiler() {
    if command -v taskpolicy >/dev/null; then taskpolicy -b nice -n 19 "$@"; else "$@"; fi
}
build=output/ws32-fixed
mkdir -p "$build"
run_compiler clang++ -std=c++17 -Wall -Wextra -Werror -ffp-contract=off \
    -fsanitize=address,undefined -Isrc/Mac/Metal tests/metal/fixed_contract.cpp \
    src/Mac/Metal/shader_fixed.cpp -o "$build/contract"
"$build/contract"
if [ "$(uname -s)" != Darwin ]; then
    echo 'SKIP fixed GPU: macOS Metal/CGL frameworks are unavailable.'
    exit 0
fi
run_compiler clang++ -std=c++17 -Wall -Wextra -Werror \
    -Wno-deprecated-declarations -ffp-contract=off -fobjc-arc -Isrc/Mac/Metal \
    tests/metal/fixed_gpu.mm src/Mac/Metal/shader_fixed.cpp \
    -framework Foundation -framework Metal -framework OpenGL -o "$build/gpu"
"$build/gpu"
