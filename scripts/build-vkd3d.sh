#!/bin/bash
# Build libvkd3d-shader, the HLSL compiler the shader generator links, from
# pinned, checksum-verified sources; no package manager needed.
#
# vkd3d's build needs GNU make 4.3+ and bison 3.x, and macOS ships make 3.81
# and bison 2.3, so both are built here first and used only for this build.
# The Vulkan and SPIR-V headers are needed to compile the library; nothing
# links Vulkan.
set -euo pipefail
cache=${1:?usage: build-vkd3d.sh build-directory}
mkdir -p "$cache"
cache=$(cd "$cache" && pwd -P)
prefix="$cache/install"
tools="$cache/tools"
fetch() {
    local url=$1 archive=$2 checksum=$3
    if [[ ! -f "$cache/$archive" ]]; then
        /usr/bin/curl --fail --location --retry 3 "$url" -o "$cache/$archive.part"
        mv "$cache/$archive.part" "$cache/$archive"
    fi
    if [[ $(/usr/bin/shasum -a 256 "$cache/$archive" | /usr/bin/awk '{print $1}') != "$checksum" ]]; then
        printf 'Checksum mismatch: %s. Remove it and retry.\n' "$cache/$archive" >&2
        exit 1
    fi
}
fetch https://dl.winehq.org/vkd3d/source/vkd3d-2.1.tar.xz vkd3d-2.1.tar.xz \
    7510146aff2adfb4ae07ab890701a607e5ff7c66e57100cfc9f630ec92eeda6a
fetch https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.xz bison-3.8.2.tar.xz \
    9bba0214ccf7f1079c5d59210045227bcf619519840ebfa80cd3849cff5a5bf2
fetch https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz make-4.4.1.tar.gz \
    dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
fetch https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz \
    Vulkan-Headers-vulkan-sdk-1.4.363.0.tar.gz 4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e
fetch https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz \
    SPIRV-Headers-vulkan-sdk-1.4.363.0.tar.gz 5b3a0d099f5c7649e29c0938476c462e71d66ad89a0bde42fa1ba3eb476956c9
stamp="vkd3d-2.1 bison-3.8.2 make-4.4.1 headers-1.4.363.0 arm64 macOS26 Release v2"
if [[ -f "$prefix/.complete" && $(cat "$prefix/.complete") == "$stamp" ]]; then
    exit 0
fi
rm -rf "$prefix" "$tools" "$cache/vkd3d-build"
for archive in vkd3d-2.1.tar.xz bison-3.8.2.tar.xz make-4.4.1.tar.gz \
    Vulkan-Headers-vulkan-sdk-1.4.363.0.tar.gz SPIRV-Headers-vulkan-sdk-1.4.363.0.tar.gz; do
    source=${archive%.tar.*}
    rm -rf "${cache:?}/$source"
    /usr/bin/tar -xf "$cache/$archive" -C "$cache"
done
# Full speed by default; COD2_BUILD_BACKGROUND=1 keeps builds on efficiency cores.
run() {
    if [[ ${COD2_BUILD_BACKGROUND:-0} == 1 ]]; then
        /usr/sbin/taskpolicy -b /usr/bin/nice -n 19 "$@"
    else
        "$@"
    fi
}
jobs=${COD2_BUILD_JOBS:-$(/usr/sbin/sysctl -n hw.ncpu)}
sdk=$(/usr/bin/xcrun --sdk macosx --show-sdk-path)
# Keep Homebrew and MacPorts out of the build: system tools plus the two built here.
export PATH="$tools/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export CC="/usr/bin/xcrun clang"
(cd "$cache/make-4.4.1" && run ./configure --prefix="$tools" --program-prefix=g --disable-nls --without-guile >"$cache/make-configure.log" 2>&1 &&
    run sh build.sh >"$cache/make-build.log" 2>&1 && run ./make install >>"$cache/make-build.log" 2>&1)
(cd "$cache/bison-3.8.2" && run ./configure --prefix="$tools" --disable-nls >"$cache/bison-configure.log" 2>&1 &&
    run gmake -j"$jobs" >"$cache/bison-build.log" 2>&1 && run gmake install >>"$cache/bison-build.log" 2>&1)
mkdir -p "$tools/include"
# vkd3d's configure insists on pkg-config, which macOS lacks. The shader
# library has no optional dependency worth finding, so answer "not installed".
printf '%s\n' '#!/bin/sh' 'case "$1" in --atleast-pkgconfig-version) exit 0 ;; --version) echo 0.29.2; exit 0 ;; esac' 'exit 1' > "$tools/bin/pkg-config"
chmod +x "$tools/bin/pkg-config"
cp -R "$cache/Vulkan-Headers-vulkan-sdk-1.4.363.0/include/vulkan" "$cache/Vulkan-Headers-vulkan-sdk-1.4.363.0/include/vk_video" "$tools/include/"
cp -R "$cache/SPIRV-Headers-vulkan-sdk-1.4.363.0/include/spirv" "$tools/include/"
mkdir -p "$cache/vkd3d-build"
(cd "$cache/vkd3d-build" && run "$cache/vkd3d-2.1/configure" --prefix="$prefix" \
        --disable-tests --disable-demos --disable-doxygen-doc --disable-static \
        BISON="$tools/bin/bison" SONAME_LIBVULKAN=libvulkan.1.dylib \
        CPPFLAGS="-I$tools/include" CFLAGS="-O2 -arch arm64 -mmacosx-version-min=26.0 -isysroot $sdk" \
        LDFLAGS="-arch arm64 -mmacosx-version-min=26.0 -isysroot $sdk" >"$cache/vkd3d-configure.log" 2>&1 &&
    run gmake include/private/vkd3d_version.h >"$cache/vkd3d-build.log" 2>&1 &&
    run gmake -j"$jobs" libvkd3d-shader.la >>"$cache/vkd3d-build.log" 2>&1)
mkdir -p "$prefix/lib" "$prefix/include" "$prefix/licenses"
cp "$cache/vkd3d-build/.libs/libvkd3d-shader.1.dylib" "$prefix/lib/"
ln -sf libvkd3d-shader.1.dylib "$prefix/lib/libvkd3d-shader.dylib"
/usr/bin/install_name_tool -id @rpath/libvkd3d-shader.1.dylib "$prefix/lib/libvkd3d-shader.1.dylib"
/usr/bin/codesign --force --sign - "$prefix/lib/libvkd3d-shader.1.dylib"
cp "$cache/vkd3d-2.1/include/vkd3d_shader.h" "$cache/vkd3d-2.1/include/vkd3d_types.h" "$prefix/include/"
cp "$cache/vkd3d-2.1/COPYING" "$prefix/licenses/vkd3d-COPYING.txt"
cp "$cache/vkd3d-2.1/LICENSE" "$prefix/licenses/vkd3d-LICENSE.txt"
cp "$cache/vkd3d-2.1/AUTHORS" "$prefix/licenses/vkd3d-AUTHORS.txt"
printf '%s\n' "$stamp" > "$prefix/.complete"
