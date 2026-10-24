#!/bin/bash
# Recover a completed /rootfs stage left by a failed classic Docker build.
# Run inside the oracle guest. The cod2engine source/Dockerfile is untouched.
set -euo pipefail
test "$#" = 1 || { echo 'Usage: finish-toolchain.sh BOOTSTRAP_IMAGE' >&2; exit 2; }
bootstrap=$1
image=cod2engine-linux-breezy-1.3
docker run --rm --platform linux/386 --cpuset-cpus=0,1 "$bootstrap" \
    chroot /rootfs dpkg-query -W gcc-3.3 g++-3.3 gcc-3.4 binutils libc6-dev
docker run --rm --platform linux/386 --cpuset-cpus=0,1 "$bootstrap" \
    tar -C /rootfs -cf - . | docker import --platform linux/386 - "$image"
test "$(docker image inspect --format '{{.Os}}/{{.Architecture}}' "$image")" = linux/386
docker run --rm --platform linux/386 --cpuset-cpus=0,1 "$image" sh -ec '
    test "$(dpkg-query -W -f="\${Version}" gcc-3.3)" = 1:3.3.6-8ubuntu1
    test "$(dpkg-query -W -f="\${Version}" g++-3.3)" = 1:3.3.6-8ubuntu1
    test "$(dpkg-query -W -f="\${Version}" gcc-3.4)" = 3.4.4-6ubuntu8
    test "$(dpkg-query -W -f="\${Version}" binutils)" = 2.16.1-2ubuntu6
    test "$(dpkg-query -W -f="\${Version}" libc6-dev)" = 2.3.5-1ubuntu12.5.10.1
    uname -m
'
