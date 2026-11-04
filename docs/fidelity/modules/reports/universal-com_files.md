# universal/com_files module reconciliation

Updated 2026-11-04.

**84/92 reconciled: 68 equal, 6 fixed, 10 intentional; 8 unchecked.** All 62 assigned Mac entries were compared with native source. Linux-only helpers were checked in emitted Mac callers where their identity could be established. Mac 1.3 is the authority; Windows was also checked for path separator policy. Bundled unzip code was read only to establish engine-wrapper contracts.

## Corrections

`FS_BuildOSPath_Internal` converted colons to slashes. Mac 0x332e8–0x33317 only collapses slash and backslash. Windows 0x421bf0–0x421c28 agrees about colons, while emitting its host backslash. Native Mac keeps its host slash.

`FS_DisplayPath` printed pure-list membership when no server pure list existed. Mac 0x335af skips that diagnostic. Its open-handle listing now tests the full native FILE pointer, matching the pointer test at 0x33614.

The inlined IWD loader stops the name-sizing pass after the first metadata error at Mac 0x34e9e. Native continued. The invalid-language warning now lists all 15 language IDs, matching 0x34fcd. The loader and separator helper each have an additional Linux-only ledger alias, accounting for six fixed rows from four changed Mac functions.

`FS_FOpenFileByMode` now writes `handleSync` to the defined reserved `fsh[0]` slot after failed opens. Mac 0x3681c–0x3684e guards `fileSize` and `streamed` separately from that write.

## Retained native behavior

The selected default configuration already added by the common-module audit remains unchanged. Full reads of `FS_InitFilesystem` and `FS_Restart` agree with Mac 0x37346/0x373aa and 0x3711a: prefer `default_localize_mp.cfg`, fall back to `default_mp.cfg`, then validate the selected name on restart.

Native font aliases remain: `UI_RegisterAssets` supplies `$font` and other role names, which pass through `CL_RegisterFont`, `R_RegisterFont` and `R_LoadFont` to `FS_FOpenFileRead`. The filesystem's existing alias branch resolves those names to `fonts/*`. Writable-home mounts, dedicated sound/CD-key omissions and optional CoD2x policy hooks are recorded as intentional.

ZIP metadata selection uses typed stream ownership rather than copying retail's 128-byte unzip structure and mutating its master cursor. Short-name reference exemptions retain their bounds check instead of reproducing retail stack underreads.

`FS_Seek` retains its bounded 4096-byte scratch discard. Retail 0x32f63 supplies NULL output to `unzReadCurrentFile`; 0x27037 stores it and the stored-member loop writes through it at 0x27097. There is no valid discard-storage contract to reproduce. Native rejects nonpositive skips, consumes the requested positive count and reports partial/error failures; retail instead tests the truthiness of one backend return. These cursor and return differences remain explicit intentional adaptations. No null writes or discarded backend were reconstructed.

## Unresolved identities

The assigned Linux definitions of `FS_FileCompare`, `FS_IsBaseGameDirOfType`, `FS_ListFilesOfDirType`, `FS_ListFilteredFilesOfDirType`, `FS_OpenFileForWrite`, `FS_Rename`, `FS_SetWritable` and `FS_ShiftedStrStr` have no settled emitted or inline Mac identity and no native body/caller. They remain unchecked. Their absence is not an equality claim.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with `taskpolicy -b nice -n 19` and two jobs. Changed files have byte-identical preprocessor output against the base when `COD2_X64` is undefined. `git diff --check` passes. No tests were added, and no runtime, CI or replay checks were run. Corrections are compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-files.noindex/`.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
