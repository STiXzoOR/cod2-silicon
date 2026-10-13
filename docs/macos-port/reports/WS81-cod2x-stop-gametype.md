# WS81 — bounded CoD2x StopGameType callback

2026-10-07. Worker tree `/Users/stix/.codex/worktrees/dvar-ownership/cod2-native`,
branch `codex/cod2x-stop-gametype`, base
`f295e8cd7525bd3d930c0cdce15035dc4b973e12`. Changes are handed back without
staging or commits. This is the current CoD2x callback followup from WS69;
0.3 remains separate. CONTRIBUTING, WS68, WS69 and WS76 were read before work.

The first handoff was reopened after independent review found error-unwind and
FIFO approval-identity defects. The corrected implementation changes four
production TUs, including the narrowly authorized ExitLevel queue call. Parent
supplied compiler commits are separately recorded in this worker as `c48060b`
and `f47783e`; they are not WS81 edits. Correction evidence below supersedes
the earlier three-TU live receipt.

## Independently checked reference

Read-only source `/Users/stix/Projects/cod2-native-refs/CoD2x`, exact HEAD
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`:

- `src/shared/server.h:14–21`: source strings `map`, `fast_restart`,
  `map_restart`, `map_rotate`, `shutdown`.
- `src/shared/gsc.cpp:93–97`: optional
  `maps/mp/gametypes/_callbacksetup::CodeCallback_StopGameType` lookup.
- `gsc.cpp:212–257`: `(fromScript, bComplete, shutdown, source)` arguments;
  only integer zero vetoes, undefined and other types allow, shutdown ignores
  the veto, and output parameters are cleared.
- `src/shared/server.cpp:1126–1245`: running-server preflight, command/spawn/
  shutdown hooks, nested map-rotation suppression and its per-frame reset.

Native code was authored against the reconstructed engine's actual APIs and
headers. No third-party implementation was copied.

The reference's one-time permission was audited separately:
`gsc_allowOneTimeLevelChange` is initialized in `gsc.cpp:22`, checked/consumed
at 250/255, and set only by `match_cancel` at `src/shared/match.cpp:399`, before
that subsystem queues `fast_restart`. This bounded callback unit has no match
cancellation subsystem and adds no permission override. Local callback execution
does not require a private backend. This receipt does not certify deferred match,
HTTP or WebSocket transition orchestration.

## Result and intentional scope

All additions require both `COD2_X64` and `COD2_CODX`. A private handle is loaded
through the existing optional script lookup and cleared on script cleanup/load.
No shared structure or original builtin table changed. WS76's method resolver,
identity headers and cleanup remain intact. Production host readiness remains 0.
WS81 makes no compiler/VM, auth, publication, Config Studio or Metal production
change. The sole `g_main_mp.c` edit delegates ExitLevel's native CoD2x queue call
to the approved-command helper; stock retains the exact original call.

The server checks the callback before a running level is mutated by map/devmap,
fast_restart, map_restart or map_rotate, and before shutdown destroys entities or
scripts. Devmap veto happens before cheat mutation. Full spawn has its own
preflight for direct callers. An accepted operation has a balanced private depth,
so map_rotate → map → spawn and restart → spawn call once. Shutdown clears that
depth during cleanup. Non-running or callback-absent operations preserve the
original gameplay paths.

Script map/map_restart/exitlevel requests preflight before changing
`level.finished`, `level.savepersist`, scores, clients, match-end state or the
command queue. Veto leaves prior state intact; no unconditional rollback is used.
An accepted queued request records its source, finished/persist state, exact
canonical command/map argument and a unique internal marker. Only its tagged
command can consume approval. Handlers consume a presented matching marker
before map/profile/running validation, invalidate altered forms/targets, and
never reuse it. Untagged console/FIFO requests receive their own callback. No
new public command or builtin is registered. The marker sequence survives level
reloads; pending state clears on script cleanup/load and shutdown. Command-buffer
overflow invalidates permission for a command that was not appended. The
ExitLevel hook tags its existing map_rotate queue entry while preserving score
and client mutations. Persistent-round completion
and script/client persistence retain the original `savepersist` behavior.
The map argument receives a temporary string reference because adding callback
parameters releases the original builtin parameters. The native VM lacks an
exported `Scr_ClearOutParams`; the local cleanup releases actual return-value
references and drains its output stack. Exact return types are preserved. The
map's temporary string reference is tracked privately so error unwinding also
releases it.

Production RuntimeError can call Com_Error(4), which dedicated execution converts
to ERR_DROP and longjmps out of the callback. Com_ErrorCleanup already calls
Scr_Abort before server shutdown. The corrected callback checks that inactive VM
before its recursion guard and resets private state, so cleanup proceeds rather
than returning because of a stale active flag. Even a non-running shutdown clears
the depth, covering initial-spawn failure before sv_running is set. Common error
cleanup itself remains unchanged.

The depth deliberately lasts for one accepted operation, rather than until the
next game frame. The reference sets its per-frame suppression flag before a
veto and clears it only after `G_RunFrame`. Source inspection implies another
independent request in that interval can bypass its callback. WS81 does not
adopt that exemption: behavioral tests prove veto/veto/allow each calls once,
while accepted nested rotation calls once overall. This is an explicit bounded
scope difference, not a claim of identical reference per-frame policy.

While the callback VM is active, all three script transition builtins return
before their original already-finished errors or queue mutations. Actual GSC
callbacks attempting all three builtins are covered, including finished states
1, 2 and 3. Later independent requests remain retryable. No general deferred
job, match permission or arbitrary command-registration policy was added.

## Behavioral evidence

`tests/cod2x/stop_gametype.py` preprocesses and extracts the actual production
commands, full spawn/shutdown/init, script entries/loading/cleanup and real
`ExitLevel`/`G_ShutdownGame` bodies using real engine flags. It links production
dvars and CoD2x runtime; filesystem, map/frame/network and unused engine services
are fixture seams. The 480-case matrix covers five sources plus devmap, present/
absent callback, console/script context, complete/persistent state and integer
0/1/-7, undefined and float zero. Additional cases cover repeated veto/allow,
script preflight state preservation, approved queued exitlevel → rotate → map,
recursive requests, direct spawn, inactive scripts, persistent-client teardown,
callback-before-entity/script destruction and repeated shutdown cleanup.
The correction fixture also extracts the real Cbuf append/insert/wait/FIFO
execution bodies. It covers `wait 1; map B` preceding script map A, inserted B,
failed BSP/profile validation, altered command/target, replayed markers, queue
overflow, callback longjmp cleanup, held map-string cleanup and failed initial
spawn followed by an independently vetoed request. Dispatch and external error
recovery services remain explicit fixture seams.

`tests/cod2x/stop_gametype_vm.py` compiles the real script loader, lexer/parser,
compiler, variable/string stores, memory trees and VM from this tree. A synthetic
filesystem/hunk provides `_callbacksetup.gsc`; required original callbacks and
optional StopGameType are actually loaded, resolved, compiled and executed.
Production script entries are invoked from compiled GSC. A fixture-only probe
builtin reads real arguments, and real returned integer 0/1/-7, undefined,
float zero and string values exercise production interpretation and cleanup.
These are actual script loading/execution tests, not only a callback helper seam.
The correction adds production `Scr_RandomInt`, `RuntimeError` and `Com_Error`:
compiled StopGameType calling `randomint(0)` executes the real VM error catch,
dedicated ERR_DROP conversion and longjmp before real Scr_Abort recovery. UI,
diagnostic source printing and Sys_GetValue storage are fixture services.

| Receipt | Observed result |
| --- | --- |
| `/tmp/ws81-red4` before implementation | 636 intended missing-callback/ordering assertions; exit 1 |
| `/tmp/ws81-order-baseline-final` expanded baseline at `f295e8c` | 659 intended assertions; exit 1 |
| `/tmp/ws81-vm-red2` real compiled script baseline | callback/queued-transition assertion fails |
| `/tmp/ws81-reentrant-finished-red` added finished-state reentrancy coverage | real VM reports `map_restart already called` before adding the early guards |
| `/tmp/ws81-order-final-afterguard`, `/tmp/ws81-order-client-afterguard` | final CoD2x dedicated/listen ASan+UBSan: zero failures |
| `/tmp/ws81-order-stock-afterguard`, `/tmp/ws81-order-stock-client-final` | stock dedicated/listen ASan+UBSan: zero failures; stock bodies unchanged by the final gated guards |
| `/tmp/ws81-vm-all-nested-final`, `/tmp/ws81-vm-ded-afterguard` | final real GSC/VM ASan listen/dedicated passes all nested/finished/return cases |
| `/tmp/ws81-vm-stock` | stock real GSC load/compile/VM fixture passes |
| `/tmp/ws81-recovery-fifo-isolated-red` before correction | four intended failures: callback-error teardown, initial-spawn suppression and real FIFO approval identity |
| `/tmp/ws81-runtime-error-realvm-red3` before correction | actual randomint(0)/RuntimeError/Com_Error drop; aborted-VM shutdown assertion fails |
| `/tmp/ws81-recovery-ded-final`, `/tmp/ws81-recovery-listen-final` | corrected CoD2x command/FIFO/lifetime/recovery fixture ASan+UBSan: zero failures |
| `/tmp/ws81-recovery-stock-ded`, `/tmp/ws81-recovery-stock-listen-final` | corrected stock dedicated/listen command fixtures: zero failures |
| `/tmp/ws81-default-fullvm-final-ded`, `/tmp/ws81-default-fullvm-final-listen` | corrected real GSC execution/error fixture: default local source, ASan+UBSan fail-fast, passes |

Earlier fixture compilation/allocation setup errors are not behavioral red
receipts. The initial full script `--ubsan` characterization was red:
`/tmp/ws81-vm-ubsan` reports the inherited parser's
`src/PC/script/yyparse_impl.h:602:26` index -1 outside `short[200]`. WS80 owns the
separate parser/compiler fixes. After `c48060b`, fail-fast
`/tmp/ws81-composed-fullvm-ubsan-red` exposed another packed compiler store at
`scr_compiler.c:1722:9`; exit 250. Parent WS87 fixed that and literal stores
separately. Full fixture execution passed with an explicitly supplied WS87
compiler TU (`/tmp/ws81-runtime-error-realvm-green3` and
`/tmp/ws81-runtime-error-realvm-listen`), then passed with local default source
after the parent cherry-pick `f47783e`. `--ubsan` now adds
`-fno-sanitize-recover=all`. These are complete fixture script-engine sanitizer
passes, not a sanitizer claim for the whole normal game executable.

Reproduce fixture runs from this worker with `taskpolicy -b nice -n 19 python3
tests/cod2x/stop_gametype.py --compile-db <engine-db> --output <private-dir>`;
add `--dedicated`, or `--baseline f295e8cd7525bd3d930c0cdce15035dc4b973e12` for
the expanded red. The real-script runner accepts the same target/baseline
options and `--ubsan`; optional `--compiler-source <Path>` overrides only the
compiler TU for explicit private integration diagnosis. Actual databases used were
`/Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos/compile_commands.engine.json`
and its `build-macos-codx` counterpart; every source/include is remapped to
this worker. Historical primary-tree databases were not used.

## Actual Release, ABI and legacy checks

`/tmp/ws81-release-compile/receipt.json` records all 12 actual Release commands:
three changed TUs × stock/CoD2x × listen/dedicated, `-O3 -DNDEBUG` retained,
zero failures. Selected real-header ABI audits against the remapped selected
databases report six TUs each, zero errors/mismatches/new findings:
`abi-stock-final.json` and `abi-codx-final.json` in that directory.

`check_inactive_gates.py --base f295e8c`: three C files across 17 inactive CoD2x
configurations, zero mismatches. `tools/abi/legacy.py --base f295e8c
--compile-commands /tmp/ws81-release-compile/build-macos-codx.json`: three files
across five inactive configurations plus six exact SDK commands with COD2_X64
removed, zero mismatches/errors. The private sv_init include follows the original
offset assertions to preserve their `__LINE__` typedef names. Existing
`tests/cod2x/run.sh` passes; `git diff --check` passes. Existing conversion and
SDK warnings remain in logs.

These are selected compilation/signature/legacy checks. Full composed engine
builds, ABI/import/raw-offset/renderer audits, host integration and Linux i386
artifact comparison remain the parent's integration gates.

Correction checks: `/tmp/ws81-recovery-release/receipt.json` contains **16**
actual Release commands, four TUs × four targets, zero failures. Selected stock
and CoD2x ABI receipts `abi-stock.json`/`abi-codx.json` in that directory cover
eight TUs each, zero errors/mismatches/new findings. Inactive checks against
`c48060b` and final `f47783e` report four files × 17 configurations, zero mismatches; legacy reports
four files × five configurations plus eight actual SDK commands with COD2_X64
removed, zero mismatches/errors. An intermediate missing early Com_sprintf
prototype failed two Release commands and was corrected; this is compile
validation, not behavioral red evidence.

## Owned-data dedicated acceptance

The parent assigned the shared game slot. The initial acceptance binary links the completed
read-only object set from
`/Users/stix/.codex/worktrees/config-catalog/cod2-native/build-macos-codx`, exact
HEAD `865743540c34e0a268e96eeafb45ae27b1118242`, replacing only the three WS81
TUs with their actual Release objects above. Exact link command/replacements:
`/tmp/ws81-fresh-overlay/receipt.json`; output
`/tmp/ws81-fresh-overlay/cod2_macos_ded`, link exit 0. This is a private overlay
of that completed build, not a claim that this worker ran a complete fresh build.
That three-TU binary is the initial acceptance only. The corrected private
overlay `/tmp/ws81-recovery-overlay/cod2_macos_ded` uses saved objects from exact
`8f17eafa2b24ad1f2d2a61d1e494831167c14c33`, replacing four WS81 TUs from
`/tmp/ws81-recovery-release`. Link command/replacements and exit 0 are in
`/tmp/ws81-recovery-overlay/receipt.json`. The completed object set includes
WS80 compiler/parser fixes; no external WS87 object was needed for this normal
Release live run. Later parent source commits/rebuilds do not alter the already
linked private executable.

`tests/cod2x/stop_gametype_live.py` reads licensed IWD data from
`/Users/stix/Games/CoD2` without modifying it. Its private mod preserves the
original callback script, adds a synthetic StopGameType and driver, and runs
the real dedicated engine at localhost:29967 with private writable home,
empty master destinations and no players. The corrected 150-second bound includes clean
shutdown/termination fallback. Initial historical command (the fixture was subsequently expanded):

```
taskpolicy -b nice -n 19 python3 tests/cod2x/stop_gametype_live.py \
  --binary /tmp/ws81-fresh-overlay/cod2_macos_ded \
  --data /Users/stix/Games/CoD2 \
  --output '/Users/stix/Library/Application Support/CoD2-native/ws81-stop-live3'
```

All 12 cases passed: console map_restart/fast_restart/devmap/map/map_rotate
vetoes, repeated script restart vetoes, script map/exitlevel vetoes, allowed
console map, allowed persistent script restart and integer-zero shutdown ignored
exactly once with exit 0. `run.json` records executable SHA256/arguments;
`console.log` contains actual loaded-script callback markers;
`receipt.json` records the cases. No game process remained afterwards; slot
released to the parent. Live script parameters use integer 1/0 explicitly.

The corrected final command, with real `true`/`false` script arguments:

```
taskpolicy -b nice -n 19 python3 tests/cod2x/stop_gametype_live.py \
  --binary /tmp/ws81-recovery-overlay/cod2_macos_ded \
  --data /Users/stix/Games/CoD2 \
  --output '/Users/stix/Library/Application Support/CoD2-native/ws81-stop-live7'
```

**All 15 corrected cases pass.** In addition to the original source, veto and
persistence cases, a single console line sets the driver request and queues
`wait 10; map mp_carentan` before script map `mp_toujane`. StopGameType permits
the script A preflight, then switches to integer-zero veto for B. B invokes its
own callback and never initializes; tagged A initializes once without a second
callback. The actual callback then deliberately calls `randomint(0)` in
developer mode. The engine reports its runtime ERR_DROP, completes server
shutdown and prints `"sv_running" is: "0^7"`. The old engine quits with exit 0;
a fresh engine executes the callback and vetoes a new map request; final integer
zero shutdown is ignored once and exits 0. `run.json`, `console.log` and
`receipt.json` in `ws81-stop-live7` are the exact corrected live receipts.
Both processes exited and the shared slot was released to parent/WS88.

There is an existing same-process dedicated restart limit: after error teardown,
console `map` is unknown because shutdown removes operator commands. This is
verified at both `f295e8c` and `8f17eaf`: `SV_Shutdown` calls
`SV_RemoveOperatorCommands`; that function removes `map`; error cleanup invokes
`Com_ShutdownInternal`, which calls server shutdown and restart without operator
re-registration. No same-process fresh-map pass is claimed, and WS81 adds no
registration policy. The live fixture restarts the engine explicitly.

Correction attempts `ws81-stop-live4` through `live6` passed FIFO and error
teardown but uncovered fixture assumptions: same-process map availability,
the dvar diagnostic's `^7` color suffix, and a Python launch-variable shadow.
Those were corrected before `live7`; those attempts are not counted as complete
live passes. They ended with engine cleanup and their logs remain private.

Preliminary attempts are retained honestly: `ws81-stop-live1` used incorrect
CoD4 `setdvar` fixture spelling and failed to compile; corrected to CoD2
`setcvar/getcvar/getcvarint`. `ws81-stop-live2`, against an older seven-TU overlay,
loaded the real mod and passed console vetoes, then exposed the inherited
`map_restart(true)` boolean-literal bug as callback `(1,1,0,fast_restart)` instead
of persistent `(1,0,0,fast_restart)`. It shut down cleanly. WS80 owns that fix.
The old object set initially failed to link missing newer runtime/method/identity
symbols and is not the final acceptance binary. Neither preliminary attempt is
counted as final live success. Original precache/spawn warnings appeared in the
successful persistent restart; this receipt does not certify unrelated gameplay.

## Parent integration handoff

Apply the four C diffs and new private header while preserving WS79's nearby
sv_init registration hunks. Extracted spawn/shutdown fixtures now need the
`SV_Cod2xBeginMapChange`/`SV_Cod2xEndMapChange` definitions and callback seam;
the real engine resolves these through sv_ccmds/g_scr_main. Parent-owned
`host_lifecycle.{py,c}` and `run_host_protocol.py` were not edited. The five new
bounded fixture files and this report are the only additional repository files.
Parent supplied the compiler fixes for the default-source fail-fast fixture;
real boolean arguments also pass in the corrected live run. Parent still owns
the composed host/gameplay/ABI/regression gates and review/commit. Assets and
private logs stay outside the repository. No readiness activation or release
publication is implied by this unit.
