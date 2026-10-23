#!/bin/bash
# Run inside the VM against its PRIVATE cod2engine checkout, never the host clone.
# Usage: sudo bash rebuild-reference.sh /opt/cod2engine /opt/cod2_lnxded /opt/oracle-evidence
set -eu
if [ "$#" -ne 3 ]; then
    echo 'Usage: rebuild-reference.sh PRIVATE_COD2ENGINE ORIGINAL_BINARY EVIDENCE_DIR' >&2
    exit 2
fi
source_root=$(realpath "$1")
anchor=$(realpath "$2")
mkdir -p "$3"
evidence=$(realpath "$3")
oracle_tools=$(dirname "$(realpath "$0")")
expected=05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7
actual=$(sha256sum "$anchor" | cut -d ' ' -f 1)
printf 'Original SHA256 %s\n' "$actual" | tee "$evidence/original-sha256.txt"
if [ "$actual" != "$expected" ]; then
    echo 'Unexpected original binary: refusing to compare the wrong server' >&2
    exit 1
fi
export PATH="/opt/cod2-oracle/bin:$PATH"
export ANCHOR="$anchor"
# The classic builder mislabels the final FROM scratch stage on arm64.
# BuildKit preserves the requested linux/386 platform without source changes.
export DOCKER_BUILDKIT=1
docker buildx version > "$evidence/buildx-version.txt"
cd "$source_root/archive/lnxded-1.3"
# nproc in the outer Makefile runs on the guest. Restrict affinity so its
# hard-coded -j$(nproc) resolves to two, and restrict each build container too.
printf -v work_arg '%q' "$PWD:/work"
docker_run="docker run --rm --platform linux/386 --cpuset-cpus=0,1 -u $(id -u):$(id -g) -v $work_arg -w /work cod2engine-linux-breezy-1.3"
run_logged() {
    local name=$1
    shift
    set +e
    taskset -c 0,1 nice -n 19 "$@" >"$evidence/$name.log" 2>&1
    local status=$?
    set -e
    printf '%s exit=%d\n' "$name" "$status" | tee -a "$evidence/rebuild-status.txt"
    tail -n 12 "$evidence/$name.log"
    return 0
}
: > "$evidence/rebuild-status.txt"
uname -a > "$evidence/guest-uname.txt"
docker version > "$evidence/docker-version.txt"
cat /proc/sys/fs/binfmt_misc/qemu-i386 > "$evidence/binfmt-i386.txt"
if [ -n "${ORACLE_BOOTSTRAP_IMAGE:-}" ]; then
    run_logged toolchain bash "$oracle_tools/finish-toolchain.sh" "$ORACLE_BOOTSTRAP_IMAGE"
else
    run_logged toolchain make toolchain
fi
run_logged check make check "DOCKER=$docker_run"
run_logged verify make verify "DOCKER=$docker_run"
run_logged coverage python3 tools/tu_cov.py
if [ -f build/cod2_lnxded_1_3 ]; then
    sha256sum build/cod2_lnxded_1_3 > "$evidence/rebuilt-sha256.txt"
    cmp "$anchor" build/cod2_lnxded_1_3 > "$evidence/binary-cmp.txt" 2>&1 || true
fi
python3 - "$evidence" <<'PY'
import glob, json, pathlib, re, sys
paths = glob.glob('build/verify/*.verify')
passed = 0
failed = []
for name in paths:
    lines = pathlib.Path(name).read_text().splitlines()
    if lines and lines[-1] == 'TU VERIFIED':
        passed += 1
    else:
        failed.append(name)
sources = pathlib.Path('Makefile').read_text().split('SRCS :=', 1)[1].split('OBJS', 1)[0]
total = len(re.findall(r'^\s+\S+\.c(?:pp)?(?:\s*\\)?\s*$', sources, re.MULTILINE))
summary = {'verified': passed, 'total': total, 'attempted': len(paths), 'failed': failed}
pathlib.Path(sys.argv[1], 'tu-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary))
PY
# A failed build/verification remains visible in the status file and exit code.
if grep -q 'exit=[1-9]' "$evidence/rebuild-status.txt"; then exit 1; fi
