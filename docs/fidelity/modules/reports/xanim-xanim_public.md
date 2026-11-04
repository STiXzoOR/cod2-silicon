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
