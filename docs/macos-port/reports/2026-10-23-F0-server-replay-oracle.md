# F0 — server replay oracle (2026-10-23)

**Current status (F0b): VM created/booted, blocked on SSH authentication;
native clients now spawn.** Cross-server definition of done is not met. No
engine discrepancy was fixed. The original F0 results below are historical;
see the F0b continuation at the end for current state.
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
