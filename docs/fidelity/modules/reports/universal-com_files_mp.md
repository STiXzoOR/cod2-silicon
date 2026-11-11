# universal/com_files_mp module reconciliation

Updated 2026-11-04.

**27/27 reconciled: 19 equal, 6 fixed, 2 intentional.** All 17 assigned Mac entries were compared by hand. Linux aliases for command callbacks, referenced-IWD parsing and inline helpers were tied to emitted Mac bodies or callers; already integrated callback/parser corrections remain unchanged.

## Corrections

`FS_GetModList` omitted the NUL bytes from both packed-string lengths, overwriting the separators between mod names, descriptions and following entries. Mac 0x47785 and 0x47861 include them. The inline title at 0x47803–0x47848 is `Call of Duty 2 Multiplayer`; native had `Call of 2 Multiplayer`.

`FS_CompareIwds` omitted the case-insensitive `_svr_` exclusion at Mac 0x47bae–0x47be8. Native now scans without creating retail's unbounded temporary copy. Human-readable output also regains the wrong-checksum explanation at 0x47cda and newline at 0x47cf4. The Linux `FS_svrIwd` alias accounts for the sixth fixed row.

`FS_SetRestrictions` now uses the demo-mode diagnostic and `demomain` directory from Mac 0x46bd2 and 0x46bea. Its checksum validation remains 0xb1f595f5. `FS_PureServerSetLoadedIwds` regains the exact mismatch and connected-to-pure-server messages at 0x4710f and 0x47181.

Exact-symbol disassembly of `FS_SV_FOpenFileRead` confirms a retail quirk: after a distinct base-path attempt fails, 0x46685 zeroes the handle and 0x4655e recomputes its table entry. The subsequent CD-path attempt uses the defined reserved `fsh[0]`, and the returned handle remains zero even if that open succeeds. Native now follows that deterministic behavior.

## Retained native behavior

The CoD2x stock-IWD classifier remains an intentional version policy. The Linux-only `FS_Read2` has no native caller or emitted Mac identity; native Mac uses the synchronous `Sys_StreamedRead` to `FS_Read` platform adapter and no-op begin/end stream operations. The ledger names that replacement rather than claiming a missing function equal.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. Both changed files retain identical preprocessor output with `COD2_X64` undefined, and `git diff --check` passes. No tests were added and no runtime, CI or replay checks were run. Corrections are compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-files.noindex/`.

## Integrated batch verification

Integrated source `cf0fc8c` with cross-module evidence commit `e785f19` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `f763fd9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
