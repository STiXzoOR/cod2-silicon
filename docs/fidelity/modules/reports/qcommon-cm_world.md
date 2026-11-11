# qcommon/cm_world module reconciliation

Updated 2026-11-05.

**28/28 reconciled: 27 equal, 1 fixed.** The original 24-row pass read the Mac 1.3 bodies at 0xaf49c-0xb13ca alongside native source, including the recursive helpers that have Mac symbols but Linux-only ledger rows. Call/string triage and literal checks were prompts for that comparison. No code changes were needed in that initial pass; the 2026-11-05 follow-up below records the overlap correction.

The comparison covers unlinking and content propagation, area bounds and capacity exits, all point/sight/move traversals, insertion, sector allocation, sorting and initial world linkage. The Linux names `CM_AddEntityToNode` and `CM_AddStaticModelToNode` map to native insertion helpers and Mac inline blocks. `CM_ClearWorld`, `CM_LinkStaticModel` and `CM_LinkAllStaticModels` are inlined into `CM_LinkWorld` at 0xb0cc2.

Examples checked include the existing one-based entity/model lists, propagation of the full sight hit result, reciprocal-then-multiply clipping fractions, midpoint operation order, fraction pruning only on split paths, and the 512-unit sector threshold. `CM_SortNode` at 0xb0708 uses the supplied bounds when allocating either child; the existing native `COD2_X64` path preserves that behaviour. The 1024-sector free list and root midpoint initialization match the Mac world setup.

## Verification and limits

The apparent extra native unlink calls are Mac-inline unlink bodies; the apparent missing native memset is compiler expansion. Attribute-decorated helpers missed by the source-view extractor were read directly from source. There were no shared-header changes or unresolved evidence gaps.

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.

## Integrated batch verification

Integrated source through `40d92c3` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.

## Routed identities

Separately indexed Mac rows moved from universal headers were resolved only where the exact symbol/address and existing complete-body audit established identity with an already reconciled Linux-indexed row. The source matches that audited implementation. Both IDs and original provenance remain; these are duplicate work items, not new implementations. Counts after routing, before the 2026-11-05 follow-up: 28 equal, 0 fixed, 0 intentional, 0 unchecked.

## Math-helper follow-up — 2026-11-05

Current accounting is **27 equal, one fixed; none unchecked**. Complete
`BoundsOverlap` comparison in `CM_AreaEntities_r` found that Mac
0xaf6d2–0xaf729 rejects only strictly separated bounds with six `UCOMISS/JA`
branches, accepting touching and unordered pairs. Native `<=` conjunctions
rejected unordered pairs. The `COD2_X64` condition now uses the corresponding
negated-`>` predicates, preserving all finite comparisons and their axis order.
The complete owner row changes from equal to fixed with its historical
evidence retained and this correction appended by explicit parent approval.

The configured stock/CoD2x arm64 Release client and dedicated object compiles
pass. The unchanged production `world_links` fixture passes both modes under
ASan; it exercises finite queries, not the unordered correction. Five inactive
source configurations and byte-for-byte `unifdef -UCOD2_X64` output match the
base; this source has no `__LINE__` uses. No tests were added. The exceptional
path is compiled and reviewed only; fresh review, full links, CI and replays
remain with the integration owner.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
