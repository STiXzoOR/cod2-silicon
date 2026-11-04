# qcommon/cm_world module reconciliation

Updated 2026-11-04.

**24/24 reconciled: 24 equal.** Read the Mac 1.3 bodies at 0xaf49c-0xb13ca alongside native source, including the recursive helpers that have Mac symbols but Linux-only ledger rows. Call/string triage and literal checks were used as prompts for that comparison. No code changes were needed.

The comparison covers unlinking and content propagation, area bounds and capacity exits, all point/sight/move traversals, insertion, sector allocation, sorting and initial world linkage. The Linux names `CM_AddEntityToNode` and `CM_AddStaticModelToNode` map to native insertion helpers and Mac inline blocks. `CM_ClearWorld`, `CM_LinkStaticModel` and `CM_LinkAllStaticModels` are inlined into `CM_LinkWorld` at 0xb0cc2.

Examples checked include the existing one-based entity/model lists, propagation of the full sight hit result, reciprocal-then-multiply clipping fractions, midpoint operation order, fraction pruning only on split paths, and the 512-unit sector threshold. `CM_SortNode` at 0xb0708 uses the supplied bounds when allocating either child; the existing native `COD2_X64` path preserves that behaviour. The 1024-sector free list and root midpoint initialization match the Mac world setup.

## Verification and limits

The apparent extra native unlink calls are Mac-inline unlink bodies; the apparent missing native memset is compiler expansion. Attribute-decorated helpers missed by the source-view extractor were read directly from source. There were no shared-header changes or unresolved evidence gaps.

The arm64 Release `cod2_macos_ded` target compiled and linked with `COD2_X64=ON`, using `taskpolicy -b nice -n 19` and two jobs. No new tests, full CI or replays were run. Runtime paths were not exercised. Private disassembly, constant checks and the build log are in `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-world.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
