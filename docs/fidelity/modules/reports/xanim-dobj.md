# xanim/dobj module reconciliation

Updated 2026-11-05.

**55/58 reconciled: 53 equal, 2 fixed, 3 unchecked.** Mac 1.3 i386 is the authority. All 40 Mac-backed functions were read, including skeleton composition, hierarchy traversal, duplicate parts, surface masks and both trace paths. The remaining rows were checked against their renamed or inlined Mac counterparts where an identity could be established. Linux supplied names only.

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

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

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
