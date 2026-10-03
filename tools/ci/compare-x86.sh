#!/usr/bin/env bash
# Build two clean git snapshots with identical metadata; never patch engine code.
set -euo pipefail
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
    echo 'Requires x86_64 Linux with the README multilib dependencies.' >&2
    exit 2
fi
compiler=${COD2_CC:-gcc-14}
command -v "$compiler" >/dev/null || { echo "Missing GCC >=14 compiler: $compiler" >&2; exit 2; }
repo=$(git rev-parse --show-toplevel)
base=$(git rev-parse "${1:-410342a82da1b2e8286c290274aeb0f1731a6859}^{commit}")
head=$(git rev-parse "${2:-HEAD}^{commit}")
out=${3:-"$repo/output/x86-reference"}
mkdir -p "$out"
out=$(cd "$out" && pwd)
# Refuse reuse: stale objects would weaken the comparison.
mkdir "$out/base" "$out/head"
export LC_ALL=C TZ=UTC
export SOURCE_DATE_EPOCH
SOURCE_DATE_EPOCH=$(git show -s --format=%ct "$base")
# Archives have no .git. Stop discovery before the enclosing worktree so both
# CMake invocations use the existing COD2_GIT_HASH="unknown" fallback.
export GIT_CEILING_DIRECTORIES="$out"
unset GIT_DIR GIT_WORK_TREE
umask 022
printf 'base=%s\nhead=%s\nepoch=%s\n' "$base" "$head" "$SOURCE_DATE_EPOCH" > "$out/inputs.txt"
"$compiler" --version >> "$out/inputs.txt"
cmake --version >> "$out/inputs.txt"
for name in base head; do
    rev=$base
    [[ $name != head ]] || rev=$head
    root="$out/$name"
    git -C "$repo" archive "$rev" | tar -x -C "$root"
    # Identical source mtimes also cover any future __TIMESTAMP__ use.
    find "$root" -type f -exec touch -d "@$SOURCE_DATE_EPOCH" {} +
    map="-ffile-prefix-map=$root=/cod2-reference -fdebug-prefix-map=$root=/cod2-reference"
    cmake -S "$root" -B "$root/build-verify" \
        -DCOD2_X64=OFF -DCMAKE_C_COMPILER="$compiler" -DCMAKE_ASM_COMPILER="$compiler" \
        -DCMAKE_C_FLAGS="-m32 $map" -DCMAKE_ASM_FLAGS="-m32 $map" \
        2>&1 | tee "$out/$name-configure.log"
    cmake --build "$root/build-verify" --parallel "${JOBS:-2}" \
        --target cod2_lnxded cod2_linux 2>&1 | tee "$out/$name-build.log"
done
python3 "$repo/tools/ci/compare-artifacts.py" "$out/base/build-verify" "$out/head/build-verify" \
    | tee "$out/comparison.txt"
