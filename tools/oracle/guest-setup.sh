#!/bin/sh
# Run INSIDE the Debian arm64 oracle VM, as root.
set -eu
test "$(id -u)" = 0 || { echo 'Run with sudo inside cod2-oracle' >&2; exit 1; }
test "$(dpkg --print-architecture)" = arm64 || { echo 'Expected Debian arm64 guest' >&2; exit 1; }
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    ca-certificates docker.io docker-cli docker-buildx qemu-user qemu-user-binfmt binfmt-support \
    make python3 git rsync binutils-i686-linux-gnu qemu-guest-agent unifdef
systemctl enable --now docker
systemctl restart systemd-binfmt
systemctl enable --now qemu-guest-agent || true
test -r /proc/sys/fs/binfmt_misc/qemu-i386
cat /proc/sys/fs/binfmt_misc/qemu-i386
grep -q '^flags:.*F' /proc/sys/fs/binfmt_misc/qemu-i386 || {
    echo 'i386 binfmt must have fix_binary (F), required across Docker mount namespaces' >&2
    exit 1
}
docker run --rm --platform linux/386 --cpuset-cpus=0,1 debian:trixie uname -m
mkdir -p /opt/cod2-oracle/bin
for tool in as ld objcopy objdump readelf nm; do
    ln -sf "/usr/bin/i686-linux-gnu-$tool" "/opt/cod2-oracle/bin/$tool"
done
printf '%s\n' 'Guest ready. Use sudo for Docker; prepend /opt/cod2-oracle/bin to PATH for make verify.'
