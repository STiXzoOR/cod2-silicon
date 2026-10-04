#!/bin/bash
# Build only pinned, checksum-verified SDL sources; no package manager needed.
set -euo pipefail
cache=${1:?usage: build-sdl.sh build-directory}
mkdir -p "$cache"
cache=$(cd "$cache" && pwd -P)
prefix="$cache/install"
fetch() {
    local archive=$1 checksum=$2
    if [[ ! -f "$cache/$archive" ]]; then
        /usr/bin/curl --fail --location --retry 3 "https://www.libsdl.org/release/$archive" -o "$cache/$archive.part"
        mv "$cache/$archive.part" "$cache/$archive"
    fi
    if [[ $(/usr/bin/shasum -a 256 "$cache/$archive" | /usr/bin/awk '{print $1}') != "$checksum" ]]; then
        printf 'Checksum mismatch: %s. Remove it and retry.\n' "$cache/$archive" >&2
        exit 1
    fi
}
fetch SDL3-3.4.10.tar.gz 12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785
fetch sdl2-compat-2.32.72.tar.gz a14d2f78dad8e83ef1039b6534ace4d14f11f5b11d023af989affd70ac1bb35e
stamp='SDL3-3.4.10 sdl2-compat-2.32.72 arm64 macOS13 Release v1'
if [[ -f "$prefix/.complete" && $(cat "$prefix/.complete") == "$stamp" ]]; then
    exit 0
fi
for source in SDL3-3.4.10 sdl2-compat-2.32.72; do
    if [[ ! -d "$cache/$source" ]]; then
        /usr/bin/tar -xzf "$cache/$source.tar.gz" -C "$cache"
    fi
done
run() { /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 "$@"; }
common=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_EXPORT_PACKAGE_REGISTRY=OFF "-DCMAKE_INSTALL_PREFIX=$prefix"
    "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local")
run cmake -S "$cache/SDL3-3.4.10" -B "$cache/sdl3-build" "${common[@]}" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_FRAMEWORK=OFF
run cmake --build "$cache/sdl3-build" --parallel 4
run cmake --install "$cache/sdl3-build"
run cmake -S "$cache/sdl2-compat-2.32.72" -B "$cache/sdl2-build" "${common[@]}" \
    "-DCMAKE_PREFIX_PATH=$prefix" -DSDL2COMPAT_TESTS=OFF -DSDL2COMPAT_INSTALL=ON -DSDL2COMPAT_FRAMEWORK=OFF
run cmake --build "$cache/sdl2-build" --parallel 4
run cmake --install "$cache/sdl2-build"
mkdir -p "$prefix/licenses"
cp "$cache/SDL3-3.4.10/LICENSE.txt" "$prefix/licenses/SDL3-LICENSE.txt"
cp "$cache/sdl2-compat-2.32.72/LICENSE.txt" "$prefix/licenses/sdl2-compat-LICENSE.txt"
printf '%s\n' "$stamp" > "$prefix/.complete"
