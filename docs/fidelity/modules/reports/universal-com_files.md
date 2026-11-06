# universal/com_files module reconciliation

Updated 2026-11-05.

**85/92 reconciled: 69 equal, 6 fixed, 10 intentional; 7 unchecked.** All 62 assigned Mac entries were compared with native source. Linux-only helpers were checked in emitted Mac callers where their identity could be established. Mac 1.3 is the authority; Windows was also checked for path separator policy. Bundled unzip code was read only to establish engine-wrapper contracts.

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

The assigned Linux definitions of `FS_FileCompare`, `FS_IsBaseGameDirOfType`, `FS_ListFilesOfDirType`, `FS_ListFilteredFilesOfDirType`, `FS_Rename`, `FS_SetWritable` and `FS_ShiftedStrStr` have no settled emitted or inline Mac identity and no native body/caller. They remain unchecked. Their absence is not an equality claim. The `FS_OpenFileForWrite` row maps to the complete `FS_OpenFileOverwrite` operation through the BSP save caller, as established below.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with `taskpolicy -b nice -n 19` and two jobs. Changed files have byte-identical preprocessor output against the base when `COD2_X64` is undefined. `git diff --check` passes. No tests were added, and no runtime, CI or replay checks were run. Corrections are compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-files.noindex/`.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.

## Write-open identity audit, 2026-11-05

The Linux-only `FS_OpenFileForWrite` row now has a complete Mac/native mapping under the name `FS_OpenFileOverwrite`. The Linux reconstruction supplies the API name and its sole callsite in `CM_SaveLump`; its constant-zero stub is not behavioral authority. This is a caller-based identity, not a claim that the differently named `FS_FOpenFileWrite` is equivalent.

Exact-symbol Mac `CM_SaveLump` occupies `[0x73b06,0x73daf)`. After reading the old BSP, checking version 4 or 61 and copying its header, `[0x73be8,0x73bf5)` passes the current `cm.name` to `FS_OpenFileOverwrite`. The returned integer handle is stored at 0x73bf5 and tested at 0x73bf8. Zero takes `[0x73d87,0x73daf)`, which reports the write-open error and returns. Success retains that same handle for the 0x140-byte header, 39 aligned lump writes and close; the optional reread/checksum follows. Native `qcommon/cm_load.c:164-257` preserves this complete argument, failure, handle-use and return contract, including the prior native error-return corrections. The corresponding Linux call occupies this same position in its complete BSP rewrite operation.

The callee's original STABS names `FS_OpenFileOverwrite` at 0x342a2 and gives its full extent as `[0x342a2,0x34411)`. The complete exact-symbol body was re-read against native `universal/com_files.c:1476-1524`: resolve an existing loose-file OS path and report lookup failure; optionally log it; clear the read-only attribute bit when needed; open with `wb`; return zero on open failure; find a free main handle with the original exhaustion diagnostic; clear its ZIP pointer, store FILE/name/sync fields, and return the handle. This agrees with the existing settled primary Mac row, whose verdict is unchanged. No source edit was needed.

The other seven identities remain unchecked. In particular, `FS_FileCompare` claims file-content comparison, so the path-string comparator is a different contract. Mac `FS_SV_Rename` at `[0x466c0,0x467ba)` uses root-relative paths and one rename followed by copy/remove; it does not establish the gamedir-relative rename/remove/retry API. Mac `FS_ShiftStr` at `[0x467ba,0x46820)` returns shifted static storage without a substring search, and the complete `CL_InitDownloads` caller discards that return. Ordinary `FS_ListFiles` passes the existing search chain directly; it does not construct and free the directory-type-filtered chain. The overwrite helper's read-only-bit operation likewise does not establish the separate writable setter's full contract.

This follow-up used source and binary review only. No source changes, builds, tests, CI, replays or Actions were run; the newly mapped operation is uncompiled and unexercised in this pass. Private exact-symbol evidence is retained under `~/Library/Application Support/CoD2-native-decomp/recon-fast-files-sound.noindex/`.


## Original identity follow-up — provenance-config7

Original Linux 1.0 IDB addresses were mapped to retained original 1.3 ELF bodies using source order, independent decompiler anchors and masked whole-body comparisons. Every unique whole-body match agrees with the mapped module order; ambiguous tiny bodies and changed implementations were located from neighboring unique anchors and inspected separately. An ambiguous non-target `FS_Path_f` comparison was not used as an anchor. The private packet retains the mapping alternatives and exact bodies. These addresses establish original identity and intent only; Mac i386 remains behavior authority.

| Helper | Original 1.0 | Original 1.3 | Verdict |
|---|---|---|---|
| `FS_FileCompare` | 0x809d1c6 | 0x809f3c8 | unchecked |
| `FS_IsBaseGameDirOfType` | 0x809f830 | 0x80a1a3a | unchecked |
| `FS_ListFilesOfDirType` | 0x809f7f2 | 0x80a19fc | unchecked |
| `FS_ListFilteredFilesOfDirType` | 0x809f98a | 0x80a1b94 | unchecked |
| `FS_Rename` | 0x809cb58 | 0x809ed5a | unchecked |
| `FS_SetWritable` | 0x809e256 | 0x80a0458 | unchecked |
| `FS_ShiftedStrStr` | 0x809d424 | 0x809f626 | unchecked |

Positive original chain: 0x80a19fc calls FS_ListFilteredFilesOfDirType 0x80a1b94 at 0x80a1a33, which calls FS_IsBaseGameDirOfType 0x80a1a3a at 0x80a1c02. All three exact original bodies are retained, but the complete Mac directory-type-filtering owner is still unresolved. The seven rows remain unchecked. Original bodies do not settle the Mac full-path comparison, game-directory rename/remove/retry, shifted substring search or writable-policy operations. Related FS_SV_Rename, generic listing and overwrite read-only-bit handling cannot stand in for those full contracts.

No production source changes, defects, compilation, tests, CI or replays in this follow-up. No runtime behavior is certified. Private evidence: `recon-provenance-config7.noindex`; focused ledger preserves all previously resolved rows unchanged.

## Complete-provider boundary review (config13)

All seven remaining rows stay unchecked. Original FileCompare0x809f3c8 includes complete file open/length/read/allocation/compare/cleanup behavior; neither a path comparator nor memcmp is its provider. SetWritable0x80a0458 requires path/stat, conditional mode-bit update, chmod and Boolean failure/result behavior, beyond the settled overwrite operation. The retained rename/root-path and shifted-string/download-owner rejections remain unchanged. The directory-type list wrapper still depends on the unresolved filtered-chain operation; it cannot be equated with generic listing. Prior original and exact Mac packets were reused instead of repeating absent-name/caller inventories. No filesystem source change or new verification/runtime claim.

## Actual rename producer/consumer review (rename21)

`FS_Rename` remains unchecked after reading fresh full Mac `FS_SV_Rename`, `CL_WWWDownload`, its `CL_ParseWWWDownload` path producer, and the complete path/copy/remove providers. The original body at 0x809ed5a–0x809ee52 resolves two game-directory-relative qpaths and, after failed rename, removes the destination and retries before copy/remove-source. Mac server rename at 0x4676a instead uses homepath/input/empty-qpath with stripped trailing separators and goes directly to copy/remove-source.

The other actual rename import at 0x162754 belongs to download completion. Its producer saves the local download name before replacing it with the URL, and prepares the temporary OS path at 0x1d5908. Completion builds the saved local destination at 0x16271c; failure directly copies at 0x1627e9 and removes source at 0x1627f1 before clearing/advancing download state. This is a concrete complete owner, but its path roles and failure effects do not establish the unresolved arbitrary-qpath wrapper. No invented game-directory prefix adapter or success-only comparison closes it. The complete copy provider can return without installing the destination; removal after its return does not imply transactional success.

Linux identifies the original contract, not required Mac behavior. A future positive same-operation identity could establish platform-specific fallback; these separately identified owners do not supply that bridge. The shifted substring and writable-policy gaps from config13 remain unchanged. Fresh full exact bodies and original range receipt are retained in recon-provenance-rename21.noindex. No source changed, no new defect was found, and no compilation, tests, CI, replay or runtime verification ran.
