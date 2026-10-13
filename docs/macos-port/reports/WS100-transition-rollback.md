# WS100 — Commit native CoD2x transitions only after enqueue success

Date: 2026-10-09. Branch: `codex/transition-rollback`. Base:
`048ff5e3004807e41a3f263179bc5d62efc717f2`.

## Finding and reference evidence

The candidate is real. A tagged append rejected by the production command
buffer left `level.finished` and `level.savepersist` committed. A later GSC
request failed its already-called guard, even after the buffer drained.
`exitlevel()` also ran match-end, reset scores, and changed connected clients
to `CON_CONNECTING` without an enqueued transition. This happened with the
optional callback absent as well as present, at hosted revision 0.

The reference establishes an important boundary: a completely full buffer
already fails this way in stock Mac 1.3. The introduced capacity window comes
from the mandatory approval marker. With 14 bytes free, `fast_restart\n`
(13 bytes) fits; `fast_restart __cod2x_stop_1\n` (28 bytes) does not.

Read the private Mac 1.3 binary with `nm -a` and
`otool -arch i386 -tV -p <symbol>`. Relevant functions and observations:

| Symbol | Address | Observation |
| --- | --- | --- |
| `__Z15GScr_MapRestartv` | `0x0019a42a` | Stores finished/persist before the void append. |
| `__Z12GScr_LoadMapv` | `0x0019a4a2` | Same ordering for the untagged map command. |
| `__Z14GScr_ExitLevelv` | `0x0019a574` | Stores state, calls match-end, then calls `ExitLevel`. |
| `__Z9ExitLevelv` | `0x001adcea` | Appends, then unconditionally clears scores and changes client states. |
| `__Z12Cbuf_AddTextPKc` | `0x0004e7fe` | Accepts only when size plus length is strictly less than maxsize. |
| `__Z16Cbuf_ExecuteTextiPKc` | `0x0004f150` | Append mode uses the same strict capacity check. |

The referenced command strings at `0x2dda9c`, `0x2d6e64`, and `0x2e00c0`
confirm `fast_restart\n`, `map %s\n`, and `map_rotate\n`. Raw disassembly
and the local string receipt remain outside Git in the evidence directory.

Read CoD2x 1.4.6.8's `src/shared/cod2_cmd.h`, `gsc.cpp:214–258`, and
`server.cpp:1126–1192`. It delegates command append to the original engine,
uses untagged commands, and invokes its optional stop callback from command
execution. Its hooks do not introduce this marker capacity window. No
reference implementation was copied.

## Change and decisions

Only the native CoD2x path changes:

- Preflight prepares approval metadata without committing the level fields.
- The tagged queue helper returns append success, inferred from the production
  buffer's exact size increase. Success commits finished/persist; rejection
  invalidates the approval source and marker and preserves the previous fields.
- The native exit-level queue hook returns failure to `ExitLevel`, which then
  returns before score/client mutations. Script match-end moves into the
  successful tagged exit queue path. The untagged direct-helper fallback keeps
  its original behavior. The only production `ExitLevel` caller is GSC.
- Stock and native-OFF paths retain the original full-buffer behavior.

The approval marker remains necessary to bind permission to the particular
queued request. This fix does **not** make a 28-byte tagged command fit a
14-byte space: that request is rejected without half-committing the level,
and GSC can retry after drainage. A successful append followed by a later
command-execution failure is outside this fix's scope.

Changed engine files: `g_scr_main_mp.c`, `g_main_mp.c`, and the native-only
declaration in `server_mp/cod2x_stop_gametype.h`. Neither
`sv_client_mp.c` nor `sv_main_mp.c` was changed; WS97 can compose independently.
`COD2X_HOST_REVISION=0` remains unchanged. The VM fixture follows the new
queue result and still supports the historical void hook through `--baseline`.

## Red tests and verification

Evidence root (left on disk):
`~/Library/Application Support/CoD2-native-current-readiness/WS100.noindex/`.
All compilation/heavy checks inherited `taskpolicy -b nice -n 19`; at most two
compiler processes ran concurrently. No game window, game data, benchmark
lease, or real player homepath was used.

The new `stop_gametype.c` cases use production `Cbuf_Init`, `Cbuf_AddText`,
`Cbuf_ExecuteText`, and `Cbuf_Execute` with a real 0x10000 buffer. Padding is
appended through production code, and drainage executes production buffer
code. They cover both optional-callback states in both engine modes:

1. 14-byte-free restart: stock accepts; CoD2x rejects and preserves state.
2. Fully rejected map append: no leaked string/approval, drainage, and a
   successful fresh GSC retry in CoD2x. Stock retains its original retry error.
3. Full and 12-byte-free exit append rejection: preserve both team scores,
   connected-client score/state, an already-connecting client's score/state,
   and match-end count; retry succeeds after drainage in CoD2x.

Before any production edit, running the new fixture against `048ff5e` produced
**54 failed assertions in CoD2x**, including already-called errors on retry.
The stock reference control produced **0 failures**. Logs are `red-codx/` and
`red-stock/`; final historical-source dedicated receipts reproduce the same
54/0 results. The historical CoD2x VM fixture also still passes.
These are source-function fixtures with synthetic external services, not live
gameplay claims.

Configuration and reproducible commands (run from this worktree):

```sh
E="$HOME/Library/Application Support/CoD2-native-current-readiness/WS100.noindex"
taskpolicy -b nice -n 19 cmake -S . -B build-ws100-stock \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake -S . -B build-ws100-codx \
  -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1

# Red, before the fix: stock exit 0; CoD2x exit 1 / 54 failures.
for mode in stock codx; do
  taskpolicy -b nice -n 19 python3 tests/cod2x/stop_gametype.py \
    --baseline 048ff5e --compile-db "build-ws100-$mode/compile_commands.json" \
    --output "$E/red-$mode"
done

# Green: each stop/host lifecycle variant exits 0.
for mode in stock codx; do
  for fixture in stop_gametype host_lifecycle; do
    taskpolicy -b nice -n 19 python3 "tests/cod2x/$fixture.py" \
      --compile-db "build-ws100-$mode/compile_commands.json" \
      --output "$E/$fixture-$mode-listen"
    taskpolicy -b nice -n 19 python3 "tests/cod2x/$fixture.py" --dedicated \
      --compile-db "build-ws100-$mode/compile_commands.json" \
      --output "$E/$fixture-$mode-dedicated"
  done
  taskpolicy -b nice -n 19 python3 tests/cod2x/run_host_protocol.py \
    --compile-db "build-ws100-$mode/compile_commands.json" \
    --output "$E/host-protocol-$mode"
  taskpolicy -b nice -n 19 python3 tests/cod2x/stop_gametype_vm.py --ubsan \
    --compile-db "build-ws100-$mode/compile_commands.json" \
    --output "$E/stop-vm-final-$mode-listen"
  taskpolicy -b nice -n 19 python3 tests/cod2x/stop_gametype_vm.py --ubsan --dedicated \
    --compile-db "build-ws100-$mode/compile_commands.json" \
    --output "$E/stop-vm-final-$mode-dedicated"
done

TMPDIR="$E/scratch.noindex/" COD2X_SANITIZERS=1 \
  taskpolicy -b nice -n 19 sh tests/cod2x/run_full.sh
TMPDIR="$E/scratch.noindex/" \
  taskpolicy -b nice -n 19 sh tests/cod2x/run_native.sh
git diff --check
```

Use fresh output directory names when replaying fixtures that reject existing
directories. Configurations produced actual engine compile databases; this
unit compiled both changed engine TUs for listen/dedicated in both modes
(eight successful compilations), rather than linking entire game binaries.
The preserved databases are `compile_commands-{stock,codx}.engine.json` under
the evidence root. Scratch CMake build directories are removed at completion.

The original 12-command matrix passed. CoD2x real-VM listen/dedicated contracts
passed with ASan/UBSan, compiling and executing synthetic GSC with the actual
compiler/VM. The stock VM run covers synthetic loading/lookup; the callback
contract is CoD2x-only. The VM fixture's append service is synthetic; buffer
overflow/retry acceptance comes from the production-buffer fixture above.
The broader `run_full.sh` (ASan/UBSan) and `run_native.sh` also exited 0.
Their synthetic URL-probe bundles lived under `.noindex`, were unregistered
with `lsregister -u`, and their temporary directories were removed.

Focused ABI: `g_main_mp.c`, `g_scr_main_mp.c`, and the two direct hook-header
consumers (`sv_ccmds_mp.c`, `sv_init_mp.c`) in each target, eight TUs per mode:

```sh
for mode in stock codx; do
  taskpolicy -b nice -n 19 python3 tools/abi/audit.py \
    "$E/abi-$mode.noindex/compile_commands.json" --jobs 1 \
    --baseline tools/abi/baseline.json \
    --output "$E/abi-$mode.noindex/functions.json"
done
```

Both modes report **0 errors, 0 mismatches, 0 new mismatches**. The supported
listen-target raw-offset checks (one TU/database to bound concurrency) also
report zero unsafe/unreviewed sites. Eight attempts using dedicated-only
databases exited 1 because the checker explicitly selects `cod2_macos.dir`
and raises `No cod2_macos sources in compile database`; those attempts are
unsupported checks, not green results. Receipts: `abi-results.json`,
`matrix-results.json`, `supplementary-results.json`, and `additional-results.json`.

All eight changed-TU import checks (two engine files × two targets × two modes)
exit 0 with zero proven extra/missing import dereferences. Both focused
bit-conversion audits report zero violations. The leaf preservation checker
reports three changed production files, ten configurations, zero mismatches:

```sh
# Import matrix: mode=stock/codx; source=g_main_mp/g_scr_main_mp;
# target=listen/dedicated. Exact invocations: abi-additional-results.json.
taskpolicy -b nice -n 19 python3 tools/abi/imports.py \
  --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  --compile-commands "$E/abi-codx.noindex/g_scr_main_mp-listen.json" \
  --json "$E/abi-codx.noindex/g_scr_main_mp-listen.imports.json" --check
for mode in stock codx; do
  taskpolicy -b nice -n 19 python3 tools/abi/astdump_strict.py \
    "$E/abi-$mode.noindex/compile_commands.json" --jobs 1
done
taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base 048ff5e
```

Archived focused databases retain all compiler flags; their working-directory
metadata is relocated to this worktree for replay after scratch cleanup. The
original database copies are retained alongside them. Fixture executables,
objects, generated fixture sources, and ABI caches were removed; logs and
receipts remain in the evidence directory.

`unifdef -UCOD2_X64` followed by `cmp` against the base passes for all three
modified production files. The same exact-text comparison with
`unifdef -UCOD2_CODX` also passes. Inputs, outputs, and six exit-zero comparison
results are in `preservation/`. This supplements, and does not substitute for,
the unavailable Linux i386 object/binary comparison.

The inherited guard commands were run unchanged:

```sh
taskpolicy -b nice -n 19 python3 tools/abi/legacy.py --base 664f1a1
taskpolicy -b nice -n 19 python3 tests/cod2x/check_inactive_gates.py --base 664f1a1
```

## Limits and composition

The inherited red gates remain visible and were not weakened:
`legacy-abi` has the previously reported 10 differences (generated
`build/lp64_gen/data_native.c` ×5, excluded by CMake OFF; test-only
`tests/platform/server_control.c` ×5). The brief cited **436** inactive CoD2x
differences. This rerun against `664f1a1` measured **457 differences across
92 files / 17 configurations**, and is red. Replaying the same unmodified
checker/configurations/file list with `git show 048ff5e:<path>` supplying the
pre-change sources also measured **457**; the two logs compare byte-identical
with `cmp`. WS100 therefore introduces no inactive-gate difference. The higher
count than the supplied prior 436 is already present at the composed starting
head; it is not silently treated as 436 or as green. These flags include
intentional stock/native repairs and synthetic x64-defined-zero configurations.
The x86 Linux comparison requires multilib on x86_64 Linux and is unavailable
on this Mac. Receipts: `legacy-abi-inherited.log`,
`inactive-gates-inherited.log`, and `inactive-gates-prechange.log`.

No GUI or full-game runtime acceptance is claimed. No push, tag, PR, remote
change, release, or merge to either main lineage was performed. Compose this
single focused commit onto the integration branch; no dependency on WS97.
The full linked-engine rebuild and repository merge gate remain the
orchestrator's composition checks; the evidence above is scoped to this unit.
No uncommitted changes or scratch builds remain at completion.
