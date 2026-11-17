#!/bin/sh
# Standalone native shader setup and synthetic parser tests; no game integration.
set -eu
cd "$(dirname "$0")/../.."
if ! command -v clang++ >/dev/null; then
    echo 'SKIP: clang++ / Apple developer tools are unavailable.'
    exit 0
fi
sh tests/metal/run_states.sh
python3 tests/metal/observed_states.py -v
build=build-macos/metal
mkdir -p "$build"
taskpolicy -b nice -n 19 clang++ -std=c++17 -Wall -Wextra -Werror -ffp-contract=off \
    -fsanitize=address,undefined -Isrc/Mac/Metal tests/metal/parser.cpp \
    src/Mac/Metal/arb_to_msl.cpp -o "$build/parser-test"
"$build/parser-test"
taskpolicy -b nice -n 19 clang++ -std=c++17 -Wall -Wextra -Werror -ffp-contract=off \
    -Isrc/Mac/Metal tools/metal/translate.cpp src/Mac/Metal/arb_to_msl.cpp \
    -o "$build/arb_to_msl"
taskpolicy -b nice -n 19 clang++ -std=c++17 -Wall -Wextra -Werror \
    -Wno-deprecated-declarations -ffp-contract=off -fobjc-arc -Isrc/Mac/Metal \
    tools/metal/prepare.mm src/Mac/Metal/shader_setup.mm src/Mac/Metal/arb_to_msl.cpp \
    -framework Foundation -framework Metal -o "$build/prepare_shaders"
cache=${1:-${COD2_MAC_SHADER_CACHE:-"$HOME/Library/Application Support/CoD2x Native/shaders"}}
if [ ! -d "$cache" ] && [ "$#" -eq 0 ]; then
    cache="$HOME/Library/Application Support/CoD2 Silicon/shaders"
fi
destination=${2:-"$HOME/Library/Application Support/CoD2-native-ws24/evidence/translated"}
set +e
"$build/prepare_shaders" "$cache" "$destination"
status=$?
set -e
if [ "$status" -eq 77 ]; then exit 0; fi
exit "$status"
