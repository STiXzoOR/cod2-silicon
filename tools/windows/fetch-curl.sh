#!/bin/sh
# Download the curl source, once, and build a static libcurl for one MinGW target.
#   tools/windows/fetch-curl.sh DEST_DIR TOOLCHAIN_FILE
# DEST_DIR ends up with install/lib/libcurl.a. HTTP and HTTPS only, with Windows'
# own TLS (Schannel), so the game needs no extra DLL.
#
# The checksum below was checked on 2026-11-10 against the release's detached
# signature (curl-8.14.1.tar.gz.asc) from the curl release key,
# 27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2, Daniel Stenberg.
set -eu
VERSION=8.14.1
SHA256=6766ada7101d292b42b8b15681120acd68effa4a9660935853cf6d61f0d984d4
ARCHIVE="curl-${VERSION}.tar.gz"
URL="https://curl.se/download/${ARCHIVE}"

dest=$1
toolchain=$2
if [ -f "$dest/.complete-$VERSION" ]; then
    exit 0
fi
rm -rf "$dest"
mkdir -p "$dest"
curl -fsSL -o "$dest/$ARCHIVE" "$URL"
echo "$SHA256  $dest/$ARCHIVE" | sha256sum -c - >/dev/null
tar -xzf "$dest/$ARCHIVE" -C "$dest"
rm -f "$dest/$ARCHIVE"
cmake -S "$dest/curl-$VERSION" -B "$dest/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$dest/install" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF \
    -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF \
    -DHTTP_ONLY=ON -DCURL_USE_SCHANNEL=ON -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF \
    -DUSE_LIBIDN2=OFF -DUSE_NGHTTP2=OFF -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF \
    >"$dest/configure.log" 2>&1
cmake --build "$dest/build" --parallel >"$dest/build.log" 2>&1
cmake --install "$dest/build" >>"$dest/build.log" 2>&1
test -f "$dest/install/lib/libcurl.a"
touch "$dest/.complete-$VERSION"
