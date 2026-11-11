# client/cl_cgame_mp module reconciliation

Updated 2026-11-04.

**71/71 reconciled: 62 equal, 8 fixed, 1 intentional.** This includes the complete `CL_LookupColor` body at Mac 0x17b7c6, whose ownership is being moved from `universal/com_vector` by the parent. Global ledgers were not edited. Mac 1.3 i386 is the authority; Windows popup routing was checked against the native UI and updater boundary. Legacy production and fixture bodies remain unchanged when the native guard is off.

## Corrections

`CL_ConfigstringModified` at 0x17a2c0 uses a 0x22004-byte large-local gamestate copy and permits 0x20000 string bytes. Native used a stack copy and rejected strings above the older 0x3e80 limit. The native type really has 131,072 string bytes, and its large-local pool is 1 MiB. The allocation, release, limit and two diagnostics now follow Mac.

`CL_GetServerCommand` at 0x17a4c4 lacked the `w ... PB` disconnect branch at 0x17a635, which preserves the untranslated reason and formats the control-prefixed error. The branch and the reliable-sequence, multipart-overflow and default-disconnect diagnostics now match. All 57 jump-table entries at 0x33b880 were decoded. This engine-side disconnect handling remains useful with the documented null PunkBuster backend.

`CL_Popup` at 0x17aeac now recognizes `UIMENU_WM_QUICKMESSAGE` and `UIMENU_SCRIPT_POPUP_NO_MOUSE`, with the Mac state/demo/fullscreen gate order. `CL_ClosePopup` at 0x17af5e now requires active menu 9 and closes the focused menu. Native used different names, also accepted menu 10, and closed all menus.

`CL_UpdateLevelHunkUsage` at 0x17b0b8 now returns when the stored usage already matches, as 0x17b1bf does. Native appended another entry. The corrupt-file and append-open diagnostics also match.

`CL_SetCGameTime` now uses `r_mode` for timedemo filenames, as 0x17be3c does, and restores both error texts. `CL_GetSnapshot` restores the warning asset `gfx/2d/warning@snapshotents.jpg` at 0x17a123 and its error text. `CL_GetUserCmd` restores the control-prefixed diagnostic at 0x179fdf.

## Preserved native behavior

Windows `CL_Popup` at 0x4024f2 also routes `UIMENU_WM_AUTOUPDATE` through menu 9; its handler at 0x5386c9 opens `autoupdate`. Native has no such enum, handler or caller: its 9/10 script menu types normalize to 9. The existing platform plan excludes auto-updater work (`docs/macos-port/PLAN.md` and `cod2x-compat.md`); no downloaded-binary updater or disconnected menu is reconstructed here. This is an explicit platform boundary, not a claim that the Windows popup body is identical.

`CL_AdjustTimeDelta` was compared in full. Mac 0x17b5d6–0x17b5de wraps the 32-bit sum before shifting it right. Native keeps the wide average introduced by 9514a8c: a measured CoD2x server epoch near 1.56 billion milliseconds otherwise repeatedly reset client time. The existing positive/negative epoch fixture remains unchanged. The two Mac debug prefixes were restored.

Renderer forwarding was checked against the actual compiled native layout and `GetRefAPI` bindings. Most relevant callback offsets double on LP64. `SetLodOrigin` is different: Mac API 60 uses offset 0x158, while native API 59 has separate `SyncGpu` and `GpuWaited` fields and binds `SetLodOrigin` at byte 696. Its semantic forwarding is correct. The direct native stretch-picture and quad-picture paths call the same command builders selected by their renderer slots. No shared-header change was needed.

`CL_InitCGame` at 0x17b412 and `CL_ShutdownCGame` at 0x17aa04 match their full lifecycle bodies. Initialization has no pause reset. Its existing `CG_Init` argument-order fix is retained. The parent/main-client worker received the lifecycle and callback-tail evidence. `CL_FirstSnapshot` was checked inside both `CL_SyncTimes` and `CL_SetCGameTime`; the native timedemo helper was compared with every corresponding Mac block. `CL_LookupColor` includes byte conversion, indexed colors, team colors and white fallback; all 32 floats in the first eight table rows match.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built with `taskpolicy -b nice -n 19` and two jobs. The regular client cgame object also compiled with its generated native flags. Existing `server_commands`, `time_delta` and `cgame_init` fixtures passed under ASan/UBSan; all 13 existing client-clock cases passed. Only the clock fixture's selector expectation and the server-command fixture's required service stubs changed. No new tests, full CI, replays or pushes were run.

The guard checker reports only the three expected native-on differences. A separate comparison without replacing `__LINE__` confirms unchanged production preprocessing in all five port-off configurations; the changed production source contains no `__LINE__` uses. `git diff --check` passes.

Configstring rebuilding, popup interaction, hunk-file I/O, PB disconnect/overflow/lost-command branches and the changed user-command/snapshot diagnostics were compiled and reviewed but not exercised. No entries remain unchecked. Exact-entry disassembly was collected for all 71 bodies; it corrected the full-section tool's misaligned `CL_GetScreenDimensions` prologue. No original disassembly or binary data is committed.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-cgame.noindex/`.

## Integrated batch verification

Integrated source `ca4ed64` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
