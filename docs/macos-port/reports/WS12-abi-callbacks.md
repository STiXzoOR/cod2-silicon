# WS12 callback and vtable audit notes

Companion inventory for [WS12-abi-audit.md](WS12-abi-audit.md). All source fixes
below preserve the legacy branch and select the corrected contract only with
`COD2_X64`. No objects, binary data, or reference dumps are tracked.

## Tool and coverage

`callback_tables.py` takes Clang's renderer record fields and the function
definition/call/cast inventory produced by `audit.py`. Source matching identifies
the `RE(offset, symbol)` and `ri_local.field = symbol` bindings only; Clang
supplies the C types. It checks every native renderer binding even where a
`void *` assignment erased type checking. Named function and function-pointer
field casts are checked separately. Unprototyped indirect calls with floating
arguments fail the gate. The JSON contains the full field/binding/cast inventory.

Rerun against the complete current audit, rather than an older source cache:

```sh
python3 tools/abi/callback_tables.py build-abi/compile_commands.json \
  --audit build-abi/verified/functions.json --output build-abi/callback-tables.json \
  --baseline tools/abi/callback-baseline.json
```

For an audit still running, `--cache build-abi/audit-cache` uses the most recently
written summary for each source path. The renderer pass reports 88 export
fields, 136 import fields, 218 callable bindings, 218 compatible bindings, and
zero table mismatches. The first pre-desugaring snapshot had 1,551 unique
indirect calls and zero unprototyped floating indirect calls. Final cast counts
must come from the parent's final AST JSON: it adds function-pointer members,
desugared typedef types, and explicit generic-storage initializer metadata.

There are 425 C sources and 390 unique native compile database source paths.
Whole-source searches found 41 `qsort(` occurrences in 15 sources, 342 simple
function-pointer-cast candidates in 62 sources, and 462 vtable-related words in
45 sources before this work. These include declarations and inactive branches;
they are search coverage counts, not semantic findings. Native compile database
exclusions are separately listed by `excluded.py`.

## Renderer table contracts fixed

All changes are local field guards in
`src/headers/PC/universal/com_math.h`. The 19 changed field contracts consist of
seven concrete signature mismatches, including the unbound Hunk_ShowTempMemory
slot, and twelve unprototyped declarations, including unbound Hunk_HideTempMemory.

| Field | Header line | Native truth and definition evidence |
|---|---:|---|
| Shutdown | 105 | `void(qboolean)`; r_init.c:567; native callers already pass 1/0 |
| InterpretSunLightParseParams | 131 | `void(SunLightParseParams *)`; r_bsp.c:80 |
| DrawStretchRaw | 152 | nine arguments; r_rendercmds.c:736 |
| DrawText | 188 | reconstruction returns int; r_font.c:170 |
| DrawConsoleText | 195 | reconstruction returns int; r_font.c:439 |
| DuplicateFont | 205 | reconstruction returns int; r_font.c:112 |
| GpuWaited | 215 | `void(int ticks)`; rb_backend.c:654 |
| Milliseconds | 226 | `int(void)`; ri_local_Milliseconds, cl_main_mp.c:833 |
| Hunk_ClearTempMemory | 241 | `void(void)`; com_memory.c:795 |
| Hunk_HideTempMemory | 242 | `int(void)`; com_memory.c:803; currently unbound |
| Hunk_ShowTempMemory | 243 | `void(int mark)`; com_memory.c:812; currently unbound |
| Hunk_ClearTempMemoryHigh | 251 | `void(void)`; com_memory.c:672 |
| Sys_DirectXFatalError | 252 | `void(void)`; mac_main.c:111 |
| Sys_ShowSplashWindow | 253 | `void(void)`; mac_splash.c:16 |
| Sys_HideSplashWindow | 254 | `void(void)`; mac_splash.c:22 |
| Sys_LoadingKeepAlive | 255 | `void(void)`; mac_main.c:258 |
| Cmd_Argc | 306 | `int(void)`; cmd.c:238 |
| CL_UpdateDebugData | 317 | `void(void)`; cl_main_mp.c:1354 |
| CM_BoxTrace | 336 | reconstruction returns int; cm_trace.c:564 |

The Mac STABS records for R_DrawText, R_DrawConsoleText, and R_DuplicateFont
return void, whereas these reconstructed definitions return int. Native table
contracts match the compiled definitions. R_DrawText's native implementation now
calls its void drawing function directly and returns zero rather than reading a
nonexistent integer result. CM_BoxTrace likewise follows its reconstructed int
definition. Changing all reconstructed return contracts to the historical void
type is unnecessary for native calling convention correctness.

## Callback and comparator fixes

| Location | Change and evidence |
|---|---|
| cg_effects_load_obj.c:43,74 | Native qsort comparator accepts two `const void *`, dereferences full-width string pointers, and uses `sizeof(pszFiles[0])`; previous int dereferences and four-byte element stride truncated native pointers. |
| cl_scrn_mp.c:44,121 | Native DrawText/DrawConsoleText cast typedefs match current int return types; console font argument is FontHandle, not int. Slot 0x128 binds R_DrawConsoleText. |
| FxUtil.c:169,193,843 | CompareSortedEffects compares full-width material addresses, uses the typed distSq field, returns zero on equal distance, and qsort uses the native SortedEffect element size (16 rather than 8). The separate SortedCluster stride 8 remains correct. |
| cg_consolecmds_mp.c:39,67; cg_weapons.c:176,1519 | Native CG_WeaponSlot_f is `void(void)` and its command table has no signature-erasing cast. Historical extra arguments were unused. Mac symbol `__Z15CG_WeaponSlot_fv` verifies no arguments. |
| FxPrimitives.c:143,2528 | Native Particle_Update takes only the Particle this pointer. The unused two extra pointer parameters remain in the legacy branch. Mac symbol `__ZN8Particle6UpdateEv` verifies a this-only method. |
| bg_weapons_load_obj.c:400 | Native ParseConfigStringToStruct callback contract is void and SetConfigString2 is passed directly, matching q_shared.c:893. Legacy long-return cast remains untouched. |
| r_font.c:173 | Native R_DrawText calls the void R_AddCmdDrawTextWithCursor directly and returns zero. The legacy int-return function cast remains untouched. |
| ui_shared_mp.c:153,4213,4226,4257,5544 | Native captureFunc retains the actual `void(displayContextDef_t *, void *)` contract for all three scroll callbacks and their dispatcher. Previously assignments cast all three to `void(void)` and the dispatcher cast back. |
| g_main_mp.c:380,657 | Native G_CreateDObjCallback matches bgs.CreateDObj's void/unsigned-short contract and deliberately discards G_CreateDObj's reconstructed int result. No incompatible named-function cast remains. |
| r_jpeg.c:103,114,127 | Native allocation/free/error adapters use the typed ri.Z_MallocInternal(int), ri.Z_FreeInternal(void *), and variadic ri.Error fields. This also removes their ILP32 ri byte offsets; JPEG structure layout remains unresolved below. |
| bg_pmove.c:90,449,741; headers/PC/bgame/bg_funcs.h:76 | Native PM_UpdateLean accepts the seven-argument pmove trace callback type and receives it directly, instead of a void(void) cast. The existing lean implementation does not call the callback and remains incomplete. |
| rb_backend.c:2521,2531 | Native RB_DrawTextInSpace uses the actual `unsigned int(const char **, qboolean *)` text decoder callback instead of a second-parameter int cast. The parent repaired the same contract in RB_DrawTextWithCursor. |
| cod2_defs.h:8013; r_jpeg.c:162 | Native jpeg_destination_mgr's three callback fields take j_compress_ptr and receive direct native assignments, matching init_destination, empty_output_buffer, and term_destination. All three formerly cast to void(void)/boolean(void). The duplicate in common_types.h is inactive under #if0 and remains untouched. |
| CVAOPacket.c:161,497,527,662 | Native insert_equal takes a tree and key/value pair and returns a one-pointer iterator; native _M_erase takes a tree and node. Native callers use direct calls instead of casting void(void) stubs to two-argument functions. These remain empty reconstruction stubs; the null iterator return is deliberate. Mac STABS records the iterator as type (0,156)=s4 with one _M_node, insert_equal FUN returns (0,156), and _M_erase FUN returns void (0,1). |

The parent also repaired Speex destroy wrappers that returned float while the
mode destroy callbacks return void, and erased ri.Printf calls in r_model.c.
Those belong in the aggregate report with its final line references.

## Callable placeholder aliases fixed (class 3 overlap)

`r_cmds.c:9-19` maps five native command callback names directly to existing
functions. Previously each name referred to a zero-initialized `char[64]` in
link_stubs.c and was declared as a function-pointer variable, so command
registration read a NULL pointer from the first native pointer-sized bytes.

| Placeholder | Native definition | Definition location |
|---|---|---|
| R_LoadSun_f | R_Cmd_LoadSun | r_sky.c:114 |
| R_SaveSun_f | R_Cmd_SaveSun | r_sky.c:142 |
| R_SmcStats_f | R_StaticModelCacheStats_f | r_staticmodelcache.c:40 |
| R_SmcFlush_f | R_StaticModelCacheFlush_f | r_staticmodelcache.c:195 |
| R_ReloadMaterialTextures_f | R_Cmd_ReloadMaterialTextures | r_material.c:946 |

All five names and their void/no-argument contract are corroborated by the
Mac 1.3 C++ symbols. The native aliases leave the legacy data symbols intact.

## Other reviewed callbacks and limitations

- Native Miles file callback typedefs at macos_audio.h:70-73 match the four
  MSS_File*Callback definitions at snd_driver.c:197,203,208,225 exactly:
  unsigned long filename/open handle, unsigned long close handle, long seek
  return/offset, and unsigned long read return/count. The legacy MacMSS_Object.c
  vtable and MacThreads.c ExecuteFn callbacks are excluded from the native
  source set and replaced by platform code.
- Native audio uses SDK-typed AudioFile and AURender callbacks at
  macos_audio.c:119,133,143,486. There are no native SDL audio callback
  registrations. SDL provides window/events; CoreAudio provides audio.
- This reconstruction has no separate ui/cg export vtable; UI/CG functions are
  directly linked. Their command and parser callback tables, including the
  weaponslot and UI scroll contracts, were inspected instead.
- The 127 indirect calls with float/double arguments in the AST snapshot were
  reviewed by subsystem. The only COM vtable calls with a by-value float are
  CDirect3DDevice::Clear at rb_backend.c:1097,3251; its definition at
  CDirect3DDevice.c:1550 and slot 43 take the same float Z/depth argument. Other
  float calls are typed renderer/UI/audio/Dvar contracts. Dynamic vtable object
  identity cannot generally be proven from this C reconstruction; the JSON
  inventory is not a runtime type proof for every raw `void **` slot.
- qsort pointer-spelling casts in compare_hudelems, iwdsort, etc. keep two pointer
  arguments and an int result, so they do not change register classes. Their
  element widths match the sorted arrays. Four-byte UI display indices,
  static-model indices, script case fields, and rank indices remain four bytes.

## Residual findings with reasons

1. **JPEG source/destination wrapper layout.** The three destination callback
   contracts are repaired; callback-baseline.json is empty. Native JPEG still uses ILP32
   cinfo offsets/sizes, a fixed byte buffer for the error structure in R_LoadJpg,
   and the jpeg_memory_src char[64] placeholder when modern libraries are off.
   Solving those requires a typed LP64 libjpeg wrapper; correcting only a
   callable placeholder would still permit structure corruption. Do not claim
   native JPEG load/save works. Native allocator/free/error adapter contracts
   are repaired independently.
2. **Incomplete script debugger reconstruction.**
   Scr_ScriptWatch_EvaluateWatchChildren at scr_debugger_ui_watch.c:935 passes
   comparator addresses 0x82297028/0x822A2370/0x82296EF8 into qsort. The function
   also discards its newly allocated array. These comparator implementations
   are missing; Mac 1.3 symbols provide no matching names. Script debugger
   features default off. Inventing comparators is not justified by the ABI
   audit and would not repair the incomplete function.
3. **Unbound renderer imports.** Five import slots are currently unbound in
   the native ri_local setup: Hunk_HideTempMemory, Hunk_ShowTempMemory,
   CM_RayTriangleIntersect, DB_EnumXAssets, DObjGetPartBits. The first two now
   match known definitions. The latter three need feature-specific setup if
   used; the Windows retail renderer adapter is their only setup path. Two
   missing function definitions (CM_RayTriangleIntersect, DObjGetPartBits)
   prevent replacing those unprototyped declarations with verified contracts.
4. **Generic storage and unprototyped casts.** Sixteen Mac DirectX sources store
   function pointers through `fnptr_t`, typedef'd to `void(*)(void)`. Those
   initializer conversions retain pointer addresses; callers recover a
   specific slot signature. They are classified as generic-storage rather than
   call mismatches, without baseline exceptions. Unprototyped pointer/integer
   dispatch remains in the JSON inventory. None is observed as an unprototyped
   indirect floating call in the native AST. The retained inventory does not
   prove dynamic vtable object identity at runtime.
5. **VAO cache container reconstruction.** CVAOPacket insert_equal and _M_erase
   retain empty reconstructed bodies, and _M_insert is likewise a stub. Native
   cache insertion/erasure is not implemented by the verified signature fixes.

## Verification and merge notes

- Syntax-only compilation using the actual compile_commands.json arguments
  passed for cg_effects_load_obj.c, cl_scrn_mp.c, FxUtil.c, r_cmds.c,
  cg_consolecmds_mp.c, cg_weapons.c, FxPrimitives.c, bg_weapons_load_obj.c,
  r_font.c, r_init.c, cl_main_mp.c, ui_shared_mp.c, g_main_mp.c, and r_jpeg.c.
  bg_pmove.c also passes syntax-only compilation after typing its trace callback.
  rb_backend.c passes syntax-only compilation after correcting the second text
  decoder callback.
  CVAOPacket.c and r_jpeg.c pass syntax-only compilation after the final
  container/destination-callback contract fixes.
  No object build was run by this child agent. Existing pointer/size warnings
  and JPEG's JSAMPARRAY pointer-spelling warning remain for the parent audit.
- `python3 -m py_compile tools/abi/callback_tables.py` succeeds.
- `python3 tools/abi/legacy.py --base 391870a` checked 98 changed C/header files in
  five COD2_X64-off configurations and found zero preprocessed body mismatches.
  This does not claim this Mac can build or execute i386 Linux/Windows binaries.
- `nm -n ~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386` verified the
  CG_WeaponSlot_f, Particle::Update, and five renderer command definitions.
- Root owns all commits, final build, and final report. Merge the small guarded
  com_math.h field edits carefully with sibling header work. No CMake change is
  needed from this callback work; tools/abi/check.sh invokes the callback gate.
  bg_funcs.h has one additional guarded prototype; no other bgame header changes
  belong to this callback work.
  cod2_defs.h has one additional guarded jpeg_destination_mgr field group;
  common_types.h remains untouched.
