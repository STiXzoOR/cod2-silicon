# universal/com_shared module reconciliation

Updated 2026-11-04.

**7/7 reconciled: 5 equal, 2 fixed.** The two fixed rows describe `Com_Filter` and its inlined `Com_StringContains` helper. Mac 1.3 is the authority; legacy source is byte-for-byte unchanged after removing `COD2_X64` branches with `unifdef`.

## Corrections

After `*`, Mac `Com_Filter` at 0x47f84 compares each remaining name suffix with the complete literal chunk. Its function-pointer slots at 0x1accd75 and 0x1accd79 resolve to `I_strcmp` at 0x2937c and `I_stricmp` at 0x295ee. Native incorrectly used length-limited prefix comparisons. This preserves the Mac quirk: `*a*` does not match `ab`, although a conventional glob implementation would accept it.

Bracket handling also differed. Mac 0x48144 rejects a closing `]` unless it is doubled; native reversed that condition. After a successful match, native advanced filter/name twice; Mac 0x48107 through 0x48117 advances once. Mac 0x481e4 permits `]` as a range endpoint when another `]` follows. Examples from those branches: `[a]z` must reject `ax`, `[]]` accepts `]`, and `[A-]]` accepts `Z`.

The entire Mac wildcard/case/bracket flow was compared. The Linux helper row is the inline suffix-search loop at 0x47ffa and the match continuation at 0x48254, not a separate Mac body.

## Verification and limits

`Com_FilterPath` matches the Mac's 63-byte truncation and slash conversion; hash signed-byte weighting and shifts, nine-field local-time copy, memcpy and memset argument order also match. The full-binary disassembler misaligned the `Com_HashKey` prologue, so its exact-symbol disassembly was used.

The arm64 Release dedicated target builds. Triage, constant checks, legacy-source comparison and `git diff --check` ran. The filter corrections are compiled and reviewed against Mac, not exercised. No new tests, full CI or replay were run.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-memory.noindex/`.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
