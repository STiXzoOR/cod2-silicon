#!/bin/sh
# Binary adapter for tools/parity/record.py, run INSIDE the ARM64 Linux guest.
# Every input/output path passed to the server must be under these bind roots.
set -eu
: "${ORACLE_BINARY:?Set the private instrumented cod2engine binary path}"
: "${ORACLE_DATA:?Set the guest licensed data root}"
: "${ORACLE_RUN_ROOT:?Set the guest private evidence root}"
binary=$(realpath "$ORACLE_BINARY")
data=$(realpath "$ORACLE_DATA")
run_root=$(realpath "$ORACLE_RUN_ROOT")
test -x "$binary"
exec docker run --rm --platform linux/386 --cpuset-cpus=0,1 --network host \
    -u "$(id -u):$(id -g)" \
    -v "$(dirname "$binary"):/oracle-server:ro" \
    -v "$data:/oracle-data:ro" --tmpfs "$data:rw,size=1m" \
    -v "$run_root:$run_root" -w "$PWD" \
    -e SYSDIFF_SEED -e SYSDIFF_MAXFRAMES -e SYSDIFF_STATEHASH -e SYSDIFF_DUMP \
    -e SYSDIFF_PLAYERS -e SYSDIFF_USERCMDS -e SYSDIFF_REPLAY \
    "${ORACLE_IMAGE:-cod2engine-linux-breezy-1.3}" sh -ec '
        # Legacy readdir narrows the 64-bit directory offsets returned by the
        # arm64 kernel through QEMU. A tmpfs directory has small offsets; its
        # symlinks still read the original, read-only IWDs without copying data.
        data=$1
        shift
        mkdir "$data/main"
        for file in /oracle-data/main/*.iwd; do
            ln -s "$file" "$data/main/"
        done
        exec "$@"
    ' oracle "$data" "/oracle-server/$(basename "$binary")" "$@"
