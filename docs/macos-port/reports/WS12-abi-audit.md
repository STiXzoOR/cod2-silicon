# WS12 — native arm64 ABI audit

Branch: `port/abi-audit`. Baseline: `391870a`. Worktree:
`~/Projects/cod2-native-wt/abi-audit`. Verified on 2026-10-03 with
Apple Clang and the local reference paths in PLAN.md. No sibling worktrees,
remotes, packages, proprietary repository additions, pushes or PRs were used.

The native client builds, completes common initialization with the licensed
IWD index, and exits cleanly. The compiled-tree ABI gate has no remaining
declaration, renderer-binding, named/member-cast or proven import-load findings.
Both reviewed baseline files are empty. This establishes the checked contracts,
not complete gameplay or support for every reconstructed utility path.

## Counts and coverage

| Class | Scope and result |
| --- | --- |
| 1: external declarations | 623 Clang translation-unit commands, including client/dedicated variants; zero parse errors and zero final ABI mismatches. The first full snapshot contained 153 mismatch rows across 114 symbols: 114 return, 35 parameter-class, 15 arity and four variadic flags (flags overlap). One row was a recursive typedef false positive, fixed in the checker; the remaining rows were corrected. Six additional SDK qsort/malloc declaration sites were found after adding SDK contracts. The final source index contains 185 added native external declaration/definition locations. |
| 1: unprototyped calls | Zero direct unprototyped floating calls. Direct integer/pointer calls remain inventoried; the final snapshot contains 181 records before client/dedicated deduplication. A previously implicit Com_Printf caller now has the variadic contract. |
| 2: import loads | 514 import symbols, 1,439 lexical sites, and 1,775 preprocessed native sites. Zero proven extra or missing variable loads. Corrected one missing pointer-variable load and four pointer-array slot accesses. The existing g_snd/SND_GLOB_PTR repair was retained. |
| 3: storage | 185 zero-storage declarations, 183 distinct names, all classified in the import companion. Corrected 21 placeholder identities at 32 uses: 14 identities/19 uses in voice/water/screenshot/sky/damage, tr at three uses, listener origin at five calls, and five callable renderer aliases. |
| 4: dispatch | 88 renderer export fields, 136 import fields, 218 bindings, all compatible. Corrected 19 renderer field contracts, three JPEG destination fields and two VAO method contracts, plus the comparator/callback repairs listed below and in the callback companion. Zero final named/member cast mismatches and zero unprototyped indirect floating calls. Final inventory: 1,552 unique indirect call sites; 137 compatible, 389 generic-storage and 508 unprototyped named/member cast records. |

There are 425 C sources. The database has 390 unique paths: 384 project C
files, one Objective-C file, one C++ file and four generated typed-data/import/
literal files. The AST pass excludes the eight generated-file commands, leaving
623 project commands. The other 41 project C sources were separately
syntax-probed: 31 parsed and ten
could not use those native flags. Eight are bundled zlib files replaced by SDK
ZLIB and lack their Byte type in this probe; the original blob import-pointer
file conflicts on g_EndPos and is replaced by generated typed imports; the wasm
mount file needs the unavailable Emscripten header. The excluded-source table
below records every file. This is distinct from claiming those replacements
are active native definitions.

Optional features follow the supplied compile database. `-DCOD2_CODX=ON` was
reported unused by CMake; it did not enable CoD2x. The three excluded CoD2x
sources were separately probed with `-DCOD2_CODX=1`. Script debugger features
remain default-off and are not ABI-certified by the default gate.

## Reproducible verification

```sh
cmake -S . -B build-abi -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-abi --target cod2_macos -j6
sh tools/abi/check.sh build-abi/compile_commands.json build-abi/final
python3 tools/abi/legacy.py --base 391870a
python3 tools/abi/legacy.py --base 391870a \
  --compile-commands build-abi/compile_commands.json
python3 tools/abi/excluded.py build-abi/compile_commands.json \
  --output build-abi/excluded.json
git diff --check
```

The client build succeeds. The ABI script runs four real-Clang regression tests,
six import-pattern tests, and the three audit stages. The final AST and callback
stages report zero findings. Import-load checks report zero findings. Generated
JSON/cache/log files stay in the ignored local build directory; the committed
rerun documentation is [tools/abi/README.md](../../../tools/abi/README.md).

The legacy body check passes for all 98 changed C/header files in five
COD2_X64-off configurations. The additional SDK probe compares actual
preprocessing tokens, preserving strings and real __LINE__ values: 137 of 139
commands agree; two macos_compat.c configurations cannot preprocess because
GL/gl.h or SDL2/SDL.h is unavailable. That optional command correctly exits
nonzero for the two errors. No runtime-code difference was found in the
successful comparisons. The line guards preserve original out-of-memory
diagnostic values and offset-assert typedef names. No runnable i386 target or
Linux/Windows i386 SDK is available here, so actual object/executable byte
parity, including debug metadata, remains unverified. No packages were installed.

The final executable was launched with a 60-second process timeout:

```sh
./build-abi/cod2_macos \
  +set fs_basepath ~/Games/CoD2 \
  +set fs_homepath "$PWD/build-abi/smoke-home" \
  +set dedicated 1 +set net_port 28998 +quit
```

Exit status was 0. The log records 81,663 files in IWDs, a localhost socket on
28998, Common Initialization Complete and CL_Shutdown. This checks the client
executable's headless common startup; no menu, map or microphone test is claimed.
The licensed installation was read in place and was not added to the repository.

`cmake --build build-abi --target cod2_macos_ded -j6` does not link: typed imports
refer to renderer/client globals and methods absent from the dedicated source
set, including g_VAOID and CVAO methods. Its compiled units pass the declaration
audit. Dedicated linking is a separate source-set/import-generation follow-up;
this report does not establish whether that link failure predates this branch.

## Fix inventory and decisions

Every modified source region and added external contract is indexed with exact
final file/line references in [WS12-abi-locations.md](WS12-abi-locations.md).
The detailed game/script/server, import/storage and callback inventories are
[WS12-abi-game.md](WS12-abi-game.md),
[WS12-abi-imports.md](WS12-abi-imports.md) and
[WS12-abi-callbacks.md](WS12-abi-callbacks.md). Together these list every fix,
all 514 imports, and every zero-storage declaration with its use/null behavior.
Their older explicitly labelled snapshots may have earlier line numbers;
the source index and final generated JSON are authoritative.

Contracts generally follow the compiled definition. Mac C++ names, STABS and
targeted instruction reads resolve ambiguous reconstructed definitions. All
source behavior/prototype fixes select COD2_X64 and preserve the legacy branch.
Native calls now pass the required arguments instead of merely silencing casts.
The following covers root-owned call/storage corrections beyond the companion
inventories; the source index supplies each related declaration location.

| Source and corrected operation | Decision |
| --- | --- |
| EffectsCore/FxSystem.c, FxHelper_WarpTime and FxHelper_AddFxToScene | Pass two null vectors to FX_AddScheduledEffects; use the actual by-value GfxModel union for the scene call. |
| EffectsCore/FxPrimitives.c, FxScheduler_PlayEffect callers | Use the fixed five-argument contract and supply a null bolt argument. |
| EffectsCore/FxScheduler.c, helper assignment | Load *(FxHelper **)imp_theFxHelper, rather than the address held in the import slot. Mac theFxHelper is a pointer variable. |
| EffectsCore/FxPrimitives.c, Cloud_Cloud | Native constructor takes only this; the unused reconstructed second parameter is retained in the legacy branch. Mac __ZN5CloudC1Ev confirms the contract. |
| cgame_mp/cg_predict_mp.c, view-angle update | Pass the required float msec input as 0.0f, followed by cmd and final integer. |
| cgame_mp/cg_ents_mp.c, goal weights/effects | Supply rootIndex zero before animIndex; call FX_PlayEffect through its actual fixed three-argument contract. |
| cgame_mp/cg_view_mp.c, effect local | Preserve the returned EffectTemplate pointer rather than truncating it to int. |
| cgame_mp/cg_draw_mp.c, flag-removal return | Execute the actual void function and return zero from the reconstructed integer wrapper. |
| cgame_mp/cg_shellshock.c, five sound calls | Use imp_vec3_origin as the origin address; the old integer placeholder supplied NULL. Keep the real pointer/int sound contracts. |
| qcommon/files.c, filtered-file listing | Insert the required FS_LIST_ALL behavior argument before count and allocation arguments. |
| qcommon/huffman.c and msg_mp.c | Native Huff_addRef and MSG_WriteBigString drop unused fake parameters. Mac names verify two actual arguments; server MSG_WriteBigString callers agree. |
| qcommon/net_chan_mp.c, packet send | Call the actual void Sys_SendPacket and return one from the reconstructed Bool wrapper after dispatch. |
| ui_mp/ui_shared_mp.c, owner-draw width | Include the actual FontHandle argument before scale. |
| stringed/stringed_hooks.c, two string concatenations | Restore dest, size, source ordering, avoiding pointer truncation and treating a small size as a string pointer. |
| gfx_d3d/r_model.c, XModel checks | Pass header.model to XModelBad/XModelUnoptimize; preserve the XAssetHeader union return contract. |
| gfx_d3d/r_init.c, device creation log | Call variadic ri.Printf directly; the previous fixed two-argument cast selected the wrong Darwin ABI. |
| gfx_d3d/rb_backend.c, text-with-cursor decoder | Use unsigned int(const char **, qboolean *) consistently, matching the independently corrected text-in-space callback. |
| win32/cinematics.c, cinematic rendering | Keep MaterialHandle as a pointer and call typed DrawStretchPic directly. |
| gfx_d3d/r_marks.c | Native tr aliases real r_globals_t rg, replacing three fake mark-counter accesses. Mac GOT/markCount evidence is in the import companion. |
| win32/win_voice.c | Read the real bandwidth/frame-size globals and return when frame size is nonpositive, preventing a zero-progress incoming-voice loop while capture is unavailable. |
| game_mp/g_combat_mp.c and g_scr_main_mp.c; headers/PC/game_mp/g_types.h | fn_pain includes direction before hit location; native G_Damage passes it and the trigger callback is assigned directly. Mac Pain_trigger_damage verifies seven arguments. |
| Mac/DirectX_9/CColorConverter.c | Supply three null output pointers to the reconstructed feature-query function; it ignores them. |
| Mac/Tools/MacDebug.c | Native game_dprintf is a void variadic no-op, replacing the bogus aggregate-return declaration/definition. Debug printing remains a no-op. |
| imports/libc.h, Mac/Tools/MacTools.c and stubs/macos_compat.c | Use the shared AbsoluteTime struct contract for time APIs and locals; match the reconstructed lo/hi field order with designated initialization. |
| speex/speex.c | Native destroy wrappers and destructor dispatch return void and retain full-width state pointers, matching bundled Speex's actual mode callbacks. |
| stubs/link_stubs.c | Use size_t for the native exception-allocation wrapper. Type the native sound no-ops to their actual caller contracts, including floating arguments; the sound operations remain unimplemented. |

Native game wrappers that had returned a real void call now execute it and
return zero (62 sites listed in the game companion). Scr_GetAnim consumes the
real structured result and packs its fields explicitly. Scr_GetFunction/Method
retain function pointers; Scr_EvalVariable retains the packed 64-bit return.
Scr_PlayerKilled receives the previously omitted tenth argument animResult,
verified by the Mac player_die instruction sequence. Scr_InitSystem retains
its verified integer parameter even though this reconstruction ignores it.

## Remaining work and limits

1. **JPEG is not safe on native yet.** Callback/allocation/error contracts are
   repaired, with no baseline exceptions. R_LoadJpg/R_SaveJpg still use ILP32
   cinfo offsets/sizes and an undersized error buffer. With modern libraries
   off, jpeg_memory_src still names char storage as executable code. A typed
   LP64 wrapper/source manager is required before JPEG loading or saving can
   be claimed. Fixing that callable symbol alone would leave corruption risks.
2. **Optional debugger reconstruction is incomplete.** Three qsort comparator
   addresses have no implemented target, and watch-child code discards its new
   array. The feature defaults off. Do not enable it based on this default audit.
3. **Five renderer imports remain unbound:** Hunk_HideTempMemory,
   Hunk_ShowTempMemory, CM_RayTriangleIntersect, DB_EnumXAssets and DObjGetPartBits.
   The first two now have verified contracts. Two of the latter functions have
   no implementation to establish a full contract. Feature-specific binding
   is needed before use; Windows retail-loader setup is not native setup.
4. **VAO container and sound no-op functionality remains incomplete.** The two
   VAO tree methods now have correct typed arguments/one-pointer iterator return,
   but insert_equal returns a null iterator and erase does nothing, preserving
   their empty reconstruction bodies. SND_SetChannelInfo and environment effects
   are also no-ops; typing them does not implement audio state changes.
5. **Microphone capture remains unavailable.** Native Record_Init returns failure.
   Storage fixes and the frame-size guard prevent invalid reads/loops but do not
   supply a recorder. record.c is probed separately and replaced in the native
   executable by platform/macos_voice.c.
6. **Legacy Carbon utilities and exception dispatch need replacement/review.**
   MacBuilder still calls zero char-array API placeholders; MacAppleEvents has
   CreateObjSpecifier and six pointer-style __ZTIl references. The zero RTTI
   array is excluded on aarch64, so those references must be checked against
   real native typeinfo rather than described as reads of native zero storage.
   Their runtime reachability was not tested. The complete storage table names
   every remaining API and distinguishes address, NULL pointer and code-target use.
7. **Static analysis has explicit boundaries.** Tagged aggregate comparisons do
   not prove full field/HFA layout or narrow-integer extension. Generic vtable
   storage preserves pointers; dynamic object/slot identity is not generally
   provable from these raw casts. The 127 floating indirect sites were inspected
   by subsystem; COM Clear's float Z contract agrees. Integer/pointer unprototyped
   dispatch, import aliasing and initialization/reachability remain inventories,
   not complete runtime proofs. CoreAudio/Miles contracts are reviewed in the
   callback companion; native SDL has no audio callback registration.
8. **Dedicated link and actual i386 byte parity remain unverified/failed as above.**
   Missing foreign SDK headers and Emscripten were reported without installing
   anything. The guards were tested; a real i386 toolchain run remains necessary.

## Merge instructions

Merge this branch's focused commits in order; no CMake changes are required.
The shared edits are guarded and limited to refimport/refexport fields in
com_math.h, PM_UpdateLean in bg_funcs.h, fn_pain in g_types.h, three JPEG fields
in cod2_defs.h, and time prototypes in imports/libc.h. Review these alongside
other ABI/runtime work rather than dropping their corresponding callers.
Preserve the existing g_snd fix and the local line guards. Reconfigure/export
the merged compile database and rerun tools/abi/check.sh; do not reuse cached
reports from this worktree. Then run the real i386 parity and WS11 gameplay
bring-up checks. Repository changes contain only source, tools and reports;
licensed data and all generated diagnostics remain local.

## Excluded-source syntax probes

| Source | Native-flags probe |
| --- | --- |
| `src/Mac/Main/mac_play_dsound.c` | Parsed; excluded from native linked targets |
| `src/Mac/Main/mac_record_dsound.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/CAudioRecorder.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/CCircularBuffer.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacDisplay.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacMSS.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacMSS_Engine.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacMSS_Object.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacMSS_Sample.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacMSS_Sample2.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacMSS_Stream.c` | Parsed; excluded from native linked targets |
| `src/Mac/Tools/MacThreads.c` | Parsed; excluded from native linked targets |
| `src/PC/groupvoice/record.c` | Parsed; excluded from native linked targets |
| `src/PC/qcommon/cod2x_identity.c` | Parsed; excluded from native linked targets |
| `src/PC/qcommon/cod2x_protocol.c` | Parsed; excluded from native linked targets |
| `src/PC/qcommon/cod2x_runtime.c` | Parsed; excluded from native linked targets |
| `src/PC/zlib/adler32.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/infblock.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/infcodes.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/inffast.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/inflate.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/inftrees.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/infutil.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/PC/zlib/zutil.c` | Byte type unavailable in this probe; native uses SDK ZLIB |
| `src/blobs/bss.c` | Parsed; excluded from native linked targets |
| `src/blobs/data.c` | Parsed; excluded from native linked targets |
| `src/blobs/import_pointers.c` | g_EndPos conflicting function/data declaration; native uses generated typed imports |
| `src/stubs/agl_stubs.c` | Parsed; excluded from native linked targets |
| `src/stubs/audio_stubs.c` | Parsed; excluded from native linked targets |
| `src/stubs/cpp_compat.c` | Parsed; excluded from native linked targets |
| `src/stubs/cpp_trampoline.c` | Parsed; excluded from native linked targets |
| `src/stubs/fx_override.c` | Parsed; excluded from native linked targets |
| `src/web/cod2_wasmfs_mount.c` | emscripten/emscripten.h unavailable; wasm-only source |
| `src/web/wasm_aliases.c` | Parsed; excluded from native linked targets |
| `src/web/webgl2_compat.c` | Parsed; excluded from native linked targets |
| `src/win32/gl_loader.c` | Parsed; excluded from native linked targets |
| `src/win32/shims-msvc/dirent_msvc.c` | Parsed; excluded from native linked targets |
| `src/win32/shims-msvc/msvc_link_glue.c` | Parsed; excluded from native linked targets |
| `src/win32/shims-msvc/msvc_watchdog.c` | Parsed; excluded from native linked targets |
| `src/win32/shims-msvc/sdl2_stub.c` | Parsed; excluded from native linked targets |
| `src/win32/win32_stubs.c` | Parsed; excluded from native linked targets |

## Focused commits

The report commit follows this ordered tooling/source series. Merge the complete
series together: early declaration/definition commits can temporarily disagree
with callers corrected by later commits.

| Commit | Change |
| --- | --- |
| `8a5d3a6` | Add repeatable native ABI declaration and import audits |
| `78bc2dd` | Give native link adapters concrete ABI contracts |
| `f94bb60` | Use real native voice globals and pointer-width sound contracts |
| `80d4b50` | Align native core and memory function declarations |
| `18c6030` | Correct native effect calls and full-width effect sorting |
| `edfe72c` | Type native movement and weapon-parser callbacks |
| `1d83695` | Align native cgame prototypes and command callbacks |
| `c952ff7` | Match native renderer exports and JPEG callback signatures |
| `918bf18` | Repair native renderer prototypes and imported glow slots |
| `6f4b892` | Replace native renderer placeholder globals and command aliases |
| `9bafd79` | Correct native Mac method, time and diagnostic contracts |
| `ccaf2a6` | Preserve native client and UI callback argument contracts |
| `72b987b` | Align native entity callbacks and damage dispatch |
| `97bce29` | Correct native game script value and return contracts |
| `1f02289` | Match native script, server and animation declarations |
