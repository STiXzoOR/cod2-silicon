# F0 — server replay oracle (2026-10-23)

**Status: blocked before VM creation; native recording/replay works.** F0's
cross-server definition of done is not met. No engine discrepancy was fixed.
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
