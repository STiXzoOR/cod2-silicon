# Game/script/server prototype audit notes

Companion inventory for [WS12-abi-audit.md](WS12-abi-audit.md). This is the game/script/server prototype unit. All edits are in the assigned worktree;
no commits, remotes, sibling worktrees, packages, or game data were touched by this unit.
The root agent owns the combined report and commits.

## Counts and scope

- Initial whole-tree AST inventory: **65 mismatch rows** with declarations in this unit.
- **64 true mismatches fixed** behind `COD2_X64`; **1 alias false positive**:
  `g_spawn_mp.c` declares `Scr_GetEntityRef` returning `scr_entref_t`, which is exactly
  `struct scr_entref_t`. The old audit expanded a typedef's own struct tag repeatedly.
- **62 native return-expression fixes** required after correcting real callees to `void`.
  These reconstructed callbacks declare integer results, but their callback consumers
  discard the result. Native code now executes the void call and returns zero;
  original `return Scr_Add*()`/`return CalculateRanks()` expressions remain in `#else`.
- **1 implicit/unprototyped variadic caller fixed**: `g_active_mp.c:111` now declares
  `Com_Printf(const char *, ...)` under `COD2_X64` before its diagnostic call.
- **3 native qsort declarations** use `<stdlib.h>`: `scr_compiler.c:51`,
  `scr_debugger_ui_lists.c:38`, `scr_debugger_ui_watch.c:39`.
- **2 comparator signatures** now exactly match qsort on native:
  `g_main_mp.c:385` (`SortRanks`) and `scr_compiler.c:256` (`CompareCaseInfo`).
  Native call sites are `g_main_mp.c:459` and `scr_compiler.c:5096`; original casts
  and original comparator definitions are preserved in the legacy branches.
- **24 source files changed**. The import-storage worker's existing guarded
  `G_RadiusDamage` vec3-origin fix in `g_combat_mp.c` was retained.

## Cross-TU fixes (native declaration lines)

| File | Function and line(s) |
| --- | --- |
| `src/PC/game_mp/g_client_fields_mp.c` | `CalculateRanks` 36; `Scr_AddConstString` 9; `Scr_AddFloat` 17; `Scr_AddInt` 22; `Scr_AddString` 27 |
| `src/PC/game_mp/g_client_mp.c` | `CalculateRanks` 18; `G_EntUnlink` 36; `Scr_AddString` 25 |
| `src/PC/game_mp/g_client_script_cmd_mp.c` | `GScr_AddEntity` 148; `Scr_AddBool` 105; `Scr_AddConstString` 110; `Scr_AddFloat` 89; `Scr_AddInt` 84; `Scr_AddString` 100; `Scr_AddVector` 94 |
| `src/PC/game_mp/g_cmds_mp.c` | `G_PrintEntities` 85; `Scr_AddString` 51 |
| `src/PC/game_mp/g_combat_mp.c` | `Com_GetServerDObj` 41; `ParseConfigStringToStruct` 18; `Scr_AddInt` 31; `Scr_PlayerKilled` 47 |
| `src/PC/game_mp/g_hudelem_mp.c` | `Scr_AddFloat` 68; `Scr_AddString` 175; `Scr_AddVector` 83 |
| `src/PC/game_mp/g_items_mp.c` | `G_SetConstString` 24; `SV_LinkEntity` 42; `Scr_AddUndefined` 97 |
| `src/PC/game_mp/g_main_mp.c` | `CheckTeamStatus` 569,827; `Com_ServerDObjCreate` 142; `GScr_FreeScripts` 169; `Scr_InitSystem` 543,801; `Scr_LoadGameType` 553,811; `Scr_LoadLevel` 548,806; `Scr_StartupGameType` 558,816; `XAnimFreeTree` 175 |
| `src/PC/game_mp/g_misc_mp.c` | `DObjSetControlTagAngles` 13 |
| `src/PC/game_mp/g_missile_mp.c` | `FX_GetEffectLength` 56; `FX_RegisterEffect` 55 |
| `src/PC/game_mp/g_scr_main_mp.c` | `Scr_AddArray` 32; `Scr_AddBool` 72; `Scr_AddConstString` 176; `Scr_AddFloat` 67; `Scr_AddInt` 27; `Scr_AddString` 22; `Scr_GetAnim` 78; `Scr_MakeArray` 37; `Scr_ParamError` 88 |
| `src/PC/game_mp/g_spawn_mp.c` | `Scr_AddArray` 90; `Scr_AddConstString` 59; `Scr_AddFloat` 48; `Scr_AddString` 43; `Scr_AddVector` 53; `Scr_MakeArray` 85 |
| `src/PC/game_mp/player_use_mp.c` | `G_DObjGetWorldTagPos` 19 |
| `src/PC/script/scr_animtree.c` | `XAnimPrecache` 80 |
| `src/PC/script/scr_compiler.c` | `Scr_GetFunction` 67; `Scr_GetMethod` 72 |
| `src/PC/script/scr_main.c` | `Scr_EvalVariable` 27; `___maskrune` 6 |
| `src/PC/script/scr_vm.c` | `Scr_CompileShutdown` 289 |
| `src/PC/server_mp/sv_snapshot_mp.c` | `MSG_ReadDeltaArchivedEntity` 131; `MSG_ReadDeltaClient` 125 |
| `src/PC/server_mp/sv_world_mp.c` | `CM_BoxTrace` 15 |
| `src/PC/xanim/xanim.c` | `Scr_AddConstString` 17 |

## Additional call/definition corrections

- `g_combat_mp.c:602`: retain the pointer returned by `Com_GetServerDObj` in
  `struct DObj_s *`, instead of truncating it to `int`.
- `g_combat_mp.c:173`: use the actual `ParseConfigStringToStruct` destination/field
  pointer types and a null callback. The local field struct has the same
  pointer/int/int layout as `cspField_t`.
- `g_combat_mp.c:687`: pass `animResult` as the previously omitted tenth argument
  (`deathAnimDuration`) to `Scr_PlayerKilled`.
- `g_scr_main_mp.c:881`: a native helper consumes `Scr_GetAnim`'s `scr_anim_t`
  return and packs its `index`/`tree` fields explicitly. Four callers at
  lines 891, 906, 1814, 1835 retain their original integer representation.
- `scr_compiler.c`: `Scr_GetFunction`/`Scr_GetMethod` declarations now return the
  real function-pointer typedef; explicit native casts preserve the existing
  pointer-width integer representation of the builtin registry.
- `scr_main.c:27`: `Scr_EvalVariable` now declares its packed 64-bit result.
  The existing cast to `unsigned int` intentionally extracts the low value word;
  `scr_variable.c` stores the value type in the high word.
- `scr_vm.c:333,822`: native `Scr_InitSystem(int sys)` matches the Mac signature
  and existing caller argument. `sys` is unused by the reconstructed body, as it
  already is in `Scr_ShutdownSystem`; the legacy no-argument definition remains.
- `g_missile_mp.c:54,205`: widen the existing effect-pointer guards from
  Apple-dedicated-only to all `COD2_X64`, including the native client handle local.
- `sv_client_mp.c:102,608`: native `MSG_WriteBigString` declaration/call uses two
  arguments, coordinated with the root agent's definition fix.

## Native void-call return expressions

| File | Lines |
| --- | --- |
| `src/PC/game_mp/g_client_fields_mp.c` | 128, 142, 148, 154, 160, 197, 203, 209, 215, 255, 348, 354, 360, 366, 383, 399 |
| `src/PC/game_mp/g_client_script_cmd_mp.c` | 362, 388, 429, 483, 487, 520, 534, 556, 570, 1027, 1163, 1171, 1179, 1185, 1202, 1211, 1219, 1225, 1572, 1581, 1587 |
| `src/PC/game_mp/g_main_mp.c` | 373 |
| `src/PC/game_mp/g_scr_main_mp.c` | 694, 703, 712, 721, 730, 794, 824, 833, 842, 929, 1117, 1154, 1584, 2006, 2112, 2564, 3786, 3801, 3816, 3831, 4072, 4298, 4303, 4369 |

## Mac evidence

Read-only commands:

```sh
nm -n ~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386
# Filter for Scr_GetAnim, Scr_PlayerKilled, XAnimFreeTree, Scr_InitSystem.
otool -tv ~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386
# Inspect player_die (__Z10player_die...) near 0x1c4b90.
```

The Mac mangled names confirm `Scr_GetAnim(unsigned, XAnimTree*)` at 0x874a4,
`XAnimFreeTree(XAnimTree*, void(*)(void*,int))` at 0x3bcf4,
`Scr_InitSystem(int)` at 0x815d0, and the final two integer parameters of
`Scr_PlayerKilled` at 0x19d476. In `player_die`, the animation callback's result
is moved from EAX to argument ten (`0x24(%esp)`) at **0x1c4b95**, immediately before
the call to `Scr_PlayerKilled` at **0x1c4bd7**. The reconstructed body already
stores that result in `animResult`.

`common_types.h` defines `scr_anim_t` as two unsigned-short fields, `index` then
`tree`, and `Scr_GetAnim` fills those fields from the packed script value.
`Scr_EvalVariable_core` returns `(type << 32) | value`, so extracting its low
32 bits in `Scr_GetFunctionHandle` remains appropriate.

## Verification

```sh
python3 tools/abi/audit.py build-abi/game-compile_commands.json \
  --output build-abi/game-abi.json --jobs 2
```

Result: **48 TUs; 0 errors; 0 mismatches; 0 new**. The filtered compile database
contains each changed source's client and dedicated command from
`build-abi/compile_commands.json` (native `COD2_X64=1`, arm64). It is ignored
verification output, not a committed artifact; regenerate it by filtering the
full native compile database to the files in the tables above.

The same **48 compile commands** were individually rerun after replacing their
`-c/-o` arguments with `-fsyntax-only`: **48 successful, 0 errors**. Existing
reconstruction warnings remain; this unit does not claim warning-free compilation.

For each command, remove `-DCOD2_X64=1`, preprocess original and modified source
bytes with `-E -P -x c -` plus the source directory include path, and compare
stdout. **48 of 48 preprocessed outputs were byte-identical**. Original source
snapshots are local ignored output at `build-abi/game-prototype-originals.json`.
This comparison is stronger than just reviewing guards, but is not an i386
object/binary comparison; this machine has no runnable i386 target.

## Remaining / limitations

- The combined all-tree audit/build is the root agent's verification. This unit
  ran only its own source commands and did not perform a whole build or launch.
- `COD2_FEATURE_SCRIPT_DEBUGGER=1` is outside the default AST inventory. A
  syntax probe of `scr_debugger_ui_watch.c` with that feature enabled succeeds,
  but exposes numerous preexisting pointer/int truncations and unprototyped
  debugger callbacks. Its qsort declaration is fixed; its entire optional
  debugger path is **not ABI-certified**. Extend the audit to feature-on
  compile commands before enabling that feature in native gameplay.
- Native game callback return placeholders remain integer-return declarations
  because changing all callback tables/consumers is a separate contract change.
  Actual mismatched void callees no longer pretend to return integers.
- Apart from qsort, the assigned default function-cast inventory's direct
  signatures use integer/pointer parameters only (including the G_CreateDObj,
  sound, allocation, and client command callbacks). No float/double or variadic
  register-class disagreement was found in that inventory. The broader export
  tables and callback dispatch audit belongs to the root agent.
