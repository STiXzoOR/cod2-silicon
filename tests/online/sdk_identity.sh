#!/bin/sh
# Include the linked legacy stubs: isolated helper tests cannot detect SDK shadowing.
set -eu
cd "$(dirname "$0")/../.."
work=$(mktemp -d "${TMPDIR:-/tmp}/ws14-sdk.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
carbon=src/stubs/carbon_stubs.c
if [ "$#" -gt 0 ]; then
    git show "$1:src/stubs/carbon_stubs.c" > "$work/carbon.c"
    carbon="$work/carbon.c"
fi
clang -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations \
    -DCOD2_X64=1 -DCOD2_CODX=1 -Isrc/PC/qcommon -Isrc/stubs \
    tests/cod2x/test_cod2x.c src/PC/qcommon/cod2x_identity.c \
    src/PC/qcommon/cod2x_protocol.c "$carbon" \
    -framework IOKit -framework CoreFoundation -framework OpenGL \
    -o "$work/identity"
"$work/identity"
