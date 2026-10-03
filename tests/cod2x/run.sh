#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
work=$(mktemp -d "$PWD/tests/cod2x/.run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
clang -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
    -DCOD2_CODX=1 -Isrc/PC/qcommon tests/cod2x/test_cod2x.c \
    src/PC/qcommon/cod2x_identity.c src/PC/qcommon/cod2x_protocol.c \
    -framework IOKit -framework CoreFoundation -o "$work/test_cod2x"
"$work/test_cod2x"
