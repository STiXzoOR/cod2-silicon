# xanim/dobj module reconciliation

Updated 2026-11-05.

**55/58 reconciled: 51 equal, 4 fixed, 3 unchecked.** Mac 1.3 i386 is the authority. All 40 Mac-backed functions were read, including skeleton composition, hierarchy traversal, duplicate parts, surface masks and both trace paths. The remaining rows were checked against their renamed or inlined Mac counterparts where an identity could be established. Linux supplied names only.

## Corrections

**Parent quaternion additions had a different order.** `DObjCalcParentLocal` accumulated the x and y components in a different sequence in both the ordinary and control branches. Native now follows the SSE additions/subtractions at Mac 0x75850–0x75913 and 0x75bf0–0x75cbe, also repeated in the child-bone paths at 0x75e6f–0x75f46 and 0x764a2–0x76579. The z expression was written in the same operand order for clarity.

**Zero squared quaternion length cleared extra components.** The Mac fallback at 0x75d15–0x75d2c, 0x765b0–0x765ce and 0x765e4–0x765f2 sets only the scalar component to one and the translation weight to two. Native also cleared the vector components. It now retains their signed zeros and tiny values whose squares underflow. Both corrections are confined to `COD2_X64` behavior.

## Comparison details

The comparison included the tree generation rollover and widened native tree header, the 127-bone limit, duplicate-pair encoding and string lengths, hierarchy mask propagation, signed LOD filtering and unsigned surface-mask carries. `DObjTraceline` was compared for valid loaded models and finite coordinates, including priority inheritance, sphere rejection, six-plane clipping, start-solid normals and the scratch sign retained across later rejected candidates. Existing native guards for absent model data and invalid parent indices were preserved; no indeterminate storage was introduced.

Examples of Linux names resolved against Mac code are `DObjGetModelSurface` to `DObjGetSurface` at 0x740ba, `DObjGetSurfaceList` to `DObjGetSurfaces` at 0x743f2, `DObjSetTree` within `DObjCreate`, and `InvMatrixTransformVectorQuatTrans` within `DObjTraceline` at 0x76bcf–0x76f31. The full-binary disassembly misaligned `DObjInit`; restarting at its exact symbol confirmed the five zero words and string registration at 0x73db0.

`DObjClone`, `DObjGetCreateParms` and `DObjSkelIsValid` remain unchecked. They have neither a native definition nor an established Mac text symbol/caller identity. In particular, the Linux `DObjSkelIsValid` does not clear the skeleton on a timestamp mismatch, so the mutating Mac `DObjSkelExists` is not an equivalent identity.

## Verification and limits

Configured arm64 Release with `COD2_X64=ON` and built `cod2_macos_ded` successfully using `taskpolicy -b nice -n 19` and two jobs. Clang preprocessing with `COD2_X64` omitted produced byte-identical output to the starting source, confirming unchanged legacy behavior.

No tests were added or changed. Existing skeleton fixtures were inspected; their simple quaternion values do not distinguish these corrections. The changes are compiled and reviewed but not exercised. CI and replays are left to the orchestrator.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-dobj.noindex/`.

## Integrated batch verification

Integrated source `cf0fc8c` with cross-module evidence commit `e785f19` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `f763fd9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.

## Independent math-caller follow-up — 2026-11-05

Current accounting is **53 equal, two fixed, three unchecked**. The start-solid
horizontal normal path in `DObjTraceline` calls `Vec2Normalize` at Mac
0x7708f–0x770af. That callee takes its normalization path for unordered lengths
and skips only ordered zero (0x2c8e9–0x2c911). Native had tested `lenSq > 0`,
leaving an unaffected component finite when the other was NaN. The guarded
path now computes the float square root, tests `len != 0`, and preserves the
reciprocal and sequential multiply/store order. Existing model guards and
scratch-sign behavior are unchanged. The owner changes from equal to fixed;
its finite-input historical evidence is retained with this correction appended.

Configured stock/CoD2x Release client/dedicated objects compile. The unchanged
xanim trace fixture passes both modes under ASan/UBSan; nonfinite normals were
not specifically exercised. Five inactive configurations and exact OFF source
text match, and there are no `__LINE__` uses. No tests, full CI or replays were
added or run.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.


## Remaining-helper follow-up — 2026-11-05

All three unchecked rows remain unchecked. Mac STABS establishes the 100-byte ILP32 DObj, including skeleton pointer, timestamp, duplicate-parts string, model pointers, parent indices and matrix offsets; it does not establish clone reference ownership or reverse create-parameter extraction. Exact `DObjCreate` remains a construction path, not a proven complete inverse getter or clone.

Exact `DObjSkelExists` at 0x73f2e–0x73f51 confirms the mismatch write clearing the skeleton pointer. Its return predicate overlaps `DObjSkelIsValid`, but the mutation means the whole operation is not equivalent to the nonmutating helper. No missing helper was declared resolved from a shared read. All existing evidence, including the unordered-normal correction, is preserved exactly.

Configured stock/CoD2x Release objects and existing xanim skeleton/mapping/trace fixtures pass both modes under ASan/UBSan. No production source or test assertion changed here; no new cases, full CI or replays. Private follow-up evidence is in `recon-animation-remaining.noindex`.


## Original-address follow-up (inline-effects4)

Original Linux 1.3 executable ranges0x80ba706–0x80ba776 (DObjClone) and0x80ba7f2–0x80ba957 (DObjGetCreateParms; upper bounds exclusive) establish stronger identity anchors than the reconstruction alone. Clone copies100 bytes, clears the skeleton, conditionally retains the duplicate-parts string, then binds the supplied tree. GetCreateParms also reconstructs parent bone names and per-model collision flags; a simple model accessor cannot establish its complete operation. Neither anchor supplies Mac behavior authority. No complete Mac clone/extraction operation was identified, so both rows remain unchecked. Direct-call absence is not a completion argument. No source changes or runtime verification.

## Original nonmutating skeleton predicate

The anonymous Linux 1.0 body at 0x80b83dc maps to original 1.3 0x80ba9b4–0x80ba9e2. It returns true only for a matching timestamp and non-null skeleton and makes no external writes. Fresh exact Mac `DObjSkelExists` at 0x73f2e–0x73f52 instead clears the skeleton on mismatch at 0x73f3c; it remains an invalid substitute for `DObjSkelIsValid`. The two earlier clone/create-parameter identities retain their ownership and inverse-operation gaps. All three remain unchecked. Targeted PPC source metadata added no complete owner for these operations. No source changes, compilation, tests, CI, replay or runtime exercise ran; evidence is in `recon-provenance-animation10.noindex`.

## Unordered sphere projection selector (provenance-distance18)

DObjTraceline's closest-point selection at Mac 0x76b58 compares one with the
projection parameter and branches only for ordered values below one. Unordered
values select all three end-minus-center residuals. Native's ordered >= test
selected the start residual instead. The COD2_X64 predicate now uses
`!(closestT < 1.0f)`; the previous predicate is retained in the legacy branch.

The actual endpoint delta, squared-length reciprocal and bone-center producer
were traced through every start/end/interior distance branch and the radius
consumer. A hand-derived finite entry-domain case, start=(2^65,0,0), end=center=0
and radiusSquared=1, overflows squared length and produces an unordered parameter:
Mac obtains end distance zero while the former native branch obtained infinite
start distance. This is not an upstream reachability or observed gameplay claim.

The existing valid-model guards, scratch-sign handling and earlier unordered
normal correction are preserved. Independent review confirmed the exact Mac
comparison, minimal patch, unchanged complete OFF bytes and zero affected
__LINE__/#line/assert locations. Only dobj.c was manually compiled for the existing
stock/CoD2x client/dedicated configurations, all successfully. The new path remains
unexercised; no tests, CI, full build or replay were run. Evidence and receipts:
recon-provenance-distance18.noindex and provenance-distance18.compile.json.


## Defined high-bit masks in tag setters (serverbits38)

Complete fresh DObjSetLocalTag0x74e5e and DObjSetControlTagAngles0x74f0c bodies use32-bit SHL at0x74ecc/0x74f7a, followed by requested/skeleton tests and animation/control ORs. Native C99 used signed `1 << (boneIndex & 31)`, which lacks a defined result at shift31. The domain is not an invalid index: actual DObjCreate accepts up to127 bones, and XModelGetBoneIndex can return31,63 or95 from valid hierarchy names. Those bit positions require mask0x80000000.

COD2_X64 now computes uint32_t masks and copies their representations into the existing signed locals before the unchanged tests/writes. The exact legacy expressions remain under #else. No field/layout, argument, failure policy, later angle calculation or ownership change was made. Both primary rows change equal→fixed for one shared C-level defect; no observed generated-code or runtime failure is claimed. The same repaired bit phases support the two complete actual server-wrapper mappings documented in server/sv_game_mp.

All four actual stock/CoD2x client/dedicated dobj.c commands compiled successfully at low priority. Exact OFF source and five actual inactive preprocessing outputs, including unsuppressed line/assert expansions, are byte-identical; source has no __LINE__/#line uses. Independent review and receipts: units/review-provenance-serverbits38.md and units/provenance-serverbits38.compile.json; private recon-provenance-serverbits38.noindex. No tests, CI, replay, full build or runtime execution. These paths remain unexercised.


## Complete clone and creation-parameter follow-up (dobj40)

DObjClone and DObjGetCreateParms remain unchecked after full original and actual player/corpse lifecycle comparison. Original clone0x80ba706–RET0x80ba775 copies100bytes, clears only the copied skeleton, retains a nonempty duplicate-parts reference and binds the supplied tree, including cache-age advance/wrap handling. The copied timestamp, resolved parents and bounds otherwise survive. Mac PlayerCmd_ClonePlayer0x1c3574 and CG_ResetEntity0x1deb10 copy clientInfo and clone a persistent animation tree. Full DObjCreate0x752f0 instead clears timestamp/duplicateParts, resolves parents and recomputes bounds. Those real inputs and ownership effects do not establish a complete source-DObj clone; no new replacement policy is inferred.

Original parameter extraction0x80ba7f2–RET0x80ba956 writes model count, tree pointer and tree entity number plus every model pointer, collision bit and resolved borrowed parent-bone name. Actual BG_UpdatePlayerDObj0x182312 builds the creation tuple from clientInfo model/tag strings, its tree, entityState number and a separately supplied collision mask. Its old DObj is only an existence/free gate. This is a complete but different source of creation parameters; no bridge establishes inverse extraction of the old object's resolved parent names and other state.

These are positive complete-operation gaps, replacing the earlier thin symbol/caller notes. No resolved verdict, source or approved ownership adaptation changed. Fresh complete Mac lifecycle/provider bodies and original/provider evidence remain private in recon-provenance-dobj40.noindex. Two unchecked refinements, zero closures/defects. No compilation, tests, CI, full build, replay or runtime verification.
