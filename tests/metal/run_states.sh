#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
compile() {
  if [ "$(uname -s)" = Darwin ]; then taskpolicy -b nice -n 19 "$@"; else nice -n 19 "$@"; fi
}
cpu_only=0
if [ "${1:-}" = --cpu-only ]; then cpu_only=1; shift; fi
out=build-macos/metal/states
mkdir -p "$out"
compile clang++ -std=c++17 -Wall -Wextra -Werror -ffp-contract=off \
  -fsanitize=address,undefined -UNDEBUG -Isrc/Mac/Metal tests/metal/states.cpp \
  src/Mac/Metal/metal_states.cpp -o "$out/cpu-test"
"$out/cpu-test"
if [ "$cpu_only" -eq 1 ]; then exit 0; fi
if [ "$(uname -s)" != Darwin ]; then
  echo 'SKIP: Metal GPU fixture requires macOS; CPU keys still tested.'
  exit 0
fi
sdk=$(/usr/bin/xcrun --sdk macosx --show-sdk-version)
if [ "${sdk%%.*}" -lt 26 ]; then
  echo 'SKIP: Metal 4 GPU fixture requires SDK 26+; CPU keys still tested.'
  exit 0
fi
for unit in metal_core metal_gpu; do
  compile clang -fobjc-arc -Isrc/Mac/Metal -mmacosx-version-min=26.0 \
    -Werror=unguarded-availability -Werror=unguarded-availability-new \
    -fsanitize=address,undefined -c "src/Mac/Metal/$unit.m" -o "$out/$unit.o"
done
compile clang++ -std=c++17 -fobjc-arc -ffp-contract=off -UNDEBUG \
  -Wall -Wextra -Werror -mmacosx-version-min=26.0 -Wno-deprecated-declarations \
  -Werror=unguarded-availability -Werror=unguarded-availability-new \
  -fsanitize=address,undefined -Isrc/Mac/Metal tests/metal/states_gpu.mm \
  src/Mac/Metal/metal_states.cpp src/Mac/Metal/metal_states_gpu.mm \
  "$out/metal_core.o" "$out/metal_gpu.o" -framework Foundation -framework Metal -framework OpenGL -o "$out/gpu-test"
set +e
"$out/gpu-test" "$@"
result=$?
set -e
if [ "$result" -eq 77 ]; then exit 0; fi
exit "$result"
