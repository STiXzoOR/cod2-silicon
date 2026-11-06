# xanim/xanim_public module reconciliation

Updated 2026-11-04.

**10/10 reconciled: 7 equal, 3 fixed.** Every assigned row was evaluated. These historical header helpers now live in native implementation files or are inlined into their callers.

`MatrixTransformVectorQuatTrans` was compared through its complete Mac body at 0x2c0e8–0x2c220 with `universal/com_math.c`. Quaternion scaling, all rotation coefficients, dot-product order, translation additions and sequential writes match. `ConvertQuatToMat` matches the coefficient construction in that same body. The alias-safe in-place transform matches the Mac DObjCalcSkel inline transform at 0x759be–0x75af0 and the native original-translation capture in `DObjCalcParentLocal`.

## Shared corrections integrated

Three rows are fixed by the now-integrated `xanim/dobj` correction `fcd775c4acbb2830833d397f74a05ef1657d0728`. Parent spot checks and independent review verified the shared helper bodies before resolving these entries.

`DObjCalcTransWeight`: Mac 0x75958–0x759ab computes the quaternion length in component order and divides 2 by it. Its zero-length branch at 0x75d15–0x75d2c writes only w=1 and transWeight=2. Native also zeroed xyz, losing signed zeros and finite subnormal components whose squares underflow. The DObj worker independently confirmed the mismatch and preserves xyz in its correction.

`QuatMultiplyEquals` and `QuatMultiplyReverseEquals`: Mac qx/qy accumulation order at 0x75850–0x75913 and 0x75bf0–0x75cbe differed from native `DObjCalcParentLocal`. The DObj worker independently confirmed and corrected both branches. Both branches were independently reviewed and the correction is integrated in the combined source.

## Inline reader identities

The parent completed the four reader comparisons after the model-loader audit identified their callers. The complete operations are fixed-width value reads followed by cursor advancement, with the proper signedness or raw float payload. Native model-loading memcpy/cursor expressions match Mac signed-short surface counts at0xbb947–0xbb951 and the six-float/three-integer collision tail at0xbbbde–0xbbc7b. Native unsigned-short animation reads match Mac0xba790–0xba79d and0xba8fc–0xba909, including the two-byte cursor update. Linux helper bodies were used only to establish the complete operation being identified; missing symbol names were not treated as equality evidence. These four rows are equal for valid serialized input on supported little-endian targets. Malformed-file validation remains the callers' responsibility.

## Verification and limits

No code in this module or `universal/com_math.c` changed in this worktree. The animation core's arm64 Release dedicated-server build and existing animation and synchronized notification fixtures pass. The integrated shared DObj correction passed its arm64 Release build, legacy preprocessing and independent review; its numerical edge cases remain compiled and reviewed, not individually exercised. No new tests, full CI or replays were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-core.noindex/`. The unit ledger retains the exact evidence gaps and linked correction ownership.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
