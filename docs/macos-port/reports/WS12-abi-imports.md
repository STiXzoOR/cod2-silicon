# WS12 import and storage audit input

All reference addresses below were read from the licensed Mac 1.3 i386 slice at
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`. No binary bytes or
decompilation are stored here. The main WS12 report owns integrated build and
runtime results; this file records the import/storage sub-audit.

Companion inventory for [WS12-abi-audit.md](WS12-abi-audit.md). The final stable
run records 514 import symbols, 1,439 lexical sites and 1,775 native sites;
185 storage declarations; zero proven wrong import loads.

## Commands

```sh
python3 tools/abi/imports.py --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" --compile-commands build-abi/compile_commands.json --json build-abi/imports-inventory.json --check
python3 tools/abi/test_imports.py
nm -n "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
xcrun llvm-objdump --disassemble --start-address=0x1ea1b0 --stop-address=0x1ea2d0 "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
```

The import checker preprocesses every translation unit in the supplied build
database, including macros, before checking live native import loads. Its
second inventory scans all source/header text and therefore includes guarded
legacy paths. It preserves every import use and classification in generated
JSON. `--check` fails on a pointer load from a STABS struct, union, or array of
non-pointer elements, and on `*(T **)&imp_X` when X is a pointer variable
(the GOT slot address is loaded but the variable value is not). Pointer variables
and arrays of pointer slots permit ordinary `*(T **)imp_X` loads. Six unit
tests cover those distinctions and exclude comments, declarations, and external
function parameter names from placeholder uses. Zero-filled char arrays and int
scalars/arrays under `src/stubs/` are inventoried, including conditional legacy
definitions and instrumentation counters.

Next-symbol extents are the distance to the next distinct symbol in the same
Mach-O section, or the section end. They match the named-symbol interpretation
of `nm -n`; they include padding. STABS is decisive: `cl` is a 4-byte pointer
even though its next-symbol extent is 28 bytes. `g_snd` is a `snd_local_t`
struct with extent 0x1404. Its existing `SND_GLOB_PTR` fix already removes the
native extra load; the legacy branch deliberately retains it.

## Fixes and evidence

Every change below is guarded by `COD2_X64`. Existing placeholder definitions
are retained, preserving the original 32-bit build input. Correcting use sites
also avoids the ambiguous shared `speex_quality_ptr` placeholder, which had
been used for two different original globals.

| Placeholder or import | Real target and evidence | Corrected uses |
| --- | --- | --- |
| `voice_freq_ptr` | GOT 0x1acd9fd -> `_g_current_bandwidth_setting` at 0x34d7c8; `Voice_Init` 0x1ea258 writes through it | `src/PC/win32/win_voice.c`, `Voice_Init` |
| `voice_maxframe_ptr` | GOT 0x1acda01 -> `_g_frame_size` at 0x34d7c4; the voice decode loop reads the frame size | `src/PC/win32/win_voice.c`, `Voice_IncomingVoiceData` |
| `voice_scale_ptr` | GOT 0x1acda05 -> `_voice_current_scaler` at 0x34d770; `Record_QueueAudioDataForEncoding` 0x1ea7cc multiplies by it | `src/PC/groupvoice/record.c` |
| `encode_vol_ptr` | GOT 0x1acda09 -> `_voice_current_voicelevel` at 0x1ac9818; 0x1ea757 initializes it | `src/PC/groupvoice/record.c` |
| `record_callback_ptr` | GOT 0x1acda0d -> `_current_audioCallback` at 0x1acaf00; `Record_Init` 0x1ea9c7 stores the callback | `src/PC/groupvoice/record.c` |
| `speex_nb_mode_ptr` | GOT 0x1acda15 -> `_speex_nb_mode` at 0x36eba0 | `src/PC/groupvoice/encode.c`, `decode.c` |
| `speex_wb_mode_ptr` | GOT 0x1acda11 -> `_speex_wb_mode` at 0x36e980 | `src/PC/groupvoice/encode.c`, `decode.c` |
| `speex_uwb_mode_ptr` | GOT 0x1acda1d -> `_speex_uwb_mode` at 0x36e820 | `src/PC/groupvoice/encode.c`, `decode.c` |
| `speex_quality_ptr`, encoder use | GOT 0x1acd33d -> `_sv_voiceQuality` at 0x17fdc24; `Encode_Sample` 0x1ead38 reads the dvar's integer | `src/PC/groupvoice/encode.c` |
| `speex_quality_ptr`, decoder use | GOT 0x1acda19 -> `_g_encoder_samplerate` at 0x34d7cc; `Decode_Init` 0x1eaa34 passes its address | `src/PC/groupvoice/decode.c` |
| `g_unknown_195f22c` | GOT 0x1acd28d -> `_g_WarmOff` at 0x10cbc00; `RB_UploadWaterTexture` 0xfa4e9/0xfa5be writes a byte directly | `src/PC/gfx_d3d/r_water.c`, two uses |
| `g_unknown_195f230` | GOT 0x1acd291 -> `_r_drawWater` at 0x1272688; 0xfa362 loads the dvar pointer | `src/PC/gfx_d3d/r_water.c` |
| `r_gammaSetting` | GOT 0x1acd1dd -> `_r_overbrightBits` at 0x12727a4; `R_EndCubemapShot` 0xe8c07 reads its integer | `src/PC/gfx_d3d/r_screenshot.c` |
| `g_phys_world` | GOT 0x1accdb5 -> `_vec3_origin` at 0x3283f0; `G_RadiusDamage` 0x1c4965 passes this address as both trace bounds | `src/PC/game_mp/g_combat_mp.c` |
| `r_occlusionQuery` | `RB_CalcSunSpriteSamples` 0xde0e5 reads 0x130d444, `sunFlareArray` base 0x130d420 + 0x24 = first `sunQuery[0]` | `src/PC/gfx_d3d/rb_sky.c` |
| `imp_r_glowBloomIntensity`, `imp_r_glowRadius`, `imp_r_glowSkyBleedIntensity` | STABS arrays of two dvar pointers; 0xda68d/0xda69e select a pointer slot before offset 8; 0xda841 likewise indexes the pointer array | `src/PC/gfx_d3d/rb_backend.c`, four accesses |

Current corrected source locations (snapshot after native prototype edits):

| Use | Native source locations |
| --- | --- |
| voice bandwidth / frame size | `src/PC/win32/win_voice.c:121`, `:222` |
| recording level / scaler / callback slot | `src/PC/groupvoice/record.c:92`, `:104`, `:197` |
| encoder mode addresses / quality | `src/PC/groupvoice/encode.c:61`, `:68`, `:75`, `:117` |
| decoder mode addresses / sample rate | `src/PC/groupvoice/decode.c:45`, `:52`, `:59`, `:72` |
| water dvar / dirty flag addresses | `src/PC/gfx_d3d/r_water.c:51`, `:242`, `:259` |
| cubemap overbright dvar | `src/PC/gfx_d3d/r_screenshot.c:299` |
| sun sample query storage | `src/PC/gfx_d3d/rb_sky.c:65` |
| radius trace origin bounds | `src/PC/game_mp/g_combat_mp.c:569` |
| glow pointer-array indexing | `src/PC/gfx_d3d/rb_backend.c:3864`, `:3866`, `:3897`, `:3898` |

There are 14 distinct placeholder identities corrected at 19 use sites, plus
four glow pointer-array accesses. The root subsequently corrected the `tr`
placeholder at three `r_marks.c` uses to the existing `rg` global: Mac
`R_CellSurfaces` 0xf3ebd/0xf3ec2 reads GOT 0x1accf31 -> `_rg` 0x1275880
then `markCount` at offset 0x20, and `R_MarkFragments` 0xf48e2/0xf48e7
increments it. The render changes preserve the old union
bit interpretation where the original code copies float bits using an integer.

`_snd_local_listener` was separately handed to the root agent: all five
shellshock sound calls need `(const vec_t *)imp_vec3_origin`. Mac
`CG_UpdateShellShock` 0x1d76b2, 0x1d7a27, 0x1d7a9e and 0x1d7b80 all load GOT
0x1accdb5. The old `extern int` reads the first four zero bytes of the stub
and passes a null origin; its zero-filled storage is not the intended address.
The five renderer callback placeholders and `jpeg_memory_src` were handed to
the callback audit owner for guarded real-function fixes.

The root also corrected the missing variable load in
`FxScheduler.c`, `helper = *(FxHelper **)&imp_theFxHelper`. The Mac
`theFxHelper` is a pointer variable at 0x343584 with extent 4. Two related
patterns are correct: `&imp_colorYellow` loads the address of a float array
(at 0x3282f0, extent 32 including padding), and `&imp_g_VAOID` loads the
address of a scalar GLuint (at 0x341424, extent 4). The checker keeps these
as a separate `import-slot-load` category.

The generic placeholder review found `buf` is build-string storage: the
longest current format writes 38 bytes including its terminator into a 64-byte
definition, despite a 128-byte extern declaration. `name` and `version` are
writable dvar pointer slots initialized by their respective registration code
before use. `tr` was an independent fake renderer counter and is corrected
as above. `commandsList` candidate references belong to a local static list,
not the unused external placeholder. Native CVAOPacket code already maps the
OpenGL singleton to its import address and allocates pointer-sized packet/tree
storage; oversized legacy int buffers remain only for the valid scalar packet
index and status fields. The instrumentation counters are actual int objects
and are neither pointer slots nor function targets.

## Verification and limits

Clang `-fsyntax-only` using the build database passed for `win_voice.c`,
`encode.c`, `decode.c`, `rb_backend.c`, `r_water.c`, `r_screenshot.c`,
`rb_sky.c` and `g_combat_mp.c`; a standalone syntax probe of `record.c` also
passed. `record.c` is excluded from the native linked target because the
platform recorder replaces it. Native microphone capture remains unavailable:
`src/platform/macos_voice.c` returns zero from `Record_Init`. A mapped frame
size alone does not enable recording. The root agent added a native return
when incoming voice sees a nonpositive frame size, preventing a zero-progress
packet loop.

For eight exclusively owned translation units (all of the above except
`g_combat_mp.c`, which another worker edits), preprocessing both `git show
HEAD:<path>` and current source through the same build compiler with
`COD2_X64` undefined and input on stdin produced byte-identical output. This
proves these guarded changes leave compiler input unchanged with the option
off. Executable/object byte comparison was not possible here because no
runnable i386 environment is available; the root report owns that limitation.

The check is a systematic cast-pattern audit, not a replacement for arbitrary
pointer-alias analysis. It records uncertain expressions instead of silently
declaring them safe. Placeholder candidate references are lexical and may
include declarations or local symbols; the generic names were manually
reviewed as described above. `_ZN7COpenGL7sOpenGLE` has unresolved C++ STABS type parsing
but a verified data extent of 4096 at 0x10cbd80; both reconstructed spellings
refer to it. `RB_AdaptiveGpuSyncWait` and `RB_GpuWaited` have no matching
Mac 1.3 STABS function; source definitions remain the applicable reference.
The zero `__ZTIl[16]` placeholder in `cpp_trampoline.c` is excluded on
aarch64. Six pointer-style `__ZTIl` references in `MacAppleEvents.c` still
need exception ABI review against the native runtime typeinfo symbol; they
cannot be described as native reads of that excluded storage. Legacy Carbon
MacBuilder UI calls still name char-array function placeholders for
DisableControl, GetPort, GetWRefCon, HideControl, InitCursor, three
NewControlUserPane*UPP constructors, RunAppModalLoopForWindow, SetControlData,
SetControlFontStyle, SetPort and ShowControl; CreateObjSpecifier has an
analogous MacAppleEvents call. They are recorded as remaining unsupported
legacy utility paths, not as null data pointers. Their runtime reachability
is unverified; the audit owner was notified. No packages were
installed and no shared headers, build files, remotes or sibling worktrees
were modified by the import/storage worker.

<!-- inventory tables follow -->

## Complete import inventory

This snapshot contains 514 distinct import symbols and 1,438 lexical access
sites across the tree. Preprocessing all supplied compile database entries
finds 1,775 unique native file/line/column/symbol/category sites, with zero
proven extra-pointer-load or missing-pointer-load errors. Of the symbols,
206 are STABS functions, 306 are classified data objects or aliases, and two
are source functions absent from the Mac STABS reference. The snapshot binary
SHA-256 is `15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
Counts below are A/C/P/S/I: unclassified address expression, cast to address,
pointer-variable load, scalar/field load, address-of-import-slot load. The
JSON preserves each individual source location. Preprocessor expansion can
produce several different columns at one source line. Function prototypes
belong to the separate Clang ABI audit; their data extent is not applicable.
The extent column includes section padding, as explained above. Unknown
expressions and unresolved debug types remain visible for review.

| Import (`imp_` prefix omitted) | STABS shape | Mac address | Next-symbol extent | Lexical A/C/P/S/I | Native A/C/P/S/I |
| --- | --- | --- | --- | --- | --- |
| `CG_DObjCalcPose` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `CIN_PlayCinematic` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `CIN_RunCinematic` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `CIN_UploadCinematic` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `CL_GetHudMsgIconMaterialName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `CM_BoxTrace` | function | — | — | 2/1/0/0/0 | 2/1/0/0/0 |
| `CM_SaveLump` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Cbuf_ExecuteText` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Cmd_AddCommand` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Cmd_Argc` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Cmd_Argv` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Cmd_RemoveCommand` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Com_Error` | function | 0x2e33a | 0x152 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Com_FindSoundAlias` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `Com_GetBsp` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Com_LoadDvarsFromBuffer` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Com_SafeClientDObjFree` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `Com_SafeServerDObjFree` | function | — | — | 2/0/0/0/0 | 1/0/0/0/0 |
| `Com_SaveDvarsToBuffer` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjBad` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjCalcAnim` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjCalcSkel` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjCompleteHierarchyBits` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjCreate` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjCreateSkel` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetAllocSkelSize` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetBoneInfo` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetBounds` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetLodForDist` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetLodOutDist` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetMatOffset` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetModel` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetNumModels` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetNumSurfaces` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetRotTransArray` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetSurface` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetSurfaceName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjGetSurfaces` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjNumBones` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjSetModel` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `DObjSkelAreBonesUpToDate` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_ChangeResetValue` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_ClearModified` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_EnumToString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_GetBool` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_GetFloat` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_GetInt` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_GetString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_GetVariantString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_IsAtDefaultValue` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterBool` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterColor` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterEnum` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterFloat` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterInt` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterVec2` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterVec3` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_RegisterVec4` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_Reset` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetBool` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetBoolByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetColor` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetColorByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetFloat` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetFloatByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetFromString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetFromStringByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetInt` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetIntByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetModified` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetStringByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetVec2` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetVec2ByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetVec3` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetVec3ByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetVec4` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_SetVec4ByName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_UnregisterSystem` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Dvar_UpdateEnumDomain` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FCloseFile` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FOpenFileByMode` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FOpenFileRead` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FileExists` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FreeFile` | function | 0x32a3c | 0x10 | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FreeFileList` | function | 0x32a4c | 0x3e | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_FullPath_f` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `FS_ListFiles` | function | 0x33f9e | 0x40 | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_Path_f` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `FS_Read` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_ReadFile` | function | 0x3603e | 0x132 | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_Write` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `FS_WriteFile` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `G_FreeEntity` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `G_RegisterWeapon` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `Hunk_AllocInternal` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Hunk_AllocXAnimClient` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `Hunk_AllocXAnimPrecache` | function | — | — | 2/0/0/0/0 | 2/0/0/0/0 |
| `Hunk_AllocateTempMemoryHighInternal` | function | 0x379a8 | 0xb8 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Hunk_AllocateTempMemoryInternal` | function | 0x38192 | 0x110 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Hunk_ClearTempMemory` | function | 0x37bda | 0x18 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Hunk_ClearTempMemoryHigh` | function | 0x37a60 | 0x10 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Hunk_FreeTempMemory` | function | 0x37c9c | 0x5c | 0/1/0/0/0 | 0/1/0/0/0 |
| `Hunk_OverrideDataForFile` | function | 0x37776 | 0x5c | 0/1/0/0/0 | 0/1/0/0/0 |
| `Material_Duplicate` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `Material_IsDefault` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `Material_RegisterHandle` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `RB_AdaptiveGpuSyncWait` | function-or-unresolved | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `RB_GpuWaited` | function-or-unresolved | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `RB_IsGpuFenceFinished` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `RB_UpdateColor` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AbortRenderCommands` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddCmdBlendSavedScreen` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddCmdDrawTextInSpace` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddCmdDrawTextWithCursor` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddCmdSaveScreen` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddCmdSetMaterialColor` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddLightToScene` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddPlume` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_AddPolyToScene` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ArchiveFogState` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_BeginCubemapShot` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_BeginDebugFrame` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_BeginDelayedDrawing` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_BeginFrame` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ClearFlares` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ClearFogs` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ClearScene` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ConsoleTextWidth` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_DObjGetSurfMaterials` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_DObjReplaceMaterial` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_DefaultVertexFrames` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_DrawConsoleText` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_DrawText` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_DuplicateFont` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_EndCubemapShot` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_EndDebugFrame` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_EndDelayedDrawing` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_EndFrame` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_FinishLoadingModels` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_FreeImageAllocations` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_GetFarPlaneDist` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_GetIgnorePrecacheErrors` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_GetMaterialName` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_GetMaterialSubimageCount` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_GetMinSpecImageMemory` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_GetWorldBounds` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_InterpretSunLightParseParams` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_IsMaterialRefractive` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_IssueDelayedDrawing` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_LightingFromCubemapShots` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_LoadWorld` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_LocateDebugLines` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_LocateDebugStrings` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_MarkFragments` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ModelBounds` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_NormalizedTextScale` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ParseSunLight` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_PickMaterial` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_RegisterFont` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_RegisterInlineModel` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_RegisterModel` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_RegisterRawImage` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_RenderScene` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ResetImageAllocations` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ResetSunLightOverride` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ResetSunLightParseParams` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SaveCubemapShot` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SetCullDist` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SetFog` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SetIgnorePrecacheErrors` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SetLodOrigin` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SetSunLightOverride` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_ShutdownDebug` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SwitchFog` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_SyncRenderThread` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_TextHeight` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `R_TextWidth` | function | — | — | 1/0/0/0/0 | 1/0/0/0/0 |
| `SEH_ReadCharFromString` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `SV_XModelGet` | function | — | — | 2/0/0/0/0 | 1/0/0/0/0 |
| `StatMon_Warning` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Sys_DirectXFatalError` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Sys_HideSplashWindow` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Sys_LoadingKeepAlive` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Sys_ShowSplashWindow` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelBad` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetBasePose` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetBasePoseBone` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetFlags` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetLodForDist` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetLodName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetLodOutDist` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetMemUsage` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetNumLods` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetSkins` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetSurfaceName` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelGetSurfaces` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelNumBones` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelPrecache` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `XModelSetTestLods` | function | — | — | 0/1/0/0/0 | 0/1/0/0/0 |
| `Z_FreeInternal` | function | 0x3740c | 0xa | 0/1/0/0/0 | 0/1/0/0/0 |
| `Z_MallocInternal` | function | 0x37d4c | 0x54 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Z_VirtualCommitInternal` | function | 0x374c4 | 0x42 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Z_VirtualDecommitInternal` | function | 0x3743a | 0x22 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Z_VirtualFreeInternal` | function | 0x37416 | 0x24 | 0/1/0/0/0 | 0/1/0/0/0 |
| `Z_VirtualReserveInternal` | function | 0x37498 | 0x2c | 0/1/0/0/0 | 0/1/0/0/0 |
| `_ZN7COpenGL7sOpenGLE` | unresolved | 0x10cbd80 | 0x1000 | 2/2/0/0/0 | 1/5/0/0/0 |
| `__ZN15CDirect3DDevice28mNeedsVertexShaderValidationE` | builtin | 0x34141b | 0x5 | 2/0/0/0/0 | 2/0/0/0/0 |
| `__ZN15CDirect3DDevice29mNeedsRasterizationValidationE` | builtin | 0x341419 | 0x1 | 2/0/0/0/0 | 2/0/0/0/0 |
| `__ZN15CDirect3DDevice30mNeedsTransformationValidationE` | builtin | 0x34141a | 0x1 | 2/0/0/0/0 | 2/0/0/0/0 |
| `__ZN7COpenGL7sOpenGLE` | unresolved | 0x10cbd80 | 0x1000 | 0/1/0/0/0 | 0/1/0/0/0 |
| `__ZTV10COpenGLVAO` | vtable | 0x36cc20 | 0x20 | 1/0/0/0/0 | 1/0/0/0/0 |
| `__ZTV11CColorArray` | vtable | 0x369b20 | 0x20 | 1/0/0/0/0 | 1/0/0/0/0 |
| `__ZTV12CNormalArray` | vtable | 0x369a60 | 0x20 | 1/0/0/0/0 | 1/0/0/0/0 |
| `__ZTV12CVertexArray` | vtable | 0x369aa0 | 0x20 | 1/0/0/0/0 | 1/0/0/0/0 |
| `__ZTV14CTexCoordArray` | vtable | 0x3699e0 | 0x20 | 1/0/0/0/0 | 1/0/0/0/0 |
| `__ZTV15CColorConverter` | vtable | 0x3692e0 | 0x40 | 0/1/0/0/0 | 0/1/0/0/0 |
| `__ZTV20CSecondaryColorArray` | vtable | 0x369ae0 | 0x20 | 1/0/0/0/0 | 1/0/0/0/0 |
| `__ZTV7CBaseVA` | vtable | 0x369a40 | 0x20 | 0/2/0/0/0 | 0/1/0/0/0 |
| `alwaysfails` | range | 0x34a300 | 0x20 | 1/0/0/0/0 | 0/0/0/0/0 |
| `backEnd` | struct:r_backEndGlobals_t | 0x12d6580 | 0x36e90 | 1/24/0/0/0 | 0/33/0/0/0 |
| `backEndData` | pointer | 0x130d410 | 0x10 | 1/0/0/0/0 | 0/0/0/0/0 |
| `bg_bobMax` | pointer | 0x10e0ce0 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `bg_fallDamageMaxHeight` | pointer | 0x10e0d08 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `bg_fallDamageMinHeight` | pointer | 0x10e0d0c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `bg_iNumWeapons` | range | 0x343b20 | 0x20 | 0/0/0/2/0 | 0/0/0/2/0 |
| `bg_itemlist` | array:struct | 0x341e00 | 0x16c0 | 0/19/0/0/0 | 0/19/0/0/0 |
| `bg_numItems` | range | 0x326230 | 0x47c | 0/2/0/7/0 | 0/2/0/7/0 |
| `bgs` | pointer | 0x34b900 | 0x20 | 2/0/15/0/0 | 1/0/14/0/0 |
| `bulletPriorityMap` | array:range | 0x34d1af | 0x31 | 0/3/0/0/0 | 0/3/0/0/0 |
| `cg` | pointer | 0x36cf50 | 0x10 | 3/0/0/0/0 | 0/0/0/0/0 |
| `cgDC` | struct:displayContextDef_s | 0x1311380 | 0x280 | 0/25/0/0/0 | 0/25/0/0/0 |
| `cg_connectionIconSize` | pointer | 0x1311118 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cg_constantSizeHeadIcons` | pointer | 0x1311110 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cg_debugEvents` | pointer | 0x1311284 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_entities` | pointer | 0x36cf48 | 0x4 | 2/0/5/0/0 | 0/0/5/0/0 |
| `cg_errorDecay` | pointer | 0x1311280 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_footsteps` | pointer | 0x1311274 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_gun_x` | pointer | 0x1311254 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cg_gun_y` | pointer | 0x1311250 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cg_gun_z` | pointer | 0x131124c | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cg_hudCompassMaxRange` | pointer | 0x1311330 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudCompassMinRadius` | pointer | 0x1311328 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudCompassMinRange` | pointer | 0x131132c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudCompassSize` | pointer | 0x1311334 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudCompassSoundPingFadeTime` | pointer | 0x1311320 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudCompassSpringyPointers` | pointer | 0x1311324 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudObjectiveMaxRange` | pointer | 0x1311318 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_hudObjectiveMinAlpha` | pointer | 0x1311314 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_items` | pointer | 0x36cf40 | 0x4 | 0/0/2/0/0 | 0/0/2/0/0 |
| `cg_nopredict` | pointer | 0x131127c | 0x4 | 2/0/0/0/0 | 0/0/0/0/0 |
| `cg_predictItems` | pointer | 0x13111e4 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `cg_scoreboardScrollStep` | pointer | 0x13111a0 | 0x4 | 0/0/0/2/0 | 0/0/0/2/0 |
| `cg_scriptIconSize` | pointer | 0x131111c | 0x4 | 2/0/0/0/0 | 2/0/0/0/0 |
| `cg_shock_mouse` | pointer | 0x1311134 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_mouse_fadeTime` | pointer | 0x1311124 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_mouse_maxpitchspeed` | pointer | 0x1311130 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_mouse_maxyawspeed` | pointer | 0x131112c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_mouse_sensitivityscale` | pointer | 0x1311128 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_screenBlendFadeTime` | pointer | 0x1311190 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_screenBlendTime` | pointer | 0x1311194 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_sound` | pointer | 0x1311184 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundDryLevel` | pointer | 0x131116c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundFadeInTime` | pointer | 0x1311180 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundFadeOutTime` | pointer | 0x131117c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundLoopEndDelay` | pointer | 0x1311174 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundLoopFadeTime` | pointer | 0x1311178 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundModEndDelay` | pointer | 0x1311164 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundRoomType` | pointer | 0x1311170 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_soundWetLevel` | pointer | 0x1311168 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_viewKickPeriod` | pointer | 0x131118c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_viewKickRadius` | pointer | 0x1311188 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_announcer` | pointer | 0x131113c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_auto` | pointer | 0x1311160 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_auto2d` | pointer | 0x131115c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_body` | pointer | 0x1311148 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_item` | pointer | 0x131114c | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_local` | pointer | 0x1311144 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_menu` | pointer | 0x1311158 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_music` | pointer | 0x1311140 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_shellshock` | pointer | 0x1311138 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_voice` | pointer | 0x1311150 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_shock_volume_weapon` | pointer | 0x1311154 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `cg_showmiss` | pointer | 0x1311278 | 0x4 | 2/0/0/0/0 | 1/0/0/0/0 |
| `cg_synchronousClients` | pointer | 0x13111f4 | 0x4 | 2/0/0/0/0 | 0/0/0/0/0 |
| `cg_teamChatsOnly` | pointer | 0x1311298 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cg_voiceIconSize` | pointer | 0x1311114 | 0x4 | 2/0/0/0/0 | 2/0/0/0/0 |
| `cg_weapons` | pointer | 0x36cf44 | 0x4 | 0/0/10/0/0 | 0/0/10/0/0 |
| `cg_youInKillCamSize` | pointer | 0x1311120 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cgs` | pointer | 0x36cf4c | 0x4 | 2/0/0/0/0 | 0/0/0/0/0 |
| `chatField` | pointer | 0x34acc8 | 0x18 | 0/2/1/0/0 | 0/2/1/0/0 |
| `chat_team` | pointer | 0x34acc4 | 0x4 | 0/0/3/0/0 | 0/0/3/0/0 |
| `cl` | pointer | 0x36cf64 | 0x1c | 4/4/69/0/0 | 1/10/104/0/0 |
| `cl_bypassMouseInput` | pointer | 0x194b780 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cl_cdkey` | array:range | 0x341cec | 0x34 | 0/7/0/0/0 | 0/6/0/0/0 |
| `cl_cdkeychecksum` | array:range | 0x341ce0 | 0xc | 0/4/0/0/0 | 0/4/0/0/0 |
| `cl_connectedToPureServer` | range | 0x1ac9600 | 0x80 | 0/0/0/1/0 | 0/0/0/1/0 |
| `cl_freezeDemo` | pointer | 0x17e06a8 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cl_paused` | pointer | 0x10cced4 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cl_pinglist` | array:struct | 0x1311780 | 0x4140 | 0/2/0/0/0 | 0/2/0/0/0 |
| `cl_showTimeDelta` | pointer | 0x17e06ac | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `cl_shownuments` | pointer | 0x17e06a4 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `clc` | pointer | 0x36cf60 | 0x4 | 2/1/25/0/0 | 0/12/46/0/0 |
| `clients` | array:struct | 0x164a880 | 0x195d94 | 0/1/0/0/0 | 0/1/0/0/0 |
| `cls` | struct:clientStatic_t | 0x13158e0 | 0x2b4da0 | 2/25/0/0/0 | 0/29/0/0/0 |
| `cm` | struct:clipMap_t | 0x10e0da0 | 0x180 | 0/3/0/0/0 | 0/3/0/0/0 |
| `colorBlack` | array:range | 0x328350 | 0x50 | 0/4/0/0/0 | 0/5/0/0/0 |
| `colorBlue` | array:range | 0x328310 | 0x20 | 0/1/0/0/0 | 0/1/0/0/0 |
| `colorCyan` | array:range | 0x3282b0 | 0x10 | 0/3/0/0/0 | 0/3/0/0/0 |
| `colorGreen` | array:range | 0x328330 | 0x10 | 0/1/0/0/0 | 0/1/0/0/0 |
| `colorLtGrey` | array:range | 0x328260 | 0x10 | 0/1/0/0/0 | 0/1/0/0/0 |
| `colorLtYellow` | array:range | 0x3282e0 | 0x10 | 0/2/0/0/0 | 0/2/0/0/0 |
| `colorMagenta` | array:range | 0x3282c0 | 0x20 | 0/1/0/0/0 | 0/1/0/0/0 |
| `colorRed` | array:range | 0x328340 | 0x10 | 0/2/0/0/0 | 0/2/0/0/0 |
| `colorWhite` | array:range | 0x328270 | 0x20 | 3/16/0/1/0 | 1/16/0/1/0 |
| `colorYellow` | array:range | 0x3282f0 | 0x20 | 0/0/0/0/1 | 0/0/0/0/1 |
| `com_dedicated` | pointer | 0x10d6584 | 0x80 | 1/4/1/0/0 | 0/0/1/0/0 |
| `com_errorEntered` | range | 0x10cceb4 | 0x4 | 0/1/0/1/0 | 0/2/0/1/0 |
| `com_expectedHunkUsage` | pointer | 0x10ccecc | 0x4 | 1/0/2/0/0 | 0/0/2/0/0 |
| `com_fixedConsolePosition` | range | 0x10cceb0 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `com_frameTime` | range | 0x10ccebc | 0x4 | 0/0/0/4/0 | 0/0/0/4/0 |
| `com_statmon` | pointer | 0x10ccefc | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `com_sv_running` | pointer | 0x10ccee8 | 0x4 | 5/0/0/0/0 | 4/0/0/0/0 |
| `com_timescaleValue` | range | 0x10ccef4 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `current_audioCallback` | pointer | 0x1acaf00 | 0x80 | 1/0/0/0/0 | 0/0/0/0/0 |
| `developer` | pointer | 0x12725a8 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `dvar_modifiedFlags` | range | 0x10d7c08 | 0x4 | 1/1/0/8/0 | 0/1/0/8/0 |
| `dx` | struct:DxGlobals | 0x1272820 | 0x2de0 | 2/22/0/0/0 | 3/22/0/0/0 |
| `dxState` | struct:DxState | 0x1279b80 | 0x2184 | 1/0/0/0/0 | 0/0/0/0/0 |
| `effectActiveCountBolt` | range | 0x10d7e40 | 0x40 | 0/0/0/1/0 | 0/0/0/1/0 |
| `effectActiveCountNonBolt` | range | 0x10d7e3c | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `entityHandlers` | array:struct | 0x34cd60 | 0x320 | 0/8/0/0/0 | 0/9/0/0/0 |
| `eventnames` | array:pointer | 0x3435e0 | 0x320 | 0/1/0/0/0 | 0/1/0/0/0 |
| `frame_msec` | range | 0x1311608 | 0x18 | 0/0/0/1/0 | 0/0/0/1/0 |
| `fs_basepath` | pointer | 0x10d63fc | 0x4 | 5/0/0/0/0 | 6/0/0/0/0 |
| `fs_checksumFeed` | range | 0x10d63d8 | 0x4 | 0/0/0/2/0 | 0/0/0/2/0 |
| `fs_fakeChkSum` | range | 0x10d63dc | 0x4 | 0/0/0/2/0 | 0/0/0/2/0 |
| `fs_gamedir` | array:range | 0x10d6420 | 0x100 | 0/7/0/0/0 | 0/8/0/0/0 |
| `fs_homepath` | pointer | 0x10d6400 | 0x4 | 0/0/0/0/0 | 1/0/0/0/0 |
| `fs_numServerIwds` | range | 0x341dc0 | 0x4 | 0/2/0/1/0 | 0/2/0/1/0 |
| `fs_numServerReferencedIwds` | range | 0x10cf1a0 | 0x20 | 0/0/0/3/0 | 0/0/0/3/0 |
| `fs_searchpaths` | pointer | 0x341dc8 | 0x18 | 0/0/9/0/0 | 0/0/9/0/0 |
| `fs_serverIwdNames` | array:pointer | 0x10cf1c0 | 0x1000 | 1/1/0/0/0 | 1/1/0/0/0 |
| `fs_serverIwds` | array:range | 0x10d01c0 | 0x1000 | 1/1/0/0/0 | 1/1/0/0/0 |
| `fs_serverReferencedIwdNames` | array:pointer | 0x10cd1a0 | 0x1000 | 0/2/0/0/0 | 0/2/0/0/0 |
| `fs_serverReferencedIwds` | array:range | 0x10ce1a0 | 0x1000 | 0/2/0/0/0 | 0/2/0/0/0 |
| `fsh` | array:struct | 0x10d11c0 | 0x5218 | 0/2/0/0/0 | 0/2/0/0/0 |
| `fxSchedulers` | array:pointer | 0x10e0c04 | 0x80 | 0/0/3/0/0 | 0/0/3/0/0 |
| `fx_camera_valid` | range | 0x10e0b84 | 0x80 | 0/0/0/2/0 | 0/0/0/2/0 |
| `fx_count` | pointer | 0x17e0630 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `fx_cull` | pointer | 0x17e0644 | 0x4 | 4/0/0/0/0 | 4/0/0/0/0 |
| `fx_debug` | pointer | 0x17e063c | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `fx_draw` | pointer | 0x17e0648 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `fx_enable` | pointer | 0x17e064c | 0x4 | 5/0/0/0/0 | 5/0/0/0/0 |
| `fx_freeze` | pointer | 0x17e0634 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `fx_sort` | pointer | 0x17e0640 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `g_EndPos` | range | 0x343b40 | 0x20 | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_NoTextureID` | builtin | 0x10cbc01 | 0x7f | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_TotalFilterPasses` | range | 0x34a440 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `g_VAOID` | range | 0x341424 | 0x4 | 0/0/0/0/1 | 0/0/0/0/1 |
| `g_WarmOff` | builtin | 0x10cbc00 | 0x1 | 0/2/0/0/0 | 0/2/0/0/0 |
| `g_antilag` | pointer | 0x194f78c | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `g_consoleField` | struct:field_t | 0x130ecc0 | 0x118 | 2/7/0/0/0 | 1/6/0/0/0 |
| `g_console_char_height` | range | 0x34b200 | 0x4 | 0/0/0/2/0 | 0/0/0/2/0 |
| `g_console_field_width` | range | 0x34b204 | 0x1c | 0/0/0/2/0 | 0/0/0/2/0 |
| `g_debugDamage` | pointer | 0x194f82c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `g_disableRendering` | range | 0x1279b70 | 0x10 | 1/0/0/2/0 | 1/0/0/2/0 |
| `g_effectVisArray` | array:struct | 0x10d7e80 | 0x8ca0 | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_effectVisArrayCount` | range | 0x10e0b20 | 0x4 | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_entities` | array:struct | 0x194f880 | 0x8c000 | 0/19/0/0/0 | 0/27/0/0/0 |
| `g_hudelems` | array:struct | 0x1aa5d00 | 0x23020 | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_password` | pointer | 0x194f854 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `g_qport` | range | 0x34b1e0 | 0x4 | 0/0/0/3/0 | 0/0/0/3/0 |
| `g_rendererExists` | range | 0x3435c0 | 0x20 | 1/0/0/0/0 | 0/0/0/0/0 |
| `g_scr_data` | struct:scr_data_t | 0x194b880 | 0x3700 | 0/4/0/0/0 | 0/8/0/0/0 |
| `g_skinBuffers` | array:struct | 0x1268580 | 0xa004 | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_snd` | struct:snd_local_t | 0x10d6780 | 0x1404 | 0/1/1/0/0 | 0/1/0/0/0 |
| `g_special` | builtin | 0x10cbb88 | 0x78 | 0/3/0/0/0 | 0/3/0/0/0 |
| `g_traceThreadInfo` | array:struct | 0x10cce04 | 0x1c | 0/1/0/0/0 | 0/1/0/0/0 |
| `g_waitingForServer` | range | 0x13158c0 | 0x20 | 0/0/0/1/0 | 0/0/0/1/0 |
| `g_wv` | struct:WinVars_t | 0x1229f00 | 0x20 | 0/1/0/0/0 | 0/1/0/0/0 |
| `hintStrings` | array:pointer | 0x34d120 | 0x20 | 0/1/0/0/0 | 0/1/0/0/0 |
| `historyEditLines` | array:struct | 0x130ede0 | 0x2320 | 0/2/0/0/0 | 0/1/0/0/0 |
| `hud_fade_compass` | pointer | 0x194b81c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `infoParms` | array:struct | 0x348820 | 0x440 | 1/2/0/0/0 | 0/2/0/0/0 |
| `keys` | pointer | 0x34acb8 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `legacyHacks` | pointer | 0x348760 | 0x20 | 0/4/21/0/0 | 0/4/20/0/0 |
| `legacyHacksArray` | array:struct | 0x1228da0 | 0x700 | 0/2/0/0/0 | 0/1/0/0/0 |
| `level` | struct:level_locals_t | 0x1aa2280 | 0x3680 | 0/16/0/1/0 | 0/24/0/3/0 |
| `level_bgs` | struct:bgs_t | 0x19db880 | 0xc6a00 | 1/2/0/0/0 | 0/2/0/0/0 |
| `loc_warnings` | pointer | 0x10ccf84 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `loc_warningsAsErrors` | pointer | 0x10ccf80 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `net_lanauthorize` | pointer | 0x17fdb80 | 0x4 | 0/0/2/0/0 | 0/0/2/0/0 |
| `net_profile` | pointer | 0x17fdb88 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `net_showprofile` | pointer | 0x17fdb84 | 0x4 | 0/1/0/0/0 | 0/0/0/0/0 |
| `nextmap` | pointer | 0x10cced0 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `player_breath_hold_time` | pointer | 0x10e0cd8 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `player_breath_snd_delay` | pointer | 0x10e0cbc | 0x4 | 2/0/0/0/0 | 2/0/0/0/0 |
| `player_breath_snd_lerp` | pointer | 0x10e0cc0 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `pmoveHandlers` | array:struct | 0x343900 | 0x20 | 0/2/0/0/0 | 0/2/0/0/0 |
| `privateEffectActiveCountBolt` | range | 0x10d7e34 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `privateEffectActiveCountNonBolt` | range | 0x10d7e30 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `r_aaAlpha` | pointer | 0x12726c4 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_aaSamples` | pointer | 0x12726c0 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `r_anisotropy` | pointer | 0x1272794 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_cosinePowerMapShift` | pointer | 0x1272724 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_drawSun` | pointer | 0x12726b4 | 0x4 | 2/0/0/0/0 | 0/0/2/0/0 |
| `r_drawWater` | pointer | 0x1272688 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_fog` | pointer | 0x1272754 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `r_fullbright` | pointer | 0x1272790 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `r_gamma` | pointer | 0x12727a0 | 0x4 | 0/0/3/2/0 | 0/0/3/0/0 |
| `r_glow` | pointer | 0x12725f0 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `r_glowBloomIntensity` | array:pointer | 0x12725d8 | 0x8 | 0/2/2/0/0 | 0/2/0/0/0 |
| `r_glowRadius` | array:pointer | 0x12725e8 | 0x8 | 0/1/1/0/0 | 0/1/0/0/0 |
| `r_glowSkyBleedIntensity` | array:pointer | 0x12725e0 | 0x8 | 0/1/1/0/0 | 0/1/0/0/0 |
| `r_gpuSync` | pointer | 0x1272778 | 0x4 | 0/0/1/1/0 | 0/0/1/0/0 |
| `r_ignoreHwGamma` | pointer | 0x127279c | 0x4 | 0/0/2/0/0 | 0/0/2/0/0 |
| `r_lightMap` | pointer | 0x1272738 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_lightTweakAmbient` | pointer | 0x12726f8 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_lightTweakAmbientColor` | pointer | 0x12726ec | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_lightTweakDiffuseFraction` | pointer | 0x12726f4 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_lightTweakSunColor` | pointer | 0x12726e8 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_lightTweakSunDiffuseColor` | pointer | 0x12726e4 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_lightTweakSunDirection` | pointer | 0x12726e0 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_lightTweakSunLight` | pointer | 0x12726f0 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `r_multiGpu` | pointer | 0x1272774 | 0x4 | 0/0/1/1/0 | 0/0/1/0/0 |
| `r_outdoorFeather` | pointer | 0x12725c4 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_overbrightBits` | pointer | 0x12727a4 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_showLightGrid` | pointer | 0x1272700 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `r_showMissingLightGrid` | pointer | 0x12726fc | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `r_skipBackEnd` | pointer | 0x12726d0 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `r_swapInterval` | pointer | 0x12726c8 | 0x4 | 0/0/0/1/0 | 0/0/0/1/0 |
| `r_testFill` | pointer | 0x12725b8 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `r_testFillEnable` | pointer | 0x12725b4 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `r_testTransform` | pointer | 0x12725bc | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `r_textureMode` | pointer | 0x1272798 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `r_vc_makelog` | pointer | 0x1272708 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `r_vc_showlog` | pointer | 0x1272704 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `r_zfar` | pointer | 0x1272758 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `re` | struct:refexport_t | 0x1311620 | 0x160 | 3/19/0/0/0 | 2/16/0/0/0 |
| `rg` | struct:r_globals_t | 0x1275880 | 0x3200 | 0/19/0/0/0 | 0/18/0/0/0 |
| `rgp` | struct:r_global_permanent_t | 0x1278a80 | 0x10f0 | 1/21/0/0/0 | 0/21/0/0/0 |
| `ri` | struct:refimport_t | 0x1275640 | 0x240 | 1/0/0/0/0 | 0/0/0/0/0 |
| `riflePriorityMap` | array:range | 0x34d19c | 0x13 | 0/1/0/0/0 | 0/1/0/0/0 |
| `s_sundvars` | array:pointer | 0x34a3c0 | 0x60 | 1/1/0/0/0 | 0/1/0/0/0 |
| `sc_enable` | pointer | 0x1272634 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `scene` | struct:GfxScene | 0x1249f80 | 0x1e584 | 1/0/0/0/0 | 0/0/0/0/0 |
| `scrAnimPub` | struct:scrAnimPub_t | 0x1226fa0 | 0x480 | 0/7/0/0/0 | 0/9/0/0/0 |
| `scrCompileGlob` | struct:scrCompileGlob_t | 0x5a7e00 | 0x200 | 1/1/0/0/0 | 0/375/0/0/0 |
| `scrCompilePub` | struct:scrCompilePub_t | 0x1225ea0 | 0x1064 | 0/20/0/0/0 | 0/18/0/0/0 |
| `scrParserPub` | struct:scrParserPub_t | 0x1226f04 | 0x1c | 0/4/0/0/0 | 0/4/0/0/0 |
| `scrVarPub` | struct:scrVarPub_t | 0x10e5e20 | 0x40060 | 0/62/0/0/0 | 0/103/0/0/0 |
| `scrVmPub` | struct:scrVmPub_t | 0x10e1180 | 0x4320 | 0/1/0/0/0 | 0/1/0/0/0 |
| `scr_const` | struct:scr_const_t | 0x1ac8e80 | 0x100 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sharedUiInfo` | struct:sharedUiInfo_t | 0x17e18c0 | 0x1c2c0 | 0/4/0/0/0 | 0/4/0/0/0 |
| `singleClientEvents` | array:range | 0x328f20 | 0xa0 | 0/1/0/0/0 | 0/1/0/0/0 |
| `snd_bits` | pointer | 0x10d6724 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `snd_enableReverb` | pointer | 0x10d6708 | 0x4 | 0/0/5/0/0 | 0/0/5/0/0 |
| `snd_khz` | pointer | 0x10d6728 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `snd_stereo` | pointer | 0x10d6720 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `speex_nb_mode` | struct:SpeexMode | 0x36eba0 | 0x40 | 2/0/0/0/0 | 2/0/0/0/0 |
| `speex_uwb_mode` | struct:SpeexMode | 0x36e820 | 0x40 | 2/0/0/0/0 | 2/0/0/0/0 |
| `speex_wb_mode` | struct:SpeexMode | 0x36e980 | 0x40 | 2/0/0/0/0 | 2/0/0/0/0 |
| `sunFlareArray` | array:struct | 0x130d420 | 0xe4 | 1/4/0/0/0 | 0/2/0/0/0 |
| `sv` | struct:server_t | 0x17fdd00 | 0x5f580 | 0/30/0/0/0 | 0/25/0/0/0 |
| `sv_allowAnonymous` | pointer | 0x17fdc48 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_allowDownload` | pointer | 0x17fdc90 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_allowedClan1` | pointer | 0x17fdc10 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_allowedClan2` | pointer | 0x17fdc0c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_cheats` | pointer | 0x17fdc4c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_debugRate` | pointer | 0x17fdc5c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_debugReliableCmds` | pointer | 0x17fdc58 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_disableClientConsole` | pointer | 0x17fdc2c | 0x4 | 2/0/0/0/0 | 1/0/0/0/0 |
| `sv_floodProtect` | pointer | 0x17fdc50 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_fps` | pointer | 0x17fdca4 | 0x5c | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_gametype` | pointer | 0x17fdc60 | 0x4 | 4/0/0/0/0 | 3/0/0/0/0 |
| `sv_hostname` | pointer | 0x17fdc80 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_iwdNames` | pointer | 0x17fdc1c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_iwds` | pointer | 0x17fdc20 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_kickBanTime` | pointer | 0x17fdc30 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_mapRotation` | pointer | 0x17fdc38 | 0x4 | 2/0/0/0/0 | 1/0/0/0/0 |
| `sv_mapRotationCurrent` | pointer | 0x17fdc34 | 0x4 | 3/0/0/0/0 | 2/0/0/0/0 |
| `sv_mapname` | pointer | 0x17fdc74 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_maxPing` | pointer | 0x17fdc64 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_maxRate` | pointer | 0x17fdc6c | 0x4 | 2/0/3/0/0 | 0/0/3/0/0 |
| `sv_maxclients` | pointer | 0x17fdc8c | 0x4 | 3/0/0/0/0 | 2/0/0/0/0 |
| `sv_minPing` | pointer | 0x17fdc68 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_packet_info` | pointer | 0x17fdc40 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_padPackets` | pointer | 0x17fdc78 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_paused` | pointer | 0x10ccec8 | 0x4 | 1/0/0/0/0 | 1/0/0/0/0 |
| `sv_privateClients` | pointer | 0x17fdc84 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_privatePassword` | pointer | 0x17fdc94 | 0x4 | 0/0/1/0/0 | 0/0/1/0/0 |
| `sv_pure` | pointer | 0x17fdc54 | 0x4 | 1/0/3/0/0 | 0/0/3/0/0 |
| `sv_reconnectlimit` | pointer | 0x17fdc7c | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_referencedIwdNames` | pointer | 0x17fdc14 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_referencedIwds` | pointer | 0x17fdc18 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_serverId_value` | range | 0x34ace0 | 0x20 | 0/0/0/3/0 | 0/0/0/3/0 |
| `sv_serverid` | pointer | 0x17fdc70 | 0x4 | 2/0/0/0/0 | 1/0/0/0/0 |
| `sv_showAverageBPS` | pointer | 0x17fdc3c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_showCommands` | pointer | 0x17fdc44 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_timeout` | pointer | 0x17fdca0 | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `sv_voice` | pointer | 0x17fdc28 | 0x4 | 1/0/0/2/0 | 0/0/0/0/0 |
| `sv_voiceQuality` | pointer | 0x17fdc24 | 0x4 | 1/0/1/0/0 | 0/0/1/0/0 |
| `sv_zombietime` | pointer | 0x17fdc9c | 0x4 | 1/0/0/0/0 | 0/0/0/0/0 |
| `svs` | struct:serverStatic_t | 0x185d280 | 0x1b100 | 0/77/0/0/0 | 0/63/0/0/0 |
| `tess` | struct:materialCommands_t | 0x127bd80 | 0x5a800 | 2/14/0/0/0 | 0/21/0/0/0 |
| `theFxHelper` | pointer | 0x343584 | 0x4 | 3/0/27/0/1 | 1/0/29/0/0 |
| `theFxScheduler` | pointer | 0x3435a4 | 0x1c | 1/3/11/0/0 | 0/3/9/0/0 |
| `var_typename` | array:pointer | 0x343b60 | 0x60 | 0/11/0/0/0 | 0/11/0/0/0 |
| `vec3_colorintensity` | array:range | 0x32afc0 | 0x20 | 1/2/0/0/0 | 0/2/0/0/0 |
| `vec3_origin` | array:range | 0x3283f0 | 0xc | 5/15/0/1/0 | 1/19/0/1/0 |
| `vidConfig` | struct:vidConfig_t | 0x1275600 | 0x40 | 2/17/0/0/0 | 1/17/0/0/0 |

## Complete zero-storage inventory

185 declarations (183 distinct names) are inventoried below: 165 zero char
arrays in link_stubs, one conditional legacy RTTI array, three mutually
exclusive material string spellings, ten instrumentation counters, one legacy
string terminal scalar, and five OpenGL/VAO scalar or array placeholders.
“Native candidates” includes names seen after preprocessing, including header
prototypes and local shadows. “None found” is a source-search result, not a
runtime reachability proof. The generated JSON stores all candidate locations.
A zero array used as an address is a valid address with zero contents. A zero
array used as pointer storage yields NULL until initialized. Calling a zero
array as a function instead branches to data and remains unsupported.

| Storage symbol | Definition | Bytes | Uses / native candidates | Classification and remaining reason |
| --- | --- | --- | --- | --- |
| `g_dip_is_tri` | `src/stubs/agl_stubs.c:10` | 4 | 1 / 2 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_dip_drawflag_zero` | `src/stubs/agl_stubs.c:11` | 4 | 1 / 2 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_dip_numelems_zero` | `src/stubs/agl_stubs.c:12` | 4 | 1 / 2 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_dip_gl_draw` | `src/stubs/agl_stubs.c:13` | 4 | 1 / 2 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_fp_enable_count` | `src/stubs/agl_stubs.c:15` | 4 | 0 / 1 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_fp_bind_count` | `src/stubs/agl_stubs.c:16` | 4 | 0 / 1 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `__ZNSs4_Rep11_S_terminalE` | `src/stubs/cpp_trampoline.c:201` | 4 | 1 / 0 | Legacy zero scalar used as NUL character; native has const char terminal definition. |
| `__ZTIl` | `src/stubs/cpp_trampoline.c:206` | 16 | 6 / 6 | Zero RTTI definition excluded on aarch64; six MacAppleEvents native pointer-style RTTI uses need exception ABI review. |
| `AUGraphGetNodeInfo` | `src/stubs/link_stubs.c:420` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `AUGraphNewNode` | `src/stubs/link_stubs.c:421` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `AUGraphUpdate` | `src/stubs/link_stubs.c:422` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `buf` | `src/stubs/link_stubs.c:428` | 64 | 3 / manual | Writable build string; 38-byte maximum current write fits 64. Extern capacity mismatch documented. |
| `cg_debug_ptr` | `src/stubs/link_stubs.c:431` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `cg_hud_ptr` | `src/stubs/link_stubs.c:436` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `cg_weapinfo_ptr` | `src/stubs/link_stubs.c:449` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `CloseComponent` | `src/stubs/link_stubs.c:457` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `cl_packetdelay` | `src/stubs/link_stubs.c:458` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `cm_phys_ptr` | `src/stubs/link_stubs.c:462` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `com_checksumFeed_dvar` | `src/stubs/link_stubs.c:463` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `com_errorEntered_ptr` | `src/stubs/link_stubs.c:465` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `commandsList` | `src/stubs/link_stubs.c:466` | 64 | 0 / 7 | External link placeholder unused; candidates are a separately defined local static command list. |
| `com_statmon_ptr` | `src/stubs/link_stubs.c:468` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `CreateEvent` | `src/stubs/link_stubs.c:469` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `CreateNibReferenceWithCFBundle` | `src/stubs/link_stubs.c:470` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `CreateObjSpecifier` | `src/stubs/link_stubs.c:471` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `CreateStandardAlert` | `src/stubs/link_stubs.c:472` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `CreateWindowFromNib` | `src/stubs/link_stubs.c:473` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `d3d_context` | `src/stubs/link_stubs.c:474` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `DisableControl` | `src/stubs/link_stubs.c:476` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `dvar_ptr_195ee78` | `src/stubs/link_stubs.c:479` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `dxIter` | `src/stubs/link_stubs.c:481` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `__dyld_func_lookup` | `src/stubs/link_stubs.c:482` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `encode_vol_ptr` | `src/stubs/link_stubs.c:488` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `entityHandlers_ptr` | `src/stubs/link_stubs.c:489` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `fx_sort_ptr` | `src/stubs/link_stubs.c:527` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `fx_time_dst1` | `src/stubs/link_stubs.c:547` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `fx_time_dst2` | `src/stubs/link_stubs.c:548` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `fx_time_src1` | `src/stubs/link_stubs.c:549` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `fx_time_src2` | `src/stubs/link_stubs.c:550` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_cheats_dvar` | `src/stubs/link_stubs.c:552` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_clients_ptr` | `src/stubs/link_stubs.c:553` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_creatingTexture` | `src/stubs/link_stubs.c:554` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_deadChat_ptr` | `src/stubs/link_stubs.c:555` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_enemylookDist` | `src/stubs/link_stubs.c:557` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `GetComponentVersion` | `src/stubs/link_stubs.c:559` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetCursor` | `src/stubs/link_stubs.c:567` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetGlobalMouse` | `src/stubs/link_stubs.c:568` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetGWorldPixMap` | `src/stubs/link_stubs.c:569` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetHandleSize` | `src/stubs/link_stubs.c:570` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMainEventLoop` | `src/stubs/link_stubs.c:571` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMainEventQueue` | `src/stubs/link_stubs.c:572` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMediaHandler` | `src/stubs/link_stubs.c:573` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMediaSampleDescription` | `src/stubs/link_stubs.c:574` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMovieDuration` | `src/stubs/link_stubs.c:575` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMovieIndTrackType` | `src/stubs/link_stubs.c:576` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMoviePreferredRate` | `src/stubs/link_stubs.c:577` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMovieTime` | `src/stubs/link_stubs.c:578` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetMovieTimeScale` | `src/stubs/link_stubs.c:579` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetNextProcess` | `src/stubs/link_stubs.c:580` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetPixRowBytes` | `src/stubs/link_stubs.c:581` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetPort` | `src/stubs/link_stubs.c:582` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `GetProcessInformation` | `src/stubs/link_stubs.c:591` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetQDGlobalsArrow` | `src/stubs/link_stubs.c:592` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetStandardAlertDefaultParams` | `src/stubs/link_stubs.c:593` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetTrackMedia` | `src/stubs/link_stubs.c:594` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetWindowResizeLimits` | `src/stubs/link_stubs.c:595` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `GetWRefCon` | `src/stubs/link_stubs.c:596` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `g_friendlylookDist` | `src/stubs/link_stubs.c:597` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `gfxBuf` | `src/stubs/link_stubs.c:599` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `GoToBeginningOfMovie` | `src/stubs/link_stubs.c:601` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `g_phys_world` | `src/stubs/link_stubs.c:603` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `g_ri` | `src/stubs/link_stubs.c:605` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_sNextDmgTableId` | `src/stubs/link_stubs.c:609` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_sv_running_ptr` | `src/stubs/link_stubs.c:614` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_unknown_195f22c` | `src/stubs/link_stubs.c:617` | 64 | 2 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `g_unknown_195f230` | `src/stubs/link_stubs.c:618` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `g_useActivateHoldTime` | `src/stubs/link_stubs.c:619` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_useActivateReuseTime` | `src/stubs/link_stubs.c:620` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_vidConfig` | `src/stubs/link_stubs.c:621` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_voiceChatsAllowed_ptr` | `src/stubs/link_stubs.c:623` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `g_voiceChatTalkingDuration_ptr` | `src/stubs/link_stubs.c:624` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `HandleControlKey` | `src/stubs/link_stubs.c:625` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `HideControl` | `src/stubs/link_stubs.c:626` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `HITextViewGetTXNObject` | `src/stubs/link_stubs.c:627` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `HIViewGetRoot` | `src/stubs/link_stubs.c:628` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `HIViewGetViewForMouseEvent` | `src/stubs/link_stubs.c:629` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `InitCursor` | `src/stubs/link_stubs.c:630` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `InstallEventLoopTimer` | `src/stubs/link_stubs.c:631` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `IOBSDNameMatching` | `src/stubs/link_stubs.c:632` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `IOObjectGetClass` | `src/stubs/link_stubs.c:633` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `IOObjectRetain` | `src/stubs/link_stubs.c:634` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `IORegistryEntryCreateCFProperty` | `src/stubs/link_stubs.c:635` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `IORegistryEntryCreateIterator` | `src/stubs/link_stubs.c:636` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `IsMovieDone` | `src/stubs/link_stubs.c:637` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `jpeg_memory_src` | `src/stubs/link_stubs.c:638` | 64 | 1 / 1 | Legacy char-array function target; modern-library path uses jpeg_mem_src. Non-modern native candidate requires follow-up. |
| `kCFAllocatorDefault` | `src/stubs/link_stubs.c:639` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `loadingMessage` | `src/stubs/link_stubs.c:641` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `LockPixels` | `src/stubs/link_stubs.c:642` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `LSCopyItemInfoForRef` | `src/stubs/link_stubs.c:643` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `MediaSetSoundBalance` | `src/stubs/link_stubs.c:644` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `MoviesTask` | `src/stubs/link_stubs.c:646` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `name` | `src/stubs/link_stubs.c:647` | 64 | 23 / manual | Oversized pointer slot; initialized by Dvar_RegisterString before use; no expected NULL afterwards. |
| `NewControlEditTextValidationUPP` | `src/stubs/link_stubs.c:649` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `NewControlKeyFilterUPP` | `src/stubs/link_stubs.c:650` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `NewControlUserPaneDrawUPP` | `src/stubs/link_stubs.c:651` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `NewControlUserPaneHitTestUPP` | `src/stubs/link_stubs.c:652` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `NewControlUserPaneTrackingUPP` | `src/stubs/link_stubs.c:653` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `NewGWorld` | `src/stubs/link_stubs.c:654` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `NewHandle` | `src/stubs/link_stubs.c:655` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `NewMovieFromFile` | `src/stubs/link_stubs.c:656` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `OpenAComponent` | `src/stubs/link_stubs.c:658` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `OpenComponent` | `src/stubs/link_stubs.c:659` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `OpenMovieFile` | `src/stubs/link_stubs.c:660` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `PBGetCatInfoSync` | `src/stubs/link_stubs.c:661` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `PBHGetVolParmsSync` | `src/stubs/link_stubs.c:662` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `PostEventToQueue` | `src/stubs/link_stubs.c:664` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `PrerollMovie` | `src/stubs/link_stubs.c:669` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `ptr_195ecb4` | `src/stubs/link_stubs.c:671` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `ptr_195ecbc` | `src/stubs/link_stubs.c:672` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `ptr_195eea4` | `src/stubs/link_stubs.c:673` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `ptr_195f58c` | `src/stubs/link_stubs.c:674` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `ptr_195f5e0` | `src/stubs/link_stubs.c:675` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `QDRegisterNamedPixMapCursor` | `src/stubs/link_stubs.c:676` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `QDSetNamedPixMapCursor` | `src/stubs/link_stubs.c:677` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `QuitAppModalLoopForWindow` | `src/stubs/link_stubs.c:678` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `rcon_password_dvar` | `src/stubs/link_stubs.c:679` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `record_callback_ptr` | `src/stubs/link_stubs.c:681` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `ReleaseEvent` | `src/stubs/link_stubs.c:682` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `re_ptr_195eca8` | `src/stubs/link_stubs.c:683` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `r_gammaSetting` | `src/stubs/link_stubs.c:686` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `R_LoadSun_f` | `src/stubs/link_stubs.c:690` | 64 | 0 / 0 | Legacy char-array callback target; native function callback correction owned by callback audit. |
| `r_occlusionQuery` | `src/stubs/link_stubs.c:691` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `R_ReloadMaterialTextures_f` | `src/stubs/link_stubs.c:692` | 64 | 0 / 0 | Legacy char-array callback target; native function callback correction owned by callback audit. |
| `R_SaveSun_f` | `src/stubs/link_stubs.c:694` | 64 | 0 / 0 | Legacy char-array callback target; native function callback correction owned by callback audit. |
| `R_SmcFlush_f` | `src/stubs/link_stubs.c:695` | 64 | 0 / 0 | Legacy char-array callback target; native function callback correction owned by callback audit. |
| `R_SmcStats_f` | `src/stubs/link_stubs.c:696` | 64 | 0 / 0 | Legacy char-array callback target; native function callback correction owned by callback audit. |
| `RunAppModalLoopForWindow` | `src/stubs/link_stubs.c:698` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `RunStandardAlert` | `src/stubs/link_stubs.c:699` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `scrPlace` | `src/stubs/link_stubs.c:705` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `SetControlData` | `src/stubs/link_stubs.c:706` | 64 | 0 / 5 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `SetControlFontStyle` | `src/stubs/link_stubs.c:707` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `SetControlMaximum` | `src/stubs/link_stubs.c:708` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetControlReference` | `src/stubs/link_stubs.c:709` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetCursor` | `src/stubs/link_stubs.c:710` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetEventParameter` | `src/stubs/link_stubs.c:711` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetMovieRate` | `src/stubs/link_stubs.c:719` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetMovieTimeValue` | `src/stubs/link_stubs.c:720` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetPort` | `src/stubs/link_stubs.c:721` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `SetThemeCursor` | `src/stubs/link_stubs.c:722` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetTrackVolume` | `src/stubs/link_stubs.c:723` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetWindowResizeLimits` | `src/stubs/link_stubs.c:724` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `SetWRefCon` | `src/stubs/link_stubs.c:725` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `ShowControl` | `src/stubs/link_stubs.c:726` | 64 | 0 / 2 | Unsupported legacy Carbon utility function target; compiled call candidate remains. Reachability unverified. |
| `_snd_local_listener` | `src/stubs/link_stubs.c:728` | 64 | 6 / 0 | Legacy zero int read was NULL origin; root mapped native to vec3_origin. |
| `speex_nb_mode_ptr` | `src/stubs/link_stubs.c:729` | 64 | 2 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `speex_quality_ptr` | `src/stubs/link_stubs.c:730` | 64 | 2 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `speex_uwb_mode_ptr` | `src/stubs/link_stubs.c:731` | 64 | 2 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `speex_wb_mode_ptr` | `src/stubs/link_stubs.c:732` | 64 | 2 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `StartMovie` | `src/stubs/link_stubs.c:733` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `StopMovie` | `src/stubs/link_stubs.c:734` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `sv_cheats_ptr` | `src/stubs/link_stubs.c:736` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `s_vc_logCount` | `src/stubs/link_stubs.c:737` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `sv_com_dvarDump_ptr` | `src/stubs/link_stubs.c:738` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `sv_dedicated_dvar2` | `src/stubs/link_stubs.c:740` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `sv_privatePassword_dvar` | `src/stubs/link_stubs.c:742` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `sv_showcommands_dvar` | `src/stubs/link_stubs.c:744` | 64 | 0 / 0 | Link-only zero placeholder; no explicit source reference found. Mapping absent; retained to preserve legacy build. |
| `tr` | `src/stubs/link_stubs.c:746` | 64 | 4 / manual | Fake independent renderer counter; root mapped native to rg.markCount. |
| `TXNSetTypeAttributes` | `src/stubs/link_stubs.c:748` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `version` | `src/stubs/link_stubs.c:764` | 64 | 2 / manual | Oversized pointer slot; initialized by Dvar_RegisterString before use; no expected NULL afterwards. |
| `voice_freq_ptr` | `src/stubs/link_stubs.c:765` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `voice_maxframe_ptr` | `src/stubs/link_stubs.c:766` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `voice_scale_ptr` | `src/stubs/link_stubs.c:767` | 64 | 1 / 0 | Legacy zero pointer/scalar placeholder; native reads replaced by verified real mapping above. |
| `WaitNextEvent` | `src/stubs/link_stubs.c:768` | 64 | 0 / 1 | Legacy API char-array function target; native candidates are header declarations only. No compiled call found. |
| `g_dip_vs_null` | `src/stubs/link_stubs.c:820` | 4 | 1 / 1 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_dip_vs_bound` | `src/stubs/link_stubs.c:821` | 4 | 1 / 1 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_dip_vs_skip` | `src/stubs/link_stubs.c:822` | 4 | 1 / 1 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `g_draw_count` | `src/stubs/link_stubs.c:823` | 4 | 0 / 0 | Actual int instrumentation counter; initialized scalar, not a pointer or function target. |
| `material_tech_string` | `src/stubs/material_tech_names.c:50` | 1024 | 0 / 0 | Zero string buffer with link-name string; no explicit C reference found. Three conditional definitions retained. |
| `material_tech_string` | `src/stubs/material_tech_names.c:54` | 1024 | 0 / 0 | Zero string buffer with link-name string; no explicit C reference found. Three conditional definitions retained. |
| `material_tech_string` | `src/stubs/material_tech_names.c:56` | 1024 | 0 / 0 | Zero string buffer with link-name string; no explicit C reference found. Three conditional definitions retained. |
| `COpenGL_sOpenGLE` | `src/stubs/missing_c_syms.c:25` | 4 | 8 / 0 | Legacy undersized object storage; existing native guard maps to actual singleton or sized packet/tree storage. |
| `CVAOPacket_sAllPackets` | `src/stubs/missing_c_syms.c:26` | 64 | 8 / 0 | Legacy undersized object storage; existing native guard maps to actual singleton or sized packet/tree storage. |
| `CVAOPacket_sCurrentPacket` | `src/stubs/missing_c_syms.c:27` | 64 | 4 / 4 | Oversized int array holds one valid initialized scalar; no pointer dereference or NULL read. |
| `CVAOPacket_sGenericPacket` | `src/stubs/missing_c_syms.c:28` | 64 | 4 / 0 | Legacy undersized object storage; existing native guard maps to actual singleton or sized packet/tree storage. |
| `CVAOPacket_sVAOStatus` | `src/stubs/missing_c_syms.c:29` | 64 | 4 / 4 | Oversized int array holds one valid initialized scalar; no pointer dereference or NULL read. |
