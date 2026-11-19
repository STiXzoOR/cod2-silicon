#!/bin/sh
# Download the official SDL2 MinGW development package, once, and unpack it.
#   tools/windows/fetch-sdl2.sh DEST_DIR
# DEST_DIR ends up with i686-w64-mingw32/ and x86_64-w64-mingw32/.
set -eu
VERSION=2.30.9
SHA256=b188d165dc4a0372042bff3d0c18e9bb90772581e92a7b8b47b521b47714f75c
ARCHIVE="SDL2-devel-${VERSION}-mingw.tar.gz"
URL="https://github.com/libsdl-org/SDL/releases/download/release-${VERSION}/${ARCHIVE}"

dest=$1
if [ -f "$dest/.complete-$VERSION" ]; then
    exit 0
fi
mkdir -p "$dest"
curl -fsSL -o "$dest/$ARCHIVE" "$URL"
echo "$SHA256  $dest/$ARCHIVE" | sha256sum -c - >/dev/null
tar -xzf "$dest/$ARCHIVE" -C "$dest" --strip-components=1
rm -f "$dest/$ARCHIVE"
touch "$dest/.complete-$VERSION"
