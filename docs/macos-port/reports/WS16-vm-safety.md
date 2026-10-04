# WS16 — script VM shutdown and game-state integrity

Date: 2026-10-03. Worktree: `~/Projects/cod2-native-wt/vm-safety`.
Branch: `port/vm-safety`, started from `port/main` at `5f69688` (merged first).

## Result

All three assigned problems are root-caused and fixed in the native build.
The sanitizer pass of the local scenario found further genuine defects on the
same paths and in the renderer, effects, network and sound code; those are
fixed too (tables below). The soak then found the defects that ended long
sessions: a client timeout after 200 s, level state carried from one map into
the next (anim script items, entity and client slots, constant strings, an
undersized client array), game strings never released, and script vectors
allocated twice per use. All are fixed, and the final soak (seven map loads
plus a `map_restart`, 13.5 minutes) quits with exit code 0.

| Problem | Root cause | Fixed in |
| --- | --- | --- |
| 1. Listen-server `quit` hangs after a player joined | `VM_Resume` wrote resumed threads' local ids into the wrong function frames; a later thread return resumed the caller under another thread's id, so its `waittill`/`endon` registrations landed on a thread that was already waiting | `5ec9874` |
| 2. Entity (trajectory) corruption | XAnim loader wrote 4 bytes past a simple-quaternion record into the previous bone's translation frames; dying players' skeletons produced translations near 1e30 and dropped weapons carried NaN `pos.trBase`/`trDelta` in snapshots. Three more animation-layout defects and four game-state defects were found on the same path | `84bf4aa`, `eb10b78`, `55f609d`, `6730875`, `9d7de6a` |
| 3. CoD2x `Sys_Quit` throws `NSInvalidArgumentException` | `MacPreferences_Synchronize` dereferenced the SDK's `kCFPreferencesCurrentApplication` CFStringRef and passed the string's class as the app id (affects stock and CoD2x builds). The CoD2x client also died earlier, at map load, on an overlapping `strcpy` | `30ddd38`, `57a7db4` |

Evidence for each problem, before and after:

- Spawn then quit (stock build): before, it hung in script cleanup. After, it
  exits 0 in 0.05–0.26 s.
- Full local sanitizer scenario (spawn, 60 s of play with deaths,
  `map_restart`, `devmap mp_carentan`, quit): exits 0 and quits in 0.23 s.
  ASan went from 31 distinct reports (7,763 total) to 8; the remaining 8 share
  one benign root cause (see "Remaining").
- Soak: before, sessions died after 200 s, and later on the fourth to seventh
  map load. After, the 13.5-minute soak and a nine-load map cycle both exit 0
  (see "Soak").
- CoD2x build: before, it died with SIGTRAP during map load and threw
  `NSInvalidArgumentException` in `Sys_Quit`. After, spawn, play 20 s and quit
  exits 0.

`sh tools/abi/check.sh build-macos/compile_commands.json build/ws16/abi`
reports **620 TUs; 0 errors; 0 mismatches; 0 new** and 0 renderer-table
mismatches. `python3 tests/fixes13/legacy.py --base 5f69688` reports
**27 changed source files; 10 legacy configurations; 0 mismatches**.
Every behaviour change is under `COD2_X64` or `COD2_APPLE_SDK`, and the
legacy branches are kept token-for-token. Several of these defects are
architecture-neutral reconstruction errors that also exist in upstream's i386
build (noted per fix); the plan's rules keep that build unchanged, so they are
fixed only on LP64 here.

No sibling worktree was touched, nothing was pushed, and no packages were
installed. Game data stayed read-only. Only processes this workstream started
were signalled.

## Method

- **Driver.** `ws16-drive.py` starts `cod2_macos` with a private
  `fs_homepath` (`~/Library/Application Support/CoD2-native-ws16`, abbreviated
  **H**; evidence in **E** = `H/evidence`) on its own `net_port`. It feeds
  console commands on stdin and joins through the stock menu responses
  (`cmd mr <serverid> <menu> ...`, menu indices read from `configstrings`).
  It plays by cycling movement/fire/reload and `kill`, and waits for each
  `Going from CS_PRIMED to CS_ACTIVE`. It records the exit code and quit time.
  `lldb` and `sample` cannot attach here (task_for_pid is refused), so on a hang
  the driver sends SIGSEGV to its own child and reads the engine's
  symbolized crash report.
- **Sanitizers.** `build-ws16-asan` uses
  `-fsanitize=address,undefined -fsanitize-recover=address`. ASan's in-process
  `atos -p` symbolizer also hangs without task_for_pid, so runs use
  `symbolize=0:print_module_map=1`. `ws16-symbolize.py` resolves frames
  offline with `atos -o`, and `ws16-findings.py` groups reports by kind and top
  frames.
- **Invariant checker.** `E/ws16-instrumentation.patch` (applies to this
  branch's HEAD; not committed) adds a script-variable checker. It covers the
  free list, slot mapping, hash collision chains, every object's sibling list
  (termination, prev links, single ownership) and every notify list (each
  listed thread must be a live notify thread waiting on that string). It runs
  around notify, resume, entity free and thread entry points, or after every
  opcode with `WS16_VERIFY=2`. The patch also adds a per-frame entity scan
  (trajectory types, entity numbers, NaN trajectory/origin).
- **References.** cod2engine's Linux 1.3 server (`research-clones/cod2engine`)
  was read for behaviour; disagreements were checked against the Mac 1.3
  binary with `otool -tV` and `nm -a`.

## 1. Shutdown hang: resumed threads ran in the wrong frame

**Reproduction.** Even without joining, `devmap mp_toujane` then `quit` spun in
`Scr_ShutdownSystem -> Scr_CancelNotifyList -> VM_TrimStack -> Scr_KillThread`.
After joining, it spun at the WS11 site `Scr_FreeEntityList -> ... ->
Scr_KillThread` (`E/base-spawnquit.log`, `E/verify1.log`).

**Diagnosis.** The pool was consistent until shutdown, but the notify-list
invariant failed on server frame 3 after a `WaitTill` opcode. Owner 19826's
`"begin"` list contained thread 406, whose status said it was waiting on
`"connecting"` (`E/verify5.log`). The per-opcode trace showed thread 406
executing a second `waittill` while still suspended. A frame dump
(`E/verify6.log`) explains why:

```text
[ws16] resume start=2501 deepest=2501 count=1 frames: [0]=2501 [1]=406
[ws16] thread-return from 15160: restore frame[1] localId=406
[ws16] waittill owner=5 str=864(connecting) local=406 start=406 startstatus=0x36670
```

In 1.3 (`VM_Execute` and `VM_UnarchiveStack`), a thread owns function frames
`1..function_count`, deepest last; frame 0 is its caller. The reconstructed
`VM_Resume` assigned local ids to frames `0..function_count-1`, so the deepest
frame kept a stale id from an earlier thread. Resumed thread 2501 started
thread 15160, which suspended. The thread return then restored 2501's code
from frame 1, running under id 406. That stale-id code registered waits and
endons with the wrong self and thread, so a thread ended up in two notify
lists. It was later killed and freed while still listed. The shutdown cancel
loops then walked freed and recycled variables forever.

The suspected `SCR_ARENA_*`/`SCR_CODEPOS_*` encodings were checked by the same
verifier and were not involved.

**Fix** (`5ec9874`).
- `src/PC/script/scr_vm.c:8166`: under `COD2_X64`, `VM_Resume` follows
  `VM_UnarchiveStack`. It assigns frames `function_count..1` and rebuilds each
  caller frame's and the current frame's local-variable cache from its local
  object.
- `scr_vm.c:587`: `VM_ArchiveStack` releases the full `localVarCount`, as 1.3
  does, instead of the reconstruction's partial "cache count". The upstream
  local-variable archive slots, which only fed the shifted resume, are no
  longer used on LP64.
- This is an architecture-neutral reconstruction error; the legacy branch is
  unchanged.

**Evidence.** With the fix and `WS16_VERIFY=2` (every opcode), join, 10 s and
quit raised no invariant failure (`E/verify7.log`). Spawn-quit and
no-join-quit both exit 0. The `tests/lp64/script` VM semantics (15 cases) and
compiler checks still pass. `run.py` stops earlier at `bindings_test`, which
fails to compile against the unmodified `g_scr_main_mp.c` declarations; that
failure is pre-existing.

## 2. Entity and game-state integrity

WS9's retail `ERR_DROP` in `BG_EvaluateTrajectory` never fired in any run, and
the per-frame scan found no invalid trajectory type in any entity. It did find
entities with NaN `pos.trBase`/`trDelta`/`currentOrigin` in snapshots
(UBSan: NaN-to-int in `MSG_WriteDeltaField`). These were always the weapons
dropped when a player died. The chain was traced back writer by writer:

| Stage | Observation | Root cause and fix |
| --- | --- | --- |
| `kill` | SIGSEGV in `BG_FirstValidAnimScriptItem`, with `bgs=0x2ac6` | `src/PC/game_mp/g_combat_mp.c:640`: the reconstruction wrote `level_bgs.time` through the `bgs` slot. Mac 1.3 (and the 1.3 server) set `bgs = &level_bgs` (`6730875`). |
| Killed callback | `dm.gsc:371` log line concatenated string 0, giving a NULL `strcpy` | `src/PC/game_mp/g_main_mp.c:717`: `G_ParseHitLocDmgTable` was never called, so every `sHitLoc` was string 0 and all non-bullet location multipliers were 0.0. Mac 1.3 `G_InitGame` calls it, then `G_InitTurrets`, right after `SV_LocateGameData`. The reconstruction cleared turrets after spawning, releasing map turrets' `turretInfo` slots (`9d7de6a`). |
| Drop position | Tag translation read past a 3-row axis | `src/PC/game_mp/g_utils_mp.c:1297`, `:1358`, `:592`: `G_DObjGetWorldTagMatrix`, `G_DObjGetWorldTagPos` and `G_CalcTagParentAxis` kept the origin in a separate local, but `MatrixTransformVector43` reads row 3. They now use the 1.3 4×3 layout (`55f609d`). |
| View-model and player trees | `XAnimInfo[4096]` indexed with 11792 (UBSan) | `src/PC/xanim/dobj.c:918`: `animToModel` began 8 bytes into the tree (the i386 header size) rather than after `infoArray`, so string ids overwrote the last `infoArray` slots. `xanim.c:498` (and `XAnimFreeTree`) sized trees for the i386 header (`84bf4aa`). |
| Death blend | Bone quaternion about 1e-4, `transWeight` about 1.7e8 | `src/PC/xanim/xanim.c:1939`: in 1.3, a node with one weighted child passes its own weight down, and two or more children are blended in a buffer and normalized. The reconstruction scaled every child by its own weight (`eb10b78`). |
| Remaining garbage | Bone 60 local translation x = 1.2e30; `frames[0][0]` bits `0x72be38b4`, identical in every run | `src/PC/xanim/xanim_load_obj.c:417`: a single-frame simple quaternion was allocated at the i386 size 8. Natively `frame02` follows the pointer-aligned union (offset 8), so its two shorts landed past the block. The XAnim hunk grows downward, so they overwrote the first float of the previous bone's translation frames. 0x38b4 = 14516 and the approximate square-root component 0x72be fit the quaternion encoding. The missing-anim fallback (`xanim.c:538`) also copied a 72-byte native `XAnimParts` into a 0x2c block (`84bf4aa`). |

After these fixes, three 45 s play runs with four deaths each, run with the
verifier patch, reported no bad bone, no garbage translation, no NaN entity
and no out-of-range xanim index (`E/probe-quat1..3.log`). Before the loader
fix, the same scenario hit the garbage translation in two of three runs and
then in every run (`E/probe-blend1..3.log`, `E/probe-trans1..3.log`). A screenshot
(`H/main/screenshots/ws16-spawn.jpg`) shows the view weapon rendering
normally. Upstream's `TODO(root not found)` guard in
`XAnimCalcAccumulateTransLocal` described this corruption; it can probably be
removed after more testing.

**Entity 439.** The original upstream symptom was entity 439's `s.pos.trType`.
No trajectory-type corruption reproduced in this build: zero ERR_DROPs and
zero per-frame scan hits over all runs. The NaN-trajectory entities above were
the only entity corruption observed. Whether upstream's entity 439 had the
same writer cannot be proven from here. The XAnim overflow is LP64-only
(`Alloc(8)` is exact on i386), so on i386 that symptom must have another cause.
WS9's `COD2_PORT_DEBUG` probes remain available.

## 3. CoD2x `Sys_Quit` exception and the earlier SIGTRAP

- `src/Mac/Tools/MacPreferences.c:131`: under the SDK,
  `kCFPreferencesCurrentApplication` is the CFStringRef itself. The i386 import
  slot pointed at it. `*kCFPreferencesCurrentApplication` passed the string's
  isa (`__NSCFConstantString`) as the application id, which produced
  `+[__NSCFConstantString characterAtIndex:]: unrecognized selector`. This was
  reproduced in the stock build as well (`E/verify7.log`) and fixed in
  `30ddd38`.
- The CoD2x build first died with SIGTRAP (exit −5) about 2 s after start,
  with no crash report. A temporary SIGTRAP handler (`E/codx-trap.log`) showed
  `__chk_fail_overlap` in `__strcpy_chk`, called from `Info_RemoveKey <-
  Info_SetValueForKey <- Dvar_InfoString <- CL_CheckForResend`.
  `src/PC/universal/q_shared.c:455`, `:504`: key removal shifts the tail with
  an overlapping `strcpy`, which the fortified macOS libc traps. CoD2x re-sets
  its `cl_hwid2`/`protocol_cod2x` userinfo keys, so it always hits this path.
  Fixed with `memmove` in `57a7db4`.
- WS10's `cod2x_native_macos.m` has no CFPreferences or CFString use; it is not
  involved.
- Result: `build-ws16-codx` (`-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1`) spawns,
  plays and quits with exit 0 (`E/codx-spawnquit3.log`). On the final tree it
  also joins, plays, changes map to mp_carentan, rejoins and quits with exit 0
  in 0.32 s (`E/codx-final.log`).

## Other sanitizer findings fixed

| Finding | Root cause | Commit |
| --- | --- | --- |
| Heap overflow in `FxScheduler_PlayEffect` (88-byte write into an 80-byte `new[]`) | Delayed effects were allocated at the i386 size and linked by storing a truncated pointer in `mScheduledCount`. Mac 1.3 links at `mScheduledHead` and then increments the count, so delayed effects never replayed. `FX_AddScheduledEffects` used i386 template and record offsets. Fixed at `FxScheduler.c:463`, `:497`, `:902` and `FxUtil.c:459`; particle impact templates no longer truncate their pointer. | `0956332` |
| `memcpy-param-overlap` in `RB_Set2D` (every 2D draw) | `COD2_ALIGNED` is empty natively, so `GfxCodeMatrix` is 260 bytes, not 0x110. The retail-size copies overlapped, and offset 0x890 landed inside `shadowLookupMatrix` (`rb_backend.c:1990`) | `b9b5eae` |
| Global write into `outdoorGlob` | The static-model lighting pixel pointer was stored at `void*` index 4 (byte 32) of a 24-byte object, and cleared only by its low half (`r_staticmodel_load_obj.c:30`) | `b9b5eae` |
| Stack write in `NET_GetLoopPacket` | A 4-byte store at `port`, the last field of the native 20-byte `netadr_t` (`net_chan_mp.c:674`); `NET_StringToAdr` compared 10 bytes against `"loopback"` (`:395`) | `05857d9` |
| UBSan index 45 out of bounds for `HSAMPLE[8]` | 2D channels are numbered from 45; Mac `SND_Stop2DChannel` reads `0x55ead4 + 4*channel` (`milesGlob + 8` at 45). Every 2D call used memory past the table (`snd_driver.c:147`) | `f1b33a8` |
| Stack read in `DObjCreate` | `R_BeginRegistration` passed an i386 three-int `DObjModel_s` (`r_init.c:903`) | `de4b500` |
| Global read in `Image_Load` | Fixed-length `memcmp` against built-in names, reading past shorter names (`r_image_load_obj.c:785`) | `de4b500` |
| 4-byte writes into 1-byte globals | C++ `bool` flags (`g_NoTextureID`, `g_InhibitCopy`, `mNeeds*Validation`, …) were declared with the int-sized C `bool` (`CDirect3DDevice.c:27`, `CDirect3DSurface.c:8`, `rb_imagefilter.c`, `MacDebug.c`) | `5c301c6`, `de4b500` |
| Signed overflow in the `configure_mp.csv` checksum | Unsigned arithmetic, same bits (`common.c:1131`) | `bc91b23` |

## Found by the soak

Each soak attempt ended on the next defect in line. They are listed in the
order they were hit.

### Listen sessions dropped after about 200 s (`741ae0e`)

The first soak (`E/soak1.log`) dropped on its third map with `ERROR: Server
connection timed out`. Server-to-client loopback packets were still flowing,
and every `kill` after about 200 s of real time answered
`Unknown command "kill"`. Mac 1.3 `CL_Frame` (`0x14b2db`) times connected
states out on `cls.realtime - clc.lastPacketTime` (`clc+0x10`), and connecting
states on the same field against `cl_connectTimeout` (`0x14b3a7`). The
reconstruction used `connectTime`. A local map load sets it to −3000, so every
listen session hit `cl_timeout` (200 s) about 200 s after it started
(`src/PC/client_mp/cl_main_mp.c:2538`, `:2583`). The `connectResponse` handler
also had the two timestamps swapped (`:2261`): Mac `0x14cb01` stores
`realtime` to `lastPacketTime` and −9999 to `lastPacketSentTime`.

### Level state carried into the next map (`3d0f4d7`)

The second soak (`E/soak2.log`) failed on its fourth map load with
`BG_AnimParseAnimScript: exceeded maximum global items (2048)`, followed by
script memory exhaustion. Fixes in `src/PC/game_mp/g_main_mp.c` (native
`G_InitGame`), compared against Mac 1.3 `G_InitGame`:

- `:761`: `level_bgs.animScriptData` was never cleared on a new level, so its
  item count grew with every map. 1.3 clears it unless restarting.
- `:707`: 1.3 clears all 1024 entities and 64 clients before
  `SV_LocateGameData` (memsets of 0x8c000 and 0xa2900 bytes). The
  reconstruction did not, so stale entity slots kept string-field IDs that the
  previous level's `SL_ShutdownSystem(1)` had released. The next
  `Scr_SetString` on them dropped a reference the slot no longer owned.
- `:700`: `GScr_LoadConsts` ran after `G_SpawnEntitiesFromString`, so
  `G_InitGentity` used the previous level's `scr_const` IDs. 1.3 loads them
  first.
- `src/blobs/bss.c:452`: `g_clients` was a byte blob of 64 i386 records
  (665,856 bytes). The native `gclient_s` is larger, so the last client ran
  into neighbouring BSS, and the memset above could not cover it. It is now
  typed under `COD2_X64`.

With string reclamation enabled but before these fixes, a map cycle failed on
its fourth load with `duplicate case expression` in `_menus.gsc:134`
(`E/mapcycle2.log`): a stale ID referred to a string that had since been freed
and reused.

### Game strings never released (`c14d98a`)

`src/PC/script/scr_main.c:398`: Mac 1.3 `Scr_FreeScripts` (`0xaf24b`) always
calls `SL_ShutdownSystem(1)` and `Scr_ShutdownOpcodeLookup` and resets the
program state. The reconstruction did that only for `sys == 0`, which the
game's `Scr_FreeScripts(1)` never passes, so each level's game strings stayed
allocated into the next. The extra `Scr_FreeScripts(1)` in the middle of
`G_InitGame` (`g_main_mp.c:779`, not present in 1.3) would now release the
constants just loaded, so it is compiled only for legacy builds.

`src/PC/script/scr_stringlist.c:176`, `:194`: two LP64 workarounds in
`SL_RemoveRefToStringOfLen`, "never reclaim a string whose count reaches zero"
and a brute-force hash-chain scrub, were added upstream against
"already defined" and lost-name errors. Those errors came from the stale IDs
above. `COD2_X64` builds now reclaim through the original 1.3 unlink path;
other 64-bit configurations keep the workarounds.

### Script memory leak: vectors allocated twice (`8b4a192`)

With all of the above, map cycles still died on the fifth to seventh load with
`MT_AllocIndex: failed memory allocation of 16 bytes for script usage`, raised
from `maps/mp/_killtriggers.gsc` line 47 or 49 (`E/mapcycle1.log`,
`E/mapcycle3.log`, `E/squar2.log`). Temporary counters in the memory tree (`E/mtcount.log`)
showed nodes still in use at each `G_InitGame`: 4,714 after one level, 13,342
after two, 17,501 after three. Almost all were 16-byte nodes, the size of a
script vector.

- A census after `Scr_ShutdownSystem` (`E/mtsite2.log`) found 4,675 vector
  nodes with reference count 0 and no variable of vector type left in the
  pool, so the vectors had lost their owner rather than being held.
- Recording the allocating call chain and the executing script position
  (`E/mtsite3.log`, `E/mtpos.log`) put 3,366 of them on two `self.origin`
  reads in `_killtriggers.gsc:47` and `:49`, which run every frame for each
  player and kill trigger. The rest came from other field reads, vector
  builtins and compile-time vector constants.
- An alloc/release trace (`E/mtev.log`) showed each `self.origin` read
  allocating two vectors from the same `Scr_AddVector` call, at two different
  return addresses. The second was released by `Scr_EvalMinus` or
  `Scr_EvalArray`; the first was never referenced again.

The cause is the X64 `SCR_VEC_ENC_FROM` macro
(`src/headers/cod2_defs.h:6075`). It named its argument twice, once in the
arena range test and once in the encoding, and callers pass allocating
expressions: `SCR_VEC_ENC(Scr_AllocVector(value))` in `Scr_AddVector`
(`scr_vm.c:8626`), `Scr_CastVector` (`scr_variable.c:3134`) and the two
compile-time constant paths (`scr_compiler.c:450`, `:2927`). Every entity
vector field read, vector builtin result, `(x, y, z)` expression and vector
constant leaked one 16-byte node. The macro now binds its argument once with a
statement expression.

After the fix, script memory in use at each `G_InitGame` stays between 136 and
230 buckets over nine map loads and returns to 138 on the second visit to
toujane (`E/mapcycle4.log`, built with `E/ws16-mtcount.patch`). Before, the
same counter read 9,544, 26,766 and 35,072 buckets after the first three
levels.

## Sanitizer results

| Run | ASan distinct (reports) | UBSan distinct (misaligned) |
| --- | ---: | ---: |
| Spawn then quit, before fixes (`asan2-spawnquit`) | 29 (863) | 548 (515) |
| Full scenario, before (`asan-full1`), ended exit 1 | 31 (7,763) | 573 (540) |
| Full scenario, after (`asan-final2`), exit 0, quit 0.23 s | 8 (8) | 565 (540) |
| Full scenario, after the soak fixes (`asan-final3`, `741ae0e`), exit 0, quit 0.21 s | 8 (8) | 565 (540) |

The final run joined on toujane, played 60 s with 7 deaths, ran `map_restart`,
re-joined and played 15 s, then ran `devmap mp_carentan`, joined and played
15 s before quitting. The run used
`ASAN_OPTIONS=detect_leaks=0:symbolize=0:halt_on_error=0`,
`UBSAN_OPTIONS=print_stacktrace=1:symbolize=0:halt_on_error=0`. Logs and
grouped findings are `E/asan-final2.sym.log` and `E/asan-final2.findings.txt`.
The same scenario on the final tree (`E/asan-final3.findings.txt`) reports the
same 8 ASan and 565 UBSan sites; only the operand values of three
signed-overflow reports differ.

## Soak

**Final soak** (`E/soak3.log`, `E/soak3.json`; built from this branch at
`741ae0e`, without instrumentation). One listen server and its local client on
`net_port` 28990:

| Step | Map | Played |
| --- | --- | ---: |
| `devmap` (command line), join | mp_toujane | 100 s |
| `map_restart`, rejoin | mp_toujane | 60 s |
| `devmap`, join | mp_carentan | 100 s |
| `devmap`, join | mp_burgundy | 100 s |
| `devmap`, join | mp_matmata | 100 s |
| `devmap`, join | mp_dawnville | 100 s |
| `devmap`, join | mp_brecourt | 100 s |
| `devmap`, join | mp_toujane | 60 s |

Result: 8 of 8 `CS_ACTIVE` transitions and all 8 joins (24 menu responses).
The player died 64 times (`kill`) and moved, fired and reloaded throughout.
`quit` was sent at 808.6 s, and the process exited with **code 0** 0.14 s
later. The log has no script runtime or compile error, no memory-tree
failure, no timeout and no crash report. The only `ERROR:` lines are the two
`Couldn't find material 'american'` on mp_brecourt, which is stock behaviour:
`mp_brecourt.csv` sets `ui_campaign` to `american`, and Mac 1.3
`UI_MapLoadInfo` makes the same lookup.

**Map cycle** (`E/mapcycle4.log`): nine map loads of 15 s play each (toujane,
carentan, burgundy, matmata, dawnville, toujane, carentan, burgundy, matmata),
built with `E/ws16-mtcount.patch`. Exit 0, quit 0.08 s. Script memory in use
at each `G_InitGame`: 136, 230, 172, 158, 210, 138, 230, 172, 158 buckets.
The second pass over the same maps gives the same numbers (toujane 136, then
138), so nothing accumulates from level to level.

Before the soak fixes the same kind of run never finished: `E/soak1.log`
(client timeout at about 200 s), `E/soak2.log` (anim script items on the
fourth map), `E/mapcycle2.log` (`duplicate case expression`) and
`E/mapcycle1.log`/`E/mapcycle3.log` (script memory exhausted on the seventh
load).

## Remaining (not fixed here)

1. **`bg_numItems` placeholder (all 8 remaining ASan reports).** The committed
   prebuilt `build/native_gen/literals32.c` declares `bg_numItems`, `faceAxis`
   and `iSlotPreferenceOrder` as `unsigned char[1]`; `src/blobs/literals.S`
   has `.long 0`, `.space 72` and `.space 64`. The datagen therefore emits
   1-byte fallbacks, and the nine `*(int *)imp_bg_numItems` users read and
   write 4 bytes. Every generated object is `aligned(16)`, so production
   accesses stay inside padding: this is undefined behaviour, not live
   corruption. The proper fix belongs in WS2's generator (take these sizes from
   `literals.S`); it was left alone to avoid changing generated output for all
   native builds.
2. **UBSan, alignment class (540 sites).** Mostly renderer command buffers and
   packed XAnim/XModel data at 4-byte alignment. These accesses are safe on
   arm64 hardware and were triaged as a class, not fixed.
3. **UBSan, other (25 sites).** Signed wrap in hash/LCG/time arithmetic
   (`com_sndalias.c:90`, `com_memory.c:343`, `FxSystem.c:208`,
   `FxScheduler.c:479`, `sv_init_mp.c:955`, `rb_light.c:550`), `1 << 31` and
   negative shifts, the yacc `yyssp[-1]` idiom, and a flattened 2D table index
   in `cg_effects_load_obj.c:142`. All are value-correct under two's-complement
   wrapping. One worth following up: NaN-to-int in
   `Image_GetLightGridWeightsForVector` (`r_image_load_obj.c:481`), a lead for
   the known lighting artifacts.
4. **Legacy builds.** The architecture-neutral defects (VM_Resume frames,
   `player_die` bgs, the missing hit-location table and turret order, tag
   matrices, overlapping `strcpy`, 2D sound indexing) still exist in the i386
   configurations, which the plan keeps unchanged. The soak-path errors are in
   shared code too (client timeout field, the `Scr_FreeScripts` user test,
   the missing entity/client and anim-script clears, the `GScr_LoadConsts`
   order); they were not tested on i386.
5. **`netchan_t` field names.** In `cod2_defs.h`, `incomingSequence` and
   `dropped` are swapped relative to the STABS (retail `dropped` is at 0x8 and
   `incomingSequence` at 0xc). `Netchan_Process` uses raw offsets that match
   retail, and only one debug print uses the names, so behaviour is correct.
   The header should still be corrected together with its assertions.
6. **Not verified.** Remote stock/CoD2x servers, rendering beyond the screenshots
   taken, turret maps (the turret-slot fix is from the Mac 1.3 call order, not a
   live turret test), and audible 2D sound after the handle fix (exercised, not
   listened to). The soak had one local player and no remote clients or bots,
   and ran 13.5 minutes and seven map loads (nine in the map cycle); longer
   sessions were not run. `r_fullscreen` is read-only at startup, so every run
   opened as desktop fullscreen; that is outside WS16.
7. **Other 64-bit configurations.** The non-`COD2_X64` 64-bit branch of
   `SCR_VEC_ENC_FROM` (`cod2_defs.h:6081`, and the copy in
   `common_types.h:9174`) still names its argument twice, and those
   configurations keep the string-list workarounds. No `COD2_X64` build uses
   them, so they were left alone under the plan's guard rule. They should be
   fixed the same way if such a configuration is revived.

## Reproduce

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos -j12 --target cod2_macos
F="-fsanitize=address,undefined -fsanitize-recover=address -fno-omit-frame-pointer"
cmake -S . -B build-ws16-asan -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  "-DCMAKE_C_FLAGS=$F" "-DCMAKE_CXX_FLAGS=$F" "-DCMAKE_OBJC_FLAGS=$F"
cmake --build build-ws16-asan -j12 --target cod2_macos
cmake -S . -B build-ws16-codx -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-ws16-codx -j12 --target cod2_macos

E="$HOME/Library/Application Support/CoD2-native-ws16/evidence"
python3 "$E/ws16-drive.py" spawnquit "$PWD/build-macos/cod2_macos" spawnquit
ASAN_OPTIONS=detect_leaks=0:symbolize=0:halt_on_error=0 \
UBSAN_OPTIONS=print_stacktrace=1:symbolize=0:halt_on_error=0 \
WS16_PORT=28991 python3 "$E/ws16-drive.py" asan "$PWD/build-ws16-asan/cod2_macos" full
python3 "$E/ws16-symbolize.py" "$E/asan.log" > "$E/asan.sym.log"
python3 "$E/ws16-findings.py" "$E/asan.sym.log"
WS16_PORT=28992 python3 "$E/ws16-drive.py" codx "$PWD/build-ws16-codx/cod2_macos" spawnquit
# Soak: seven map loads plus a map_restart, about 13.5 minutes, then quit.
SC='["active","join","play 100","send map_restart","active","join","play 60",
"send devmap mp_carentan","active","join","play 100","send devmap mp_burgundy","active",
"join","play 100","send devmap mp_matmata","active","join","play 100",
"send devmap mp_dawnville","active","join","play 100","send devmap mp_brecourt","active",
"join","play 100","send devmap mp_toujane","active","join","play 60","quit 60"]'
python3 "$E/ws16-drive.py" soak "$PWD/build-macos/cod2_macos" "$SC"

# Map cycle with the script-memory counter (not committed; revert afterwards).
git apply "$E/ws16-mtcount.patch" && cmake --build build-macos -j12 --target cod2_macos
SC=$(python3 -c 'import json; m=["mp_carentan","mp_burgundy","mp_matmata","mp_dawnville",
"mp_toujane","mp_carentan","mp_burgundy","mp_matmata"]; s=["active","join","play 15"]
for x in m: s+=["send devmap "+x,"active","join","play 15"]
print(json.dumps(s+["quit 60"]))')
python3 "$E/ws16-drive.py" mapcycle "$PWD/build-macos/cod2_macos" "$SC"
grep '\[ws16-mt\]' "$E/mapcycle.log"
git apply -R "$E/ws16-mtcount.patch"

# Optional verifier (not committed): apply, build, run with WS16_VERIFY=1 or 2.
git apply "$E/ws16-instrumentation.patch"

sh tools/abi/check.sh build-macos/compile_commands.json build/ws16/abi
python3 tests/fixes13/legacy.py --base 5f69688
python3 tests/lp64/script/vm_semantics.py && python3 tests/lp64/script/compiler_checks.py
```

`ws16-drive.py` writes `E/<label>.log` and `E/<label>.json` (exit code, quit
time, hang flag, sanitizer counts). Each run uses its own `WS16_PORT`
(default 28990).

## Commits

`5ec9874` script VM resume frames · `30ddd38` CFPreferences app id ·
`5c301c6` C++ bool flags · `bc91b23` configure checksum · `6730875` player_die
bgs · `9d7de6a` hit-location table and turret order · `84bf4aa` XAnim record
and tree layout · `eb10b78` XAnim 1.3 blending · `55f609d` tag matrices ·
`0956332` scheduled effects · `05857d9` loopback/net compares · `b9b5eae`
renderer writes · `de4b500` renderer reads · `f1b33a8` 2D sound handles ·
`57a7db4` Info_RemoveKey overlap · `3d0f4d7` level reset before spawning ·
`c14d98a` game strings released per level · `8b4a192` vector encoding
evaluated once · `741ae0e` client timeout on `lastPacketTime` · then this
report.
