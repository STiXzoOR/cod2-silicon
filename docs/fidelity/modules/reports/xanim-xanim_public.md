# xanim/xanim_public module reconciliation

Updated 2026-11-04.

**3/10 reconciled: 3 equal, 7 unchecked.** Every assigned row was evaluated. These historical header helpers now live in native implementation files or are inlined into their callers.

`MatrixTransformVectorQuatTrans` was compared through its complete Mac body at 0x2c0e8–0x2c220 with `universal/com_math.c`. Quaternion scaling, all rotation coefficients, dot-product order, translation additions and sequential writes match. `ConvertQuatToMat` matches the coefficient construction in that same body. The alias-safe in-place transform matches the Mac DObjCalcSkel inline transform at 0x759be–0x75af0 and the native original-translation capture in `DObjCalcParentLocal`.

## Shared corrections awaiting integration

Three rows remain unchecked in this worktree because the `xanim/dobj` worker owns their corrections in commit `fcd775c4acbb2830833d397f74a05ef1657d0728`.

`DObjCalcTransWeight`: Mac 0x75958–0x759ab computes the quaternion length in component order and divides 2 by it. Its zero-length branch at 0x75d15–0x75d2c writes only w=1 and transWeight=2. Native also zeroed xyz, losing signed zeros and finite subnormal components whose squares underflow. The DObj worker independently confirmed the mismatch and preserves xyz in its correction.

`QuatMultiplyEquals` and `QuatMultiplyReverseEquals`: Mac qx/qy accumulation order at 0x75850–0x75913 and 0x75bf0–0x75cbe differed from native `DObjCalcParentLocal`. The DObj worker independently confirmed and corrected both branches. Parent acceptance must verify and integrate that commit before these three rows become fixed in the combined ledger.

## Other unchecked entries

The four Linux inline readers `XModelDataReadFloat`, `XModelDataReadInt`, `XModelDataReadShort` and `XModelDataReadUnsignedShort` are no longer defined by the native public header. Candidate native loader reads and the Mac symbol inventory were inspected, but unique complete Mac inline identities were not established. Similar primitive loads alone are not an equality claim.

## Verification and limits

No code in this module or `universal/com_math.c` changed in this worktree. The animation core's arm64 Release dedicated-server build and existing animation and synchronized notification fixtures pass. The shared DObj worker reports its own arm64 Release build and unchanged legacy preprocessing; its corrections are compiled and reviewed but not exercised. No new tests, full CI or replays were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-core.noindex/`. The unit ledger retains the exact evidence gaps and linked correction ownership.
