#!/bin/sh
# No execution of i386 code is required. All outputs remain in this worktree.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$repo"
binary=${1:-${COD2_STABS_BINARY:-$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386}}
out=${2:-build/x64_gen}
compiler=${CLANG:-clang}
python=${PYTHON:-python3}
if [ ! -f "$binary" ]; then
    echo "Missing STABS reference binary: $binary" >&2
    exit 1
fi
if [ -n "${OBJDUMP:-}" ]; then
    objdump=$OBJDUMP
elif command -v xcrun >/dev/null 2>&1; then
    objdump=$(xcrun --find llvm-objdump)
elif command -v llvm-objdump >/dev/null 2>&1; then
    objdump=$(command -v llvm-objdump)
else
    echo 'Missing llvm-objdump; no packages will be installed.' >&2
    exit 1
fi
"$python" -m unittest discover -s tools/datagen -v
"$python" tools/datagen/generate.py --binary "$binary" --output "$out" --clang "$compiler"
for group in data literals import_pointers; do
    "$compiler" -target i386-unknown-linux-gnu -ffreestanding -std=c11 -c "$out/$group.c" -o "$out/generated-$group.o"
    "$objdump" -s -r "$out/original-$group.o" > "$out/original-$group.objdump"
    "$objdump" -s -r "$out/generated-$group.o" > "$out/generated-$group.objdump"
    "$python" tools/datagen/compare.py "$out/original-$group.o" "$out/generated-$group.o"
    "$compiler" -target arm64-apple-macos -ffreestanding -std=c11 -c "$out/$group.c" -o "$out/arm64-$group.o"
    "$compiler" -target arm64-apple-macos -ffreestanding -std=c11 -c "$out/${group}_native.c" -o "$out/arm64-${group}_native.o"
    "$compiler" -target i386-unknown-linux-gnu -ffreestanding -std=c11 -c "$out/${group}_native.c" -o "$out/generated-${group}_native.o"
    "$objdump" -s -r "$out/reference-native-$group.o" > "$out/reference-native-$group.objdump"
    "$objdump" -s -r "$out/generated-${group}_native.o" > "$out/generated-${group}_native.objdump"
    "$python" tools/datagen/compare.py "$out/reference-native-$group.o" "$out/generated-${group}_native.o"
done
for group in data literals; do
    "$compiler" -target arm64-apple-macos -ffreestanding -std=c11 -c "$out/reference-native-$group.c" -o "$out/arm64-reference-native-$group.o"
done
"$python" tools/datagen/arm64_test.py "$out"
"$compiler" -target i386-unknown-linux-gnu -ffreestanding -std=c11 -c "$out/bss_probe.c" -o "$out/i386-bss_probe.o"
"$compiler" -target arm64-apple-macos -ffreestanding -std=c11 -c "$out/bss_probe.c" -o "$out/arm64-bss_probe.o"
# A second generation must leave all source/coverage outputs byte-identical.
"$python" tools/datagen/determinism.py "$binary" "$out" "$compiler"
echo 'PASS: i386 bytes/relocations/symbols, arm64 full/native and BSS probes, LP64 address points, determinism'
