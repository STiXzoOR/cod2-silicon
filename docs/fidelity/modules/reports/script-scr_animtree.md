# script/scr_animtree module reconciliation

Updated 2026-11-05.

**20/21 reconciled: 12 equal, 4 fixed, 4 intentional.** Mac 1.3 i386 is the primary authority. Every named Mac body was read, including all of `AnimTreeParseInternal` (0x9c30a–0x9c928) and `Scr_LoadAnimTreeAtIndex` (0x9c928–0x9ccf0). The constant checker and triage ran for all assigned names. The property table at 0x343c00 was decoded: `loopsync`, `nonloopsync`, `complete`, with flags 1, 2, 8. Referenced parser, filename and error strings also match.

The LP64 animation reference encoder silently returned zero after 16,383 external pointers. Mac links every reference directly in `Scr_EmitAnimation` at 0x9c2c8–0x9c2cc and `Scr_FindAnim` at 0x9c268–0x9c26c. Native now grows the external pointer table, checks the bit-31 index limit and allocation-size arithmetic, and preserves its previous allocation on `realloc` failure. The existing reset runs after variable-object release and now frees the table. Four fixed rows cover the inline encoder and its three callers, including the stored XAnim pointer in `Scr_LoadAnimTreeAtIndex` at 0x9cb97–0x9cbba. This is one shared storage defect.

Three intentional rows describe decoding four-byte animation links and XAnim pointers on LP64. The fourth preserves the existing 128-tree error/clamp. Each user has valid loaded indices 1–127. In Mac `scrAnimGlob`, the lookup starts at +0x8 and `bAnimCheck` is +0x208: user-0 index 128 aliases user-1 index 0, while user-1 index 128 overwrites `bAnimCheck`. The corresponding native offsets are +0x10 and +0x210. Later XAnim publication likewise aliases user-1 index 0 or `xanim_num[0]` (Mac lookup +0xc/count +0x40c; native +0x10/+0x810). The parent explicitly directed retaining defined native bounds instead of reproducing this adjacent-storage corruption.

The Linux-only parse/error/property/defined-animation helpers were compared in their identified Mac callers. `Scr_GetAnimsIndex` remains unchecked: Mac has only a zero-address absolute `.eh` symbol, and native has neither a body nor callers. No executable inline range was established.

The arm64 Release `cod2_macos_ded` target compiled and linked with two low-priority jobs; the final incremental build recompiled both assigned files without source warnings. Table growth, allocation failure and high-reference counts were not exercised. No new tests, CI or replays ran. Legacy source remains in the `#else` branches, and `unifdef -U COD2_X64` confirms the selected OFF source is byte-identical to the starting source. No shared headers or other units were edited.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-script-main.noindex/`. All 21 IDs are recorded in the private `script-main.ledger.json`.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.

## Remaining-helper audit, 2026-11-05

Original Mac source metadata, executable tree routines and native call sites were checked for the remaining reverse lookup. The zero-address `.eh` symbol identifies `Scr_GetAnimsIndex(XAnim const*)` but supplies no executable range. The existing forward `Scr_GetAnims(index)` operation and tree-name lookup do not establish the descending pointer-search helper. It remains unchecked. Native code is unchanged.

The assigned files compiled as actual configured stock and CoD2x arm64 Release client objects in a separate evidence directory, with low priority and at most two jobs. No source or fixture was edited. Previously resolved ledger evidence is preserved exactly in the full `script-remaining.ledger.json`. No full CI or replay ran.

Independent source/binary review at source freeze `28e7d452399e38b2c296b8b4230e3eb8017b8c52` agreed with the two compiler overload closures and sixteen remaining gaps. Eight existing compiler/parser cases passed in each stock and CoD2x configuration: four binary-condition loop cases and four parser cases, using the existing ASan/UBSan runner. These checks do not exercise all reference-count types or the unresolved debugger APIs. No new cases or assertions were added.


## Original reverse-lookup identity

Original1.0 IDB body 0x806d034 uniquely relocates to original1.3 0x806d67c: start at the server animation count, compare animation pointers while descending, and return a matching index or zero. This establishes the original reverse helper's complete contract. Fresh exact Mac `Scr_UsingTreeInternal` at 0x9c020 instead searches ushort variable IDs upward at 0x9c0db–0x9c11e, and `Scr_GetAnims` at 0x9bdca only indexes the animation table forward. Neither supplies the descending animation-pointer search. `Scr_GetAnimsIndex` remains unchecked; no source or settled verdict changed, and no compilation, tests, CI or replay ran.
