# universal/com_file_wrapper module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 5 equal.** The engine wrappers at Mac 0x373d8, 0x373e2, 0x373ec, 0x373f6 and 0x37400 forward directly to `fread`, `fwrite`, `fopen`, `fclose` and `fseek`. Native `src/PC/qcommon/com_fileaccess.c` forwards the same arguments and returns using the host stdio types. These engine wrappers are in scope; upstream stdio implementations are not.

The `FS_FileRead` prologue was rechecked with exact-symbol disassembly because full-binary disassembly misaligned its entry. No wrapper changes were needed. The arm64 Release dedicated target compiled; no runtime or replay checks were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-files.noindex/`.

## Integrated batch verification

Integrated source `cf0fc8c` with cross-module evidence commit `e785f19` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `f763fd9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
