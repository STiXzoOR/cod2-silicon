# client/cl_cgame_mp module reconciliation

Updated 2026-11-04.

**71/71 reconciled: 62 equal, 8 fixed, 1 intentional.** This includes the complete `CL_LookupColor` body at Mac 0x17b7c6, whose ownership is being moved from `universal/com_vector` by the parent. Global ledgers were not edited. Mac 1.3 i386 is the authority; no Windows-specific behavior decision was needed. Legacy production and fixture bodies remain unchanged when the native guard is off.

## Corrections

`CL_ConfigstringModified` at 0x17a2c0 uses a 0x22004-byte large-local gamestate copy and permits 0x20000 string bytes. Native used a stack copy and rejected strings above the older 0x3e80 limit. The native type really has 131,072 string bytes, and its large-local pool is 1 MiB. The allocation, release, limit and two diagnostics now follow Mac.

`CL_GetServerCommand` at 0x17a4c4 lacked the `w ... PB` disconnect branch at 0x17a635, which preserves the untranslated reason and formats the control-prefixed error. The branch and the reliable-sequence, multipart-overflow and default-disconnect diagnostics now match. All 57 jump-table entries at 0x33b880 were decoded. This engine-side disconnect handling remains useful with the documented null PunkBuster backend.

`CL_Popup` at 0x17aeac now recognizes `UIMENU_WM_QUICKMESSAGE` and `UIMENU_SCRIPT_POPUP_NO_MOUSE`, with the Mac state/demo/fullscreen gate order. `CL_ClosePopup` at 0x17af5e now requires active menu 9 and closes the focused menu. Native used different names, also accepted menu 10, and closed all menus.

`CL_UpdateLevelHunkUsage` at 0x17b0b8 now returns when the stored usage already matches, as 0x17b1bf does. Native appended another entry. The corrupt-file and append-open diagnostics also match.

`CL_SetCGameTime` now uses `r_mode` for timedemo filenames, as 0x17be3c does, and restores both error texts. `CL_GetSnapshot` restores the warning asset `gfx/2d/warning@snapshotents.jpg` at 0x17a123 and its error text. `CL_GetUserCmd` restores the control-prefixed diagnostic at 0x179fdf.

## Preserved native behavior

`CL_AdjustTimeDelta` was compared in full. Mac 0x17b5d6–0x17b5de wraps the 32-bit sum before shifting it right. Native keeps the wide average introduced by 9514a8c: a measured CoD2x server epoch near 1.56 billion milliseconds otherwise repeatedly reset client time. The existing positive/negative epoch fixture remains unchanged. The two Mac debug prefixes were restored.

Renderer forwarding was checked against the actual compiled native layout and `GetRefAPI` bindings. Most relevant callback offsets double on LP64. `SetLodOrigin` is different: Mac API 60 uses offset 0x158, while native API 59 has separate `SyncGpu` and `GpuWaited` fields and binds `SetLodOrigin` at byte 696. Its semantic forwarding is correct. The direct native stretch-picture and quad-picture paths call the same command builders selected by their renderer slots. No shared-header change was needed.

`CL_InitCGame` at 0x17b412 and `CL_ShutdownCGame` at 0x17aa04 match their full lifecycle bodies. Initialization has no pause reset. Its existing `CG_Init` argument-order fix is retained. The parent/main-client worker received the lifecycle and callback-tail evidence. `CL_FirstSnapshot` was checked inside both `CL_SyncTimes` and `CL_SetCGameTime`; the native timedemo helper was compared with every corresponding Mac block. `CL_LookupColor` includes byte conversion, indexed colors, team colors and white fallback; all 32 floats in the first eight table rows match.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built with `taskpolicy -b nice -n 19` and two jobs. The regular client cgame object also compiled with its generated native flags. Existing `server_commands`, `time_delta` and `cgame_init` fixtures passed under ASan/UBSan; all 13 existing client-clock cases passed. Only the clock fixture's selector expectation and the server-command fixture's required service stubs changed. No new tests, full CI, replays or pushes were run.

The guard checker reports only the three expected native-on differences. A separate comparison without replacing `__LINE__` confirms unchanged production preprocessing in all five port-off configurations; the changed production source contains no `__LINE__` uses. `git diff --check` passes.

Configstring rebuilding, popup interaction, hunk-file I/O, PB disconnect/overflow/lost-command branches and the changed user-command/snapshot diagnostics were compiled and reviewed but not exercised. No entries remain unchecked. Exact-entry disassembly was collected for all 71 bodies; it corrected the full-section tool's misaligned `CL_GetScreenDimensions` prologue. No original disassembly or binary data is committed.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-cgame.noindex/`.
