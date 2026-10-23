# F0 — server replay oracle (2026-10-23)

**Current status (F0c): original rebuilt byte-identically; live reference
recording and both replays pass. Both native HEAD replays crash after frame
373.** Cross-server definition of done remains blocked by engine behavior.
No engine discrepancy was fixed. The F0/F0b sections below are historical;
see the F0c continuation at the end for current results.
Branch: `decomp/f0-oracle`; starting commit:
`4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7` (clean worktree).

Private evidence: `~/Library/Application Support/CoD2-native-decomp/F0.noindex/`.
The [oracle README](../../../tools/oracle/README.md) gives VM, rebuild,
instrumentation, recording, replay, comparison and x87 commands.

## Delivered

- COD2_X64-gated hooks in `sysdiff_statehash.c`, `SV_ClientThink` and `SV_Frame`
  record ordered usercmd fields, replay before each simulation tick, suppress
  network input during replay, and emit core player fields with raw float bits.
  Existing entity trace format and seed hook are retained. Recording synthesizes
  turning, movement and attack commands for test clients; both modes suppress
  stock bot input and its RNG consumption.
- Extended existing `tools/parity/record.py` and `compare.py`; no parallel trace
  comparer was introduced. Comparison names the first frame/client/field and
  labels adjacent finite same-sign float bits `float-last-bit`, other value
  differences `logic`, and malformed/missing framing `harness`. These are
  observation labels, not root-cause findings.
- Runtime test-client bootstrap reads the installed DM script, enables its
  existing dormant helper, and writes only into a fresh private homepath. No
  game-script text is stored in Git. `--clients` sets scr_testclients and
  sv_pure 0 on the local test server.
- VM creation, guest setup, rebuild-check and Docker server-adapter scripts;
  an i386 libm/x87 probe; and a generator for a private instrumented cod2engine
  copy. The generated private patch stays in evidence, never Git.
- Fifteen synthetic Python tests and an ASan/UBSan C fixture using the actual
  native hooks. The fixture exercises time rebasing, network suppression,
  command injection, exact player bits, and malformed/truncated streams.

The interception point follows Mac 1.3
`__Z14SV_ClientThinkP8client_sP9usercmd_s` at `0x17da1a`: instructions copy seven
32-bit words from usercmd into lastUsercmd, compare client state with 4, then
call G_SetLastServerTime and tail-call ClientThink. The new hook runs before
that copy; inactive instrumentation leaves those existing operations intact.
This is instrumentation, not a gameplay reconciliation. No Mac/cod2engine
behavior conflict was resolved or claimed absent.

## First replay result

The stock native **dedicated** target ran headlessly with read-only
`~/Games/CoD2`, fresh homepaths, mp_toujane DM, seed 12345, fixedtime 50 and
sv_fps 20. One 2,000-frame stream was recorded and replayed twice on unique
ports 29222, 29223 and 29224. Both replays and the recording agree on all
2,000 normalized entity hashes and all emitted player fields. No first
diverging frame/field exists in this native-only run.

The traces cover engine frames 4–2003. Two active clients appear at frame 101;
the stream has 3,804 commands (from frame 102), 3,806 player records and the
terminal record `end 2003`. **Every player record has pm_type 4, health 0 and
weapon 0.** The clients moved/turned as spectators. This validates repeatable
input delivery and observation, with no live-player movement/combat coverage.
Whether the helper's failure to spawn players is a harness or native script
issue is unresolved until the original can run; it is not labelled an engine
bug. A preceding 200-frame smoke also matched.

The original-server recording, its two replays, and the original/native
comparison are **not run**. Their first divergent frame and classification
are **unknown**, not “none”. The native stream is not an original-server corpus.
No GUI game or performance measurement ran; no benchmark lease was needed or
taken. Screen lock state was not collected because no performance claim is made.

Reproduce the native result (use new output names when rerunning):

```sh
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
  --binary build-f0-stock/cod2_macos_ded --data "$HOME/Games/CoD2" \
  --output "$evidence/native-record-2000" --dedicated --players --clients 2 \
  --commands-record --frames 2000 --port 29222 --timeout 240
# Run twice, replacing output/port with native-replay-b-2000/29224 the second time:
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
  --binary build-f0-stock/cod2_macos_ded --data "$HOME/Games/CoD2" \
  --output "$evidence/native-replay-a-2000" --dedicated --players --clients 2 \
  --replay "$evidence/native-record-2000/usercmds.txt" \
  --frames 2000 --port 29223 --timeout 240
python3 tools/parity/compare.py "$evidence/native-replay-a-2000/statehash.txt" \
  "$evidence/native-replay-b-2000/statehash.txt" \
  --players "$evidence/native-replay-a-2000/players.txt" \
  "$evidence/native-replay-b-2000/players.txt" --require-clients 2
```

## VM/rebuild/probe result

| Check | Actual result |
| --- | --- |
| Official Debian arm64 image | Downloaded; SHA512 matches official SHA512SUMS |
| Disk preparation | Private 36 GiB qcow2 prepared; UTM not reconfigured |
| Original supplied binary SHA256 | Matches `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` |
| cod2-oracle created / booted | No / no |
| SSH / Docker linux/386 uname | Not run / not run |
| make toolchain / check / verify | Not run |
| Rebuilt binary SHA identical | Unknown; no rebuilt binary |
| TUs verified / total | Not run / 157 source TUs listed by reference Makefile |
| .text coverage | Not run |
| Private instrumentation generation | Passed against a private host copy; guest compilation/runtime unverified |
| x87 probe | C syntax check only; no i386 result output |

UTM's process existed but AppleScript returned -600 and utmctl list hung.
UTM's unified log identifies the responsible application as Warp and shows
TCC timeout followed by errAEEventNotPermitted. Read-only UTM Registry was
empty. No VM was created and no host permission/configuration was changed.

Required human click path: **System Settings → Privacy & Security → Automation
→ Warp → enable UTM**, or approve the pending “Warp wants to control UTM”
dialog. Then open UTM from Applications and rerun utmctl list. If the launcher
changes, the listed responsible app may change too.

Automatic approval review separately rejected generating the task-private
Ed25519 SSH key: “Access to a sensitive path is not allowed”, rule
`secret.pattern.ssh-key-basename`. The command did not run. No key was created,
and no retry, renaming or workaround was attempted. The user must manually
prepare the private key/public-key seed before VM creation. The exact seed
and resume steps are in the README and private `vm-outcome.md`.

## Local verification

Commands actually run; logs use the corresponding names under evidence:

```sh
taskpolicy -b nice -n 19 cmake -S . -B build-f0-stock -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-f0-stock \
  --target cod2_macos_ded cod2_macos --parallel 2
taskpolicy -b nice -n 19 cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
taskpolicy -b nice -n 19 cmake --build build-macos-codx \
  --target cod2_macos_ded cod2_macos --parallel 2
python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/fidelity -v
taskpolicy -b nice -n 19 python3 tools/oracle/test_hooks.py \
  --compile-db build-f0-stock/compile_commands.json --output "$evidence/hooks-stock-rebased"
taskpolicy -b nice -n 19 python3 tools/oracle/test_hooks.py \
  --compile-db build-macos-codx/compile_commands.json --output "$evidence/hooks-codx-rebased"
python3 tools/check_trace_guards.py
sh -n tools/oracle/guest-setup.sh tools/oracle/container_server.sh
bash -n tools/oracle/rebuild-reference.sh
osacompile -o "$evidence/vm-final.scpt" tools/oracle/vm.applescript
clang -std=c99 -D__i386__ -Wall -Wextra -Werror -fsyntax-only tools/oracle/x87_probe.c
git diff --check
```

All four native targets built. Tools tests: 47 passed; fidelity tests: 35
passed; both sanitized hook fixtures passed. Trace guards: zero unguarded
diagnostics. Script/probe checks prove syntax only. The first broad tools run
had one unchanged 0.3-second synthetic-process timeout during compilation;
the later complete run passed without changing the test/timeout. The hook
fixture initially failed to link on missing hooks before implementation.

ABI checker unit tests passed (4 audit, 6 import, 3 raw-offset and 4 bit-conversion
tests). The stock full ABI command completed its raw-offset stage: 400 sources,
1,214 sites, zero unreviewed/unsafe. It was stopped during the subsequent
634-TU function audit, so there is **no complete ABI result**. The stock and
CoD2x fidelity stub audits were also interrupted without results. These broad
AST passes were not continued into an extended repository audit after the VM
blocker; the compiled hooks, sanitizer fixture and required native replay are
the unit's completed behavior checks. The orchestrator must run its integrated
merge gates. Logs and `gate-status.txt` preserve the partial outcomes.

For each of the three modified engine files, `unifdef -UCOD2_X64` output from
the starting commit and working source passed actual `cmp`:

```sh
base=4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7
for file in src/unix/sysdiff_statehash.c src/PC/server_mp/sv_client_mp.c src/PC/server_mp/sv_main_mp.c; do
  cmp <(git show "$base:$file" | unifdef -UCOD2_X64) <(unifdef -UCOD2_X64 "$file") || exit 1
done
```

This is a byte comparison of preprocessed source, not a Linux i386 object or
binary comparison. The x86 Linux comparison is unavailable. Known red gates
remain visible: `legacy-abi` has 10 known differences and
`tests/cod2x/check_inactive_gates.py` has 436 known differences. Neither is
claimed fixed/green. The full repository merge matrix is not claimed green.

## Merge/resume notes

Merge as a **blocked F0 harness foundation**, not as a finished reference
oracle or gameplay fix. New hooks are off without diff environment switches;
the engine OFF-source comparisons pass. No game data, binary, decompiler
output, cod2engine text or private patch is included. No remote operation ran.

Next: resolve the two host blockers, create the VM, run the stock rebuild,
compile the private instrumentation, collect original repeatability and x87
results, and investigate spectator-only coverage before interpreting gameplay
parity. Do not fix engine differences inside this oracle unit.


## F0b continuation — 2026-10-23

**Blocked:** after NoCloud completed, the supplied agent-authenticated SSH
command returned exit 255, `Permission denied (publickey)`. The brief says
“If authentication fails, stop and report it; do not work around it.” No
further SSH attempt or alternate access method followed that failure. No key
was generated, copied, printed or moved. The VM was cleanly shut down through
a power-down request and remains present and stopped.

Started at `02094fd` on `decomp/f0-oracle`. The pre-existing untracked
`build-f0-stock/` was reused for the requested stock rebuild and is excluded
from the commit. Execution was inline Codex, no delegated workers, remote
operations, engine changes or standalone shipping workflow.

### What changed and what ran

- Fixed UTM 4.7.5 AppleScript creation: `serial ports` resolved to the serial
  element instead of the configuration property and caused error -1700.
  Using property code `SrPt` permitted creation. Directory-share `none` and
  TCP also use explicit enumeration codes. The installed UTM scripting
  dictionary and [official scripting reference](https://docs.getutm.app/scripting/reference/)
  provide these codes and types.
- Built the NoCloud ISO from the user-prepared seed. The initial genericcloud
  image with USB seed booted to `localhost`, without cloud-init, and SSH
  timed out before its banner. Changed the same VM to fixed VirtIO seed and
  the checksum-verified official `debian-13-generic-arm64.qcow2`, resized to
  36 GiB. Both changes are recorded; their individual necessity was not
  isolated. Final serial output reports cloud-init 25.1.4 finished after
  13.75 seconds using `DataSourceNoCloud [seed=/dev/vda]`, and SSH started.
  The generic image's SHA512 matched the previously downloaded official
  SHA512SUMS. No authenticated guest shell was obtained.
- The stock helper's menu notifications still produced spectators after a
  trial 0.1-second delay. That failed approach was removed. The recorder now
  derives the installed DM/team scripts at runtime and redirects the test
  helper to an independently written selection routine calling the stock
  gametype's team and weapon callbacks after 0.25/0.5-second waits. It selects
  Thompson for allies and MP40 for axis. No extracted script is committed.
  This uses the stock game script API, and changes no engine behavior.
- The native 300-frame smoke recorded frames 4–303. Both clients first spawn
  at frame 116: pm_type 0, health 100, weapons 11 and 18. Each has 188 live
  records; client 0 has 164 distinct live origins and client 1 has 188.
  Weaponstate 3 occurs in 50 records. All live records have health 100:
  **no damage, death or respawn is claimed**, and the existing command
  generator has not yet been extended for jump, crouch/prone or grenades.
  The reference side of this bootstrap remains untested.

### Exact verification and retained evidence

From this worktree:

```sh
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
taskpolicy -b nice -n 19 cmake --build build-f0-stock \
  --target cod2_macos_ded --parallel 2
# Original 300-frame native smoke; choose a new output directory when rerunning:
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
  --binary build-f0-stock/cod2_macos_ded --data "$HOME/Games/CoD2" \
  --output "$evidence/native-f0b-callback-smoke" --dedicated --players \
  --clients 2 --commands-record --frames 300 --port 29226 --timeout 90
python3 -m unittest discover -s tools/tests -v
osacompile -o "$evidence/vm-f0b.scpt" tools/oracle/vm.applescript
osascript tools/oracle/vm.applescript \
  "$evidence/debian-13-oracle-generic.qcow2" "$evidence/cloud-init.iso"
/Applications/UTM.app/Contents/MacOS/utmctl list
du -sk "$HOME/Library/Containers/com.utmapp.UTM/Data/Documents/cod2-oracle.utm"
git diff --check
```

Native dedicated build passed (`build-f0b-stock.log`). The existing bootstrap
integration test was strengthened; it failed before implementation and the
final full tools suite passed **47 tests** (`f0b-tools-tests.log`). This
synthetic test checks script/bootstrap delivery, not real spawning; the
native smoke supplies the real spawning observation. AppleScript compiled
and existing-VM lookup returned the correct UUID without changing/starting
it. The final fresh-creation fixed-seed variant is syntax checked, with
matching drive settings applied to the existing VM, rather than tested by
creating a second VM. `guest-generic-boot.log` retains serial boot output;
`native-f0b-callback-smoke/` holds traces, stream, metadata and console log.

The failing SSH invocation used agent authentication, with the new host key
accepted into task-private known_hosts:

```sh
ssh -p 22220 -o ConnectTimeout=10 -o StrictHostKeyChecking=accept-new \
  -o UserKnownHostsFile="$evidence/known_hosts" oracle@127.0.0.1 \
  'uname -a; cloud-init status; df -h /'
# exit 255: oracle@127.0.0.1: Permission denied (publickey).
```

**Not run:** guest-setup.sh; Docker linux/386 uname; make toolchain/check/verify;
tu_cov.py; instrumented reference compilation/runtime; x87 probe; original
recording and two replays; native replay of the original live corpus;
repeatability and cross-server comparisons. Rebuilt SHA equality, TU coverage,
and the first real diverging frame/field are **unknown**. There is no emulator,
harness or engine-logic classification to make yet. The comparison command
remains documented in tools/oracle/README.md, but has not run cross-server.
No Mac/cod2engine disagreement is claimed resolved or absent.

No engine file changed, so this continuation has no new OFF-source comparison
target. The previous three-file unifdef/cmp results remain as recorded above.
Only the focused tooling suite and native dedicated rebuild ran; the full
merge matrix was not rerun. Known red gates remain: legacy-abi 10 differences,
inactive CoD2x gates 436 differences; x86 Linux comparison unavailable.
No GUI/performance run occurred and no benchmark lease was needed.

### Handoff

VM UUID `0DDDB408-5D92-451D-8691-58B1ABD35D7D`, named `cod2-oracle`, stopped.
Bundle: `~/Library/Containers/com.utmapp.UTM/Data/Documents/cod2-oracle.utm`.
Allocated disk use: **1,220,868 KiB** (`du -sk`), approximately 1.2 GiB;
virtual system disk 36 GiB, seed about 900 KiB; 4 vCPUs / 8 GiB RAM. Seed is
currently `/dev/vda` and system disk `/dev/vdb` (the creation script returns
this VM unchanged). There are no scratch builds outside the worktree.

The user must make the supplied SSH-agent authentication work in the task's
terminal context. Resume with plain `ssh -p 22220 -o
UserKnownHostsFile="$evidence/known_hosts" oracle@127.0.0.1` after starting
this same VM. Then follow the README's guest setup/rebuild/instrumentation
steps, extend the corpus to all required live actions, and run both pairs of
replays before cross-server triage. Merge this as a **blocked F0b continuation**,
not as a finished oracle. The orchestrator owns integrated merge verification.


## F0c continuation — 2026-10-23

Started at `a32ffbec185e044c8e3c9595f2ac7a90b5288a9a`, on
`decomp/f0-oracle`. The only inherited tracked edit was the orchestrator's
`docker-cli` addition to guest setup; it is included as requested. The existing
`build-f0-stock/` was reused and remains untracked. Execution was inline, with
no sub-agents, engine changes, remote publication or host package/config changes.
The supplied SSH agent worked; no credential files were handled.

### Rebuild and guest fixes

- Added Debian 13's `docker-buildx` alongside the inherited `docker-cli`, and
  enabled BuildKit in the rebuild script. The inherited classic build failed
  at its final `scratch` stage because it was labelled arm64. The fresh
  BuildKit attempt passed that builder setup but stalled downloading binutils
  from the historical archive (zero bytes for several minutes). Only this
  run's Buildx process was terminated. Its `make toolchain` exit is **2**;
  it is not reported as successful.
- Recovered the complete Ubuntu i386 `/rootfs` retained in the earlier
  bootstrap image `c2d5e9b85f07`, using the new `finish-toolchain.sh`. It imports
  that root filesystem with explicit `linux/386` metadata and checks GCC 3.3
  `1:3.3.6-8ubuntu1`, G++ 3.3 of the same version, GCC 3.4 `3.4.4-6ubuntu8`,
  binutils `2.16.1-2ubuntu6`, and libc6-dev `2.3.5-1ubuntu12.5.10.1`.
  The resulting image reports `linux/386` and runs `uname -m` as `i686`.
  This is the allowed alternate image-production path; cod2engine sources
  and its Dockerfile were not edited.
- The recovered-toolchain run completed `make check`, `make verify`, and
  `python3 tools/tu_cov.py`, each exit **0**. All **157/157 TUs** verified.
  `.text` coverage is **99.96%**, with **434 / 998,480 bytes uncovered**;
  the coverage tool separately identifies 359 bytes of alignment fill.
  No attempt was made to close or analyze those remaining bytes.
- The rebuilt stock executable and supplied original have the same SHA256:
  `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`.
  The script's `cmp` output is empty. The generated instrumentation was built
  using the same compiler image. Unmodified objects from the verified stock
  build were reused; the four modified TUs and our new hook TU were compiled.
- The first instrumented launch discovered zero IWDs. A syscall trace of the
  byte-identical **stock** rebuild, followed by a small private directory
  probe, isolated `readdir` EOVERFLOW: 64-bit directory offsets returned through
  QEMU cannot be narrowed by the old libc's 32-bit API. `readdir64` listed the
  files successfully. The adapter now uses a 1 MiB tmpfs directory containing
  symlinks to the read-only IWD bind mount. The same probe then reports errno
  0 for both APIs. No game bytes were copied or changed for this workaround.
  The instrumented server subsequently discovered **81,663 packaged files**
  and loaded **mp_toujane**.

### Live reference corpus

The reference recording `original-f0c-live-record` contains 2,000 frames,
**4–2003**, with seed 12345, fixedtime 50, sv_fps 20, two test clients and
3,804 ordered commands, terminated by `end 2003`. Its 3,806 player records
include **3,735 live records**. Both clients first spawn at frame **116**, with
health 100 and weapons 11/18. The generator modifies only our recording
workload in the private reference copy. Native runs inject the saved stream.

`live_coverage.py --require-all` passes on observed state, with these first
frame/client pairs (counts are player records, not distinct button presses):

| Observed action | First frame / client | Records |
| --- | --- | --- |
| live player | 116 / 0 | 3,735 |
| changing live origin | 117 / 0 | 1,306 |
| jump input, jump flag and upward velocity | 600 / 0 | 18 |
| crouch | 120 / 0 | 386 |
| prone | 160 / 0 | 316 |
| firing weapon state | 200 / 0 | 652 |
| frag input and offhand state | 281 / 0 | 447 |
| health drop | 499 / 1 | 1 |
| death after positive health | 499 / 1 | 1 |
| respawn after death | 540 / 1 | 1 |

The workload holds/release frags and pulses **use**, as the installed DM
script's normal respawn handler requires. The health drop is fatal; this
corpus does not establish a surviving wound or a player-versus-player kill.
No teleport, scripted damage or forced respawn was used.

Reference replays `original-f0c-live-a` and `original-f0c-live-b` both completed
2,000 frames. `tools/parity/compare.py` reports a match for the recording
versus replay A, and for replay A versus B, including frame, relative time,
active entity count, quantized entity hash and every emitted raw player field.
The unnormalized full-struct raw hash is not the comparison contract.

### x87 probe

The probe compiled and ran inside the recovered i386 toolchain container.
QEMU is **10.0.13**, Debian `1:10.0.13+ds-0+deb13u1`; the probe reports
control word **037f**, float/double/long-double sizes **4/8/12**, and 40 libm
results. For its binary64 pi input, `sin` gives binary64
`3ca1a62633145c07`, rounded binary32 `250d3132`. The complete output is
`$evidence/x87-probe-f0c.log`. This is a baseline for this emulator/compiler,
not evidence that transcendental functions match hardware or Mac SSE.

### Native comparison and actionable engine findings

The primary native runs are `native-f0c-head-a` / `native-f0c-head-b`, ports
29239/29240. CMake was configured at **a32ffbe** and the stock Release arm64
dedicated target rebuilt with two jobs before these runs. The inherited cache's
crash stamp initially still said `4f0ba10`; those earlier `native-f0c-live-*`
attempts are exploratory. The primary crash reports correctly identify
`a32ffbe`. All five primary runs use **the same 28 IWD hashes**.

Both primary native attempts exit via **SIGSEGV (-11)** after emitting **370
frames, 4–373**. The recorder returns **1** for each failed run. Comparison of
those two prefixes matches all 370 frames and player fields. This is only
prefix repeatability: **neither requested 2,000-frame native replay completed**,
and the native repeatability gate is not green.

The normal cross-build comparer reports:

```text
FAIL: first difference at record 1, frame 4, field ents (logic)
left:  (4, 350, 168, ..., de316057)
right: (4, 350, 205, ..., fa03018e)
```

There is **no client number** for this first divergence: zero clients are
active at frame 4. This is triaged as an **engine world-initialization logic
mismatch**, rather than an emulator floating-point finding. A targeted one-frame
initialization dump has every reference slot present in native plus **37
additional slots**: 7 type-9 turret entities, 25 type-6 script models and 5
type-0 entities. Of the shared slots, 617/618 also have different quantized
state: their reference model origins are (1069,2761,87)/(2818,1640,79), while
native leaves both at zero. It points at `G_SpawnEntitiesFromString`,
`G_CallSpawn`, `G_SpawnTurret`, model positioning/linking, and script deletion
through `ScrCmd_Delete` / `G_FreeEntity`. The exact cause and whether any part
is an intentional Mac/Linux difference remain unresolved; there is no original
Mac runtime entity-count result. No engine patch was made.

The first **player-only** difference is frame **102**, client **0**,
`velocity[1]`: reference `c390e697`, native `c390e696`, labelled
`float-last-bit` by `player_difference` in the existing comparer. Both players
are still spectators. The x87 probe is retained as the emulator baseline;
without an Intel hardware control or a targeted arithmetic probe, this
secondary one-ULP difference is not attributed specifically to QEMU rather
than Linux x87 code generation versus Mac/arm64 SSE-style rounding.

The separate blocking crash has a concrete engine cause:

- During the tick following emitted frame 373, the stack goes through
  `G_RunMissile` → `G_ExplodeMissile` → `G_RadiusDamage` → `G_Damage` →
  `Scr_PlayerDamage` → `PlayerCmd_finishPlayerDamage` → `player_die` →
  `fire_grenade+0xd0`. Fault address is **0x318**. `atos` resolves the fault to
  `src/PC/game_mp/g_missile_mp.c:317`, the weapon definition's damage read.
- `src/PC/game_mp/g_combat_mp.c:685` passes `ps.grenadeTimeLeft` as the fourth
  argument and `ps.offHandIndex` as the fifth. `fire_grenade` interprets the
  fourth as a weapon index, then dereferences the result of
  `BG_GetWeaponDef` at the faulting damage read.
- **Mac evidence:** `__Z10player_dieP9gentity_sS0_S0_iiiPKf13hitLocation_ti`
  at **0x1c4a7c**, specifically **0x1c4dd3–0x1c4df5**, loads client offset
  `+0x3c` (grenade time) into outgoing stack argument 5 and `+0xd0` (offhand
  index) into argument 4 before calling `fire_grenade`.
  `__Z12fire_grenadeP9gentity_sPfS1_ii` at **0x1c9796**, specifically
  **0x1c9809–0x1c980f**, sends incoming argument 4 to `BG_GetWeaponDef`.
  The Linux reference agrees with this ordering. There is no authority
  disagreement on this finding.

The native grenade-death argument order is an **engine logic defect**, not
an x87-transcendental classification. It must be fixed by the owning engine
unit before the full live corpus can complete. This oracle unit deliberately
leaves engine code unchanged.

### Reproduction, verification and retained state

The [oracle README](../../../tools/oracle/README.md) is the command sequence
for guest setup, ordinary BuildKit/recovered toolchain, stock rebuild,
instrumentation, recording, both reference replays, both native attempts,
comparison and x87. New output/destination names are required on reruns.
The private evidence root remains
`~/Library/Application Support/CoD2-native-decomp/F0.noindex/`.
Key logs are `rebuild-f0c-recovered/{rebuild-status.txt,check.log,verify.log,
coverage.log,tu-summary.json}`, `original-record-replay-f0c.log`,
`original-repeatability-f0c.log`, `native-head-repeatability-f0c.log`,
`cross-head-f0c.log`, `live-coverage-f0c.json`, `initial-entities-f0c.json`,
`x87-probe-f0c.log`, and each primary run's `run.json`, state/player trace and
native `console.log` crash report. The private generated reference patch,
objects, executables and full verification files remain inside the VM.
No game data, reference source, pseudocode, disassembly or captures enter Git.

Focused local verification actually run:

```sh
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
taskpolicy -b nice -n 19 cmake -S . -B build-f0-stock -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-f0-stock --target cod2_macos_ded --parallel 2
python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/fidelity -v
sh -n tools/oracle/guest-setup.sh tools/oracle/container_server.sh
bash -n tools/oracle/rebuild-reference.sh tools/oracle/finish-toolchain.sh
git diff --check
```

The native dedicated target builds. Tools tests: **49 pass**; fidelity unit
tests: **35 pass**. The new coverage check was observed failing before its
implementation, then passing; its tests reject command-button-only coverage
and distinguish initial health-zero spectators from a genuine death/respawn.
The shell changes were exercised by the actual recovery, build and reference
runs; ordinary fresh BuildKit completion remains unverified after the stalled
download. No engine file changed, so there is no new OFF-source comparison
target. This Linux reference rebuild is not the repository's legacy OFF-object
gate. The full merge matrix was not rerun. Known red gates remain visible:
**legacy-abi: 10 differences; inactive CoD2x gates: 436 differences; repository
x86 Linux byte comparison: unavailable**.

VM `cod2-oracle`, UUID `0DDDB408-5D92-451D-8691-58B1ABD35D7D`, was shut down
with `utmctl stop ... --request`; `utmctl status` reports **stopped**. It is
present, with a 36 GiB virtual disk and **6,774,404 KiB allocated** for the VM
bundle (about **6.46 GiB**, `du -sk`). Guest root filesystem used 6.2 GiB of
36 GiB before shutdown. No GUI game/performance run or app bundle was created;
no benchmark lease was needed. No scratch build directory was created outside
this host worktree. The orchestrator owns integrated merge verification.

Merge this as an operational reference oracle with a reproducible native
engine blocker, **not as completed cross-engine parity**. The next engine unit
can start with the proven grenade argument-order defect, then rerun the same
saved corpus and investigate the earlier initialization mismatch.
