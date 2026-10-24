# Server replay oracle

Command recording/replay and raw player fields extend the existing
[`tools/parity`](../parity) tools. **F0c validated the byte-identical Linux
rebuild and a live 2,000-frame reference corpus. Native parity remains open.**
Both reference replays match. After F1a integration and the held-grenade death
argument fix, two stock native replays complete 2,000 frames and match each
other. The first cross-build difference remains frame 4, active entities
168 versus 205, before any active client. Native combat outcomes also differ.
See the [grenade-death follow-up](../../docs/macos-port/reports/2026-10-24-F1-grenade-death.md)
for regression coverage and the limits of the completed native replays.
See the [F0 report](../../docs/macos-port/reports/2026-10-23-F0-server-replay-oracle.md)
for the entity triage and Mac-confirmed reversed grenade arguments.

`cod2-oracle` is present and stopped. Its bundle occupies 6,774,404 KiB
(about 6.46 GiB) after F0c. The commands below use the existing VM and agent
authentication; they need no GUI game run.

Keep downloads, VM inputs, reference patches, licensed scripts, streams and
captures outside Git. This unit uses:

```sh
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
```

## VM and stock rebuild

UTM 4.7.5 automation works. Existing VM UUID:
`0DDDB408-5D92-451D-8691-58B1ABD35D7D`. Do not create a second VM.
It has
4 vCPUs, 8 GiB RAM and a 36 GiB virtual system disk. It lives at
`~/Library/Containers/com.utmapp.UTM/Data/Documents/cod2-oracle.utm`.

For a fresh setup, use the official **generic** Debian 13 arm64 image from
[Debian's cloud directory](https://cloud.debian.org/images/cloud/trixie/latest/),
verify it against `SHA512SUMS`, then resize a private copy to 36 GiB using
an existing qemu-img. F0b verified `debian-13-generic-arm64.qcow2` and
prepared `$evidence/debian-13-oracle-generic.qcow2`. This machine's private
launcher for UTM's bundled qemu-img is `$evidence/qemu-img`.
The earlier genericcloud/USB-seed combination booted without cloud-init;
F0b's generic image and fixed VirtIO seed completed `DataSourceNoCloud`.
The installed VM has the seed as `/dev/vda`, system disk as `/dev/vdb`.

The user prepared `cloud-init/user-data` and loaded the task key into the
SSH agent. Use agent authentication only. Do not generate or handle keys.
On authentication failure, stop and report; do not try another access path.

```sh
# Fresh creation only; existing VM is returned unchanged:
hdiutil makehybrid -iso -joliet -default-volume-name cidata \
  -o "$evidence/cloud-init.iso" "$evidence/cloud-init"
vm_id=$(osascript tools/oracle/vm.applescript \
  "$evidence/debian-13-oracle-generic.qcow2" "$evidence/cloud-init.iso")
/Applications/UTM.app/Contents/MacOS/utmctl start "$vm_id"
ssh -p 22220 -o UserKnownHostsFile="$evidence/known_hosts" oracle@127.0.0.1
```

Allow about 20 seconds for first boot. For a new host key, verify/accept it
in the task's `known_hosts`. F0b accepted the guest key with
`-o StrictHostKeyChecking=accept-new`, then received the authentication failure.
F0c inherits working agent authentication and a populated guest. Use
`-o BatchMode=yes` for unattended SSH/scp; keep its host key file private.

The AppleScript specifies virtualization, fixed VirtIO system/seed disks and
localhost:22220 → guest:22. Explicit AppleEvent codes disambiguate UTM's
serial-port property, directory-share mode and TCP enumeration. Existing-VM
lookup is tested; the final fixed-seed fresh-creation variant is syntax
checked, while equivalent drive settings were applied to the existing VM.

Copy repo source/tools to `/opt/cod2-silicon` in the guest, the read-only host
cod2engine clone to `/opt/cod2engine`, the original server to
`/opt/cod2-data/cod2_lnxded`, and licensed `main/*.iwd` to `/opt/cod2-data/main`, using
SSH/scp/rsync. First create guest directories owned by `oracle`. Exclude
`.git`, builds/output and unrelated data. Inside the guest:

```sh
sudo sh /opt/cod2-silicon/tools/oracle/guest-setup.sh
# Must report i386/i686; qemu-i386 binfmt must have the F flag:
sudo docker run --rm --platform linux/386 --cpuset-cpus=0,1 debian:trixie uname -m
sudo bash /opt/cod2-silicon/tools/oracle/rebuild-reference.sh \
  /opt/cod2engine /opt/cod2-data/cod2_lnxded /opt/oracle-evidence/rebuild-f0c
```

The rebuild script checks the anchor SHA256 against
`05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`,
runs `make toolchain`, `make check`, `make verify` and `python3 tools/tu_cov.py`,
and records each exit status plus verified/total/attempted TUs. Make and its
containers use two CPUs. `make check` tests the rebuilt SHA; `cmp` compares
against the anchor. Failures remain visible; inspect `.verify`, coverage and
binary-cmp logs before using a closest build. Debian 13 needs both `docker-cli`
and `docker-buildx`: `rebuild-reference.sh` enables `DOCKER_BUILDKIT=1` because
the classic builder mislabels its final `FROM scratch` stage as arm64.
The cod2engine sources and Dockerfile stay unchanged.

F0c recovered the complete `/rootfs` stage retained from the classic build,
rather than waiting for a stalled archive download. On this VM that stage is
`c2d5e9b85f07`. To reproduce that path, replace the rebuild invocation above:

```sh
sudo env ORACLE_BOOTSTRAP_IMAGE=c2d5e9b85f07 \
  bash /opt/cod2-silicon/tools/oracle/rebuild-reference.sh \
  /opt/cod2engine /opt/cod2-data/cod2_lnxded /opt/oracle-evidence/rebuild-f0c-recovered
```

`finish-toolchain.sh` imports only that stage's root filesystem with explicit
`linux/386` metadata and checks the compiler/binutils/glibc package versions.
It requires an already completed bootstrap stage; a fresh VM uses BuildKit.
The recovery log is a toolchain recovery result, not a successful rerun of
`make toolchain`. Both subsequent `make check` and `make verify` run normally.


## Private reference instrumentation

Run inside the guest. Keep the new directory under `archive/` for the relative
toolchain path. The generator reuses this repo's independently written hook;
its private patch contains reference context and must never enter Git.

```sh
cd /opt/cod2-silicon
python3 tools/oracle/instrument_reference.py \
  --source /opt/cod2engine/archive/lnxded-1.3 \
  --destination /opt/cod2engine/archive/lnxded-1.3-oracle-f0c \
  --patch /opt/oracle-evidence/reference-f0c.patch
cd /opt/cod2engine/archive/lnxded-1.3-oracle-f0c
sudo docker run --rm --platform linux/386 --cpuset-cpus=0,1 \
  -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  cod2engine-linux-breezy-1.3 nice -n 19 make -j2 IN_IMAGE=1 build/cod2_lnxded_1_3
sha256sum build/cod2_lnxded_1_3 > /opt/oracle-evidence/instrumented-sha256.txt
```

Instrumentation necessarily changes the executable: only the stock copy should
match the original SHA. COD2_X64 is enabled only when extracting our hook text,
never for reference engine code. `live_input.inc` replaces only our recording
workload in the generated copy; it changes no native engine source. It cycles
movement/jump, crouch, prone, attack/reload, a frag held then released, and use/attack
pulses for the stock DM respawn handler. Saved commands drive every replay.

## Record, replay twice, compare

Use fresh destination/output names when rerunning. Inside the guest, the
adapter presents the read-only IWDs through a 1 MiB tmpfs directory of symlinks.
This avoids `readdir` EOVERFLOW: the old 32-bit libc cannot narrow the 64-bit
directory cookies returned through QEMU on the guest filesystem. Asset bytes
still come from the read-only bind mount. The private run root is writable.

Run this guest sequence after the build above:

```sh
cd /opt/cod2-silicon
export ORACLE_BINARY=/opt/cod2engine/archive/lnxded-1.3-oracle-f0c/build/cod2_lnxded_1_3
export ORACLE_DATA=/opt/cod2-data ORACLE_RUN_ROOT=/opt/oracle-evidence
original_run() {
  sudo --preserve-env=ORACLE_BINARY,ORACLE_DATA,ORACLE_RUN_ROOT \
    nice -n 19 python3 tools/parity/record.py \
    --binary tools/oracle/container_server.sh --data "$ORACLE_DATA" \
    --dedicated --players --clients 2 --frames 2000 --timeout 360 "$@"
}
original_run --output "$ORACLE_RUN_ROOT/original-f0c-live-record" \
  --commands-record --port 29236
original_run --output "$ORACLE_RUN_ROOT/original-f0c-live-a" \
  --replay "$ORACLE_RUN_ROOT/original-f0c-live-record/usercmds.txt" --port 29231
original_run --output "$ORACLE_RUN_ROOT/original-f0c-live-b" \
  --replay "$ORACLE_RUN_ROOT/original-f0c-live-record/usercmds.txt" --port 29232
python3 tools/oracle/live_coverage.py \
  "$ORACLE_RUN_ROOT/original-f0c-live-record/players.txt" \
  "$ORACLE_RUN_ROOT/original-f0c-live-record/usercmds.txt" --require-all \
  > "$ORACLE_RUN_ROOT/live-coverage-f0c.json"
```

`run.json` hashes the adapter, so record the actual instrumented executable
identity separately when using another build. On the host, fetch the command
stream and traces, build this branch's stock native dedicated server, then
replay twice:

```sh
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
for run in original-f0c-live-record original-f0c-live-a original-f0c-live-b; do
  mkdir -p "$evidence/$run"
  scp -P 22220 -o BatchMode=yes -o UserKnownHostsFile="$evidence/known_hosts" \
    "oracle@127.0.0.1:/opt/oracle-evidence/$run/*.txt" \
    "oracle@127.0.0.1:/opt/oracle-evidence/$run/run.json" "$evidence/$run/"
done
taskpolicy -b nice -n 19 cmake -S . -B build-f0-stock -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-f0-stock --target cod2_macos_ded --parallel 2
for pair in a:29239 b:29240; do
  if taskpolicy -b nice -n 19 python3 tools/parity/record.py \
    --binary build-f0-stock/cod2_macos_ded --data "$HOME/Games/CoD2" \
    --output "$evidence/native-f0c-head-${pair%:*}" --port "${pair#*:}" \
    --dedicated --players --clients 2 --frames 2000 --timeout 360 \
    --replay "$evidence/original-f0c-live-record/usercmds.txt"; then
    printf 'native %s completed\n' "${pair%:*}"
  else
    replay_exit=$?
    printf 'native %s failed: recorder exit %s; inspect console.log\n' "${pair%:*}" "$replay_exit"
  fi
done
compare_runs() {
  python3 tools/parity/compare.py "$evidence/$1/statehash.txt" \
    "$evidence/$2/statehash.txt" --players "$evidence/$1/players.txt" \
    "$evidence/$2/players.txt" --require-clients 2
}
compare_runs original-f0c-live-a original-f0c-live-b
# A matching truncated prefix does not satisfy the 2000-frame requirement:
compare_runs native-f0c-head-a native-f0c-head-b
compare_runs original-f0c-live-a native-f0c-head-a
```

First compare each same-build pair, then cross-build.
Same-build divergence invalidates a cross-build diagnosis. At F0c the native
prefixes match for 370 frames, but both recorders fail on SIGSEGV; the native
repeatability gate remains incomplete. The cross-build comparison still names
the earlier frame-4 entity-count difference in these partial traces. The recorder uses
fresh homepaths, mp_toujane DM, seed 12345, 20 Hz and fixedtime 50. Keep the
IWD sets identical. Dedicated runs need no GUI lease; GUI runs still do.

With `--clients`, the recorder reads the latest installed DM and team scripts
and enables the dormant test-client helper under that run's private homepath.
It replaces the helper's selection entry point with an independently written
routine calling the gametype's normal team and weapon callbacks (Thompson for
allies, MP40 for axis), after 0.25/0.5-second waits. It sets `scr_testclients`
and `sv_pure 0`. No extracted game-script text is stored in Git.
F0b's native 300-frame smoke spawned both clients at frame 116, with health
100 and pm_type 0. The F0c result below records reference-side coverage.
After recording, check observed actions rather than the requested buttons:

```sh
python3 tools/oracle/live_coverage.py \
  /opt/oracle-evidence/original-f0c-live-record/players.txt \
  /opt/oracle-evidence/original-f0c-live-record/usercmds.txt --require-all \
  > /opt/oracle-evidence/live-coverage.json
```

The check requires positive-health normal players, changing live origins, a
jump command with the jump flag and upward velocity, crouch/prone flags, firing
and frag offhand states, a health drop/death, then positive health after death.
It reports the first frame/client and number of records for each. It does not
claim player-versus-player damage merely from self-inflicted grenade damage.

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

## F0c measured results before F1a and the grenade-death fix

| Check | Result |
| --- | --- |
| Stock rebuilt SHA / original | identical: `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` |
| TU verification | 157/157; `.text` coverage 99.96%, 434 bytes uncovered |
| Original recording / replay A / replay B | 2,000 frames each; comparisons pass |
| Live actions | move, jump, crouch, prone, fire, frag, fatal damage, death and respawn observed |
| Native HEAD A / B | both SIGSEGV after frame 373; emitted prefixes match for 370 frames |
| First cross-engine divergence | frame 4, no active client, field `ents`: 168 / 205 |
| x87 probe | ran in QEMU 10.0.13; control word 037f; full output in private evidence |
| VM | stopped, present; about 6.46 GiB allocated |

The native crash resolves to the weapon-definition damage read in
`fire_grenade`, called by `player_die` with grenade time and offhand index
reversed. Mac 1.3 passes the offhand index fourth and fuse time fifth. Engine
code was unchanged in that oracle continuation. The
[engine follow-up](../../docs/macos-port/reports/2026-10-24-F1-grenade-death.md)
corrects this call and records subsequent native replay results.
