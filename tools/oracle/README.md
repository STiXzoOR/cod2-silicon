# Server replay oracle

Command recording/replay and raw player fields extend the existing
[`tools/parity`](../parity) tools. **The Linux reference path is prepared but
unvalidated:** macOS Automation and private SSH-key preparation blocked VM
creation. See the [F0 report](../../docs/macos-port/reports/2026-10-23-F0-server-replay-oracle.md).
Native validation alone does not establish reference parity.

Keep downloads, VM inputs, reference patches, licensed scripts, streams and
captures outside Git. This unit uses:

```sh
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
```

## VM and stock rebuild

UTM 4.7.5 is installed. Allow **System Settings → Privacy & Security →
Automation → Warp → UTM**, or approve “Warp wants to control UTM”; open UTM
from Applications. Read-only `utmctl list` must work before creation. Another
launch context may show ChatGPT/Codex instead of Warp.

The verified official Debian 13 arm64 image is in `$evidence/downloads`, with
a resized 36 GiB copy at `$evidence/debian-13-oracle-arm64.qcow2`. For a new
machine, download `debian-13-genericcloud-arm64.qcow2` and `SHA512SUMS` from the
[official cloud directory](https://cloud.debian.org/images/cloud/trixie/latest/),
verify the checksum and resize a private copy to 36 GiB using an existing
qemu-img. This machine's task-private launcher for UTM's bundled qemu-img is
`$evidence/qemu-img`; its source is beside it. No host installation is needed.

The user must manually prepare a task-private SSH key. Automatic approval
review rejected this agent's key generation under
`secret.pattern.ssh-key-basename`; no retry was attempted. Put its public key
in `$evidence/cloud-init/user-data.template` in place of
`REPLACE_WITH_PUBLIC_SSH_KEY`, save as `user-data`, and keep the private key
outside Git. The template creates `oracle`, disables password/root SSH and
grants sudo inside this disposable guest. Build the NoCloud seed and create
the one VM:

```sh
hdiutil makehybrid -iso -joliet -default-volume-name cidata \
  -o "$evidence/cloud-init.iso" "$evidence/cloud-init"
vm_id=$(osascript tools/oracle/vm.applescript \
  "$evidence/debian-13-oracle-arm64.qcow2" "$evidence/cloud-init.iso")
/Applications/UTM.app/Contents/MacOS/utmctl start "$vm_id"
ssh -p 22220 -i "$evidence/oracle_ed25519" \
  -o UserKnownHostsFile="$evidence/known_hosts" oracle@127.0.0.1
```

The AppleScript specifies 4 CPUs, 8 GiB RAM, 36 GiB disk, virtualization and
localhost:22220 → guest:22. It returns an existing `cod2-oracle` unchanged;
it refuses duplicate names. Creation/boot remain untested.

Copy repo source/tools to `/opt/cod2-silicon` in the guest, the read-only host
cod2engine clone to `/opt/cod2engine`, the original server to
`/opt/cod2_lnxded`, and licensed `main/*.iwd` to `/opt/cod2-data/main`, using
SSH/scp/rsync. First create guest directories owned by `oracle`. Exclude
`.git`, builds/output and unrelated data. Inside the guest:

```sh
sudo sh /opt/cod2-silicon/tools/oracle/guest-setup.sh
# Must report i386/i686; qemu-i386 binfmt must have the F flag:
sudo docker run --rm --platform linux/386 --cpuset-cpus=0,1 debian:trixie uname -m
sudo bash /opt/cod2-silicon/tools/oracle/rebuild-reference.sh \
  /opt/cod2engine /opt/cod2_lnxded /opt/oracle-evidence/rebuild
```

The rebuild script checks the anchor SHA256 against
`05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`,
runs `make toolchain`, `make check`, `make verify` and `python3 tools/tu_cov.py`,
and records each exit status plus verified/total/attempted TUs. Make and its
containers use two CPUs. `make check` tests the rebuilt SHA; `cmp` compares
against the anchor. Failures remain visible; inspect `.verify`, coverage and
binary-cmp logs before using a closest build. F0 has no rebuild result yet.

## Private reference instrumentation

Run inside the guest. Keep the new directory under `archive/` for the relative
toolchain path. The generator reuses this repo's independently written hook;
its private patch contains reference context and must never enter Git.

```sh
cd /opt/cod2-silicon
python3 tools/oracle/instrument_reference.py \
  --source /opt/cod2engine/archive/lnxded-1.3 \
  --destination /opt/cod2engine/archive/lnxded-1.3-oracle \
  --patch /opt/oracle-evidence/reference-instrumentation.patch
cd /opt/cod2engine/archive/lnxded-1.3-oracle
sudo docker run --rm --platform linux/386 --cpuset-cpus=0,1 \
  -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  cod2engine-linux-breezy-1.3 make -j2 IN_IMAGE=1 build/cod2_lnxded_1_3
sha256sum build/cod2_lnxded_1_3 > /opt/oracle-evidence/instrumented-sha256.txt
```

Instrumentation necessarily changes the executable: only the stock copy should
match the original SHA. Generation worked against a private host copy;
GCC 3.3 compilation/runtime remain unverified. COD2_X64 is enabled only when
extracting our hook text, never for reference engine code.

## Record, replay twice, compare

Inside the guest, the adapter bind-mounts data read-only and the private run
root writable, at identical absolute paths. Invoke the outer recorder with
sudo so the adapter can use Docker:

```sh
cd /opt/cod2-silicon
export ORACLE_BINARY=/opt/cod2engine/archive/lnxded-1.3-oracle/build/cod2_lnxded_1_3
export ORACLE_DATA=/opt/cod2-data ORACLE_RUN_ROOT=/opt/oracle-evidence
sudo --preserve-env=ORACLE_BINARY,ORACLE_DATA,ORACLE_RUN_ROOT \
  nice -n 19 python3 tools/parity/record.py \
  --binary tools/oracle/container_server.sh --data "$ORACLE_DATA" \
  --output "$ORACLE_RUN_ROOT/original-record" --dedicated --players --clients 2 \
  --commands-record --frames 2000 --port 29220 --timeout 300
```

`run.json` hashes the adapter; preserve `instrumented-sha256.txt` for the actual
reference server identity. Repeat twice using new outputs `original-a` and
`original-b`, unique ports, and
`--replay /opt/oracle-evidence/original-record/usercmds.txt` instead of
`--commands-record`. Copy the command stream and traces privately to the host.
Build the stock native dedicated target and replay twice with new outputs and
ports:

```sh
taskpolicy -b nice -n 19 cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos_ded --parallel 2
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
  --binary build-macos/cod2_macos_ded --data "$HOME/Games/CoD2" \
  --output "$evidence/native-a" --dedicated --players --clients 2 \
  --replay "$evidence/original-record/usercmds.txt" \
  --frames 2000 --port 29223 --timeout 300
python3 tools/parity/compare.py "$evidence/original-a/statehash.txt" \
  "$evidence/native-a/statehash.txt" --players "$evidence/original-a/players.txt" \
  "$evidence/native-a/players.txt" --require-clients 2
```

First compare original-a/original-b, then native-a/native-b, then cross-build.
Same-build divergence invalidates a cross-build diagnosis. The recorder uses
fresh homepaths, mp_toujane DM, seed 12345, 20 Hz and fixedtime 50. Keep the
IWD sets identical. Dedicated runs need no GUI lease; GUI runs still do.

With `--clients`, the recorder reads the latest installed DM script and enables
its dormant test-client helper under that run's private homepath. It sets
`scr_testclients` and `sv_pure 0`. The helper adds clients after five simulation
seconds and selects teams/weapons. Check actual health, weapon and pm_type:
F0's native clients remained spectators, limiting gameplay coverage.

## Format and limits

`statehash.txt` retains the existing format and normalized q CRC. `players.txt`
has `frame F clients N`, then N ascending client records with origin, velocity,
viewangles, pm_type, pm_flags, pm_time, weapon, weaponstate and health
(`ps.stats[0]`). Vector components are raw binary32 hex bits; pm_flags is hex,
other scalars decimal. Active means server state 4, in-use entity and game
client. Warmup frames may have zero clients. No script VM/RNG digest is added.

`usercmds.txt` starts `cod2-replay 1`; each numeric row is
`cmd FRAME CLIENT TIME_OFFSET BUTTONS WEAPON OFFHAND ANGLE0 ANGLE1 ANGLE2 FORWARD RIGHT`;
`end FINAL_FRAME` terminates it. Times are relative to `level.startTime`; frame
is `level.framenum + 1` at SV_ClientThink. No struct padding or pointers enter
the stream. Multiple commands/client/frame retain order. Replay rejects
truncation, missed frames and inactive slots, suppresses live network commands
and injects before the corresponding simulation tick. Record/replay both
suppress stock bot random input and its RNG consumption. Recording generates
turning/movement/attack input for test clients each tick through SV_ClientThink;
ordinary network usercmds are recorded at that entry too. Connection, disconnect
and reliable client commands are outside this format. One map per process.

New switches are `SYSDIFF_PLAYERS`, `SYSDIFF_USERCMDS` (record) and
`SYSDIFF_REPLAY`, alongside existing statehash/seed/maxframes/dump switches.
Absent switches leave hooks inactive; new engine code is COD2_X64-gated.

The comparer chooses the earliest frame across traces and prefers an exact
player field over a hash at that frame. Adjacent finite same-sign float bits
are labelled `float-last-bit`, other field differences `logic`, and invalid
framing `harness`. Labels describe observations, not proven causes. A q-only
difference needs the existing entity-dump workflow. Hash quantization,
collisions and omitted script/RNG state remain limits.

## x87 probe and synthetic validation

Inside the same guest/toolchain container:

```sh
docker run --rm --platform linux/386 --cpuset-cpus=0,1 \
  -v /opt/cod2-silicon/tools/oracle:/oracle:ro -v /opt/oracle-evidence:/evidence \
  -w /evidence cod2engine-linux-breezy-1.3 sh -c \
  'gcc-3.3 -std=c99 -O0 -fno-builtin -mfpmath=387 /oracle/x87_probe.c -lm -o x87_probe && ./x87_probe' \
  > /opt/oracle-evidence/x87-probe.log
```

It prints sin/cos/atan2/sqrt/pow as binary64 and rounded binary32 bits plus the
x87 control word. Save QEMU/compiler versions too. This is an emulator baseline,
not proof of hardware transcendental equality.

Without licensed data or a VM:

```sh
python3 -m unittest discover -s tools/tests -p test_parity_oracle.py -v
taskpolicy -b nice -n 19 python3 tools/oracle/test_hooks.py \
  --compile-db build-macos/compile_commands.json --output "$evidence/hooks-test-new"
```

The C fixture runs production hooks with ASan/UBSan in a synthetic world. It
tests injection, player bits, relative command times and rejection of truncated
streams/inactive targets. It exercises no engine gameplay.
