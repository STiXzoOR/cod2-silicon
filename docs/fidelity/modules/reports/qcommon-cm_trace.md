# qcommon/cm_trace module reconciliation

Updated 2026-11-05.

**38/38 reconciled: 36 equal, 2 fixed.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The initial comparison covered brush clipping, leaf traversal, the tree walk, sphere tests, transformed traces and the
whole sight path. The follow-up below corrects four additional unordered tree-walk branches.

## Corrections

**The temporary capsule position test rounded differently.** Mac 0x64cd6-0x64e21 tests the four end sphere pairs and then
the shared cylinder, using each end height directly. Native used a single height gap, which gives the same result except
at a rounding boundary.

**The temporary capsule side hit used the wrong height.** Mac 0x65461 removes the contact bias from the biased fraction
before checking height, so it uses (fraction - epsilon). Native used the original root, which can differ by rounding.

## Verification and limits

No new per-fix tests were written. The local CI steps 02 to 13 passed, and both builds compile.

Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the `original` recording, the counts stay at 3,781 `origin` and 1,257 `velocity` differences, first at frame 102. That recording was made by the Linux dedicated server in the oracle container, not by the Mac binary, and its first differences are platform differences rather than defects. A model of frame 102 using x87 80-bit arithmetic reproduces the recording bit for bit on both clients, and a model using the SSE steps in the Mac code reproduces the native trace bit for bit. At frame 116, Linux `Sys_SnapVector` uses `rint` where Mac 0xaed28 truncates. Native follows Mac, and the velocity differs by exactly 1.0 on those axes.

One independent review compared the partition checks, `CM_TestTempCapsulePosition`, `CM_TraceTempCapsule` and `CM_TraceThroughBrush` with the Mac disassembly and found no mismatches. It did not check callers or the `trace_t` field offsets. The corrections themselves are compiled and reviewed but not run on their own.

The initial audit retained a point-triangle NaN-`s` difference and unspecified unordered comparison differences. Four
identified `CM_TraceThroughTree` comparisons are now corrected in the follow-up below. The separate point-triangle
`!(s_val >= -0.001f)` gate in `cm_mesh.c` remains outside this change; no claim is made that its NaN behavior was repaired
or that the former map-data assumption establishes complete input coverage.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.

## Independent math-caller follow-up — 2026-11-05

Current accounting is **36 equal, two fixed; none unchecked**. Fresh review
identified four unordered branches in `CM_TraceThroughTree`: the selected
minimum/maximum child tests at 0x63fee–0x64010 and 0x640c1–0x640ee, fraction
pruning at 0x64160–0x6416f, and the parallel gate at 0x64171–0x64182. Native
`COD2_X64` now follows the negated strict comparisons, preserving the selected
operands, finite branch behavior and Mac unordered outcomes. The complete owner
row changes from equal to fixed with its original evidence retained and this
correction appended by parent approval.

Configured stock/CoD2x arm64 Release client and dedicated objects compile. All
five unchanged collision suites pass both modes under ASan/UBSan. Unordered
cases remain compiled and reviewed, not specifically exercised. Five inactive
source configurations and exact `unifdef -UCOD2_X64` output match the base;
this source has no `__LINE__` uses. No new tests, full CI or replays were run.
The retained point-triangle gap above is not part of this correction.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.

The separately identified point-triangle NaN lower-bound gap mentioned above is now corrected by the batch21 `qcommon/cm_mesh` follow-up. Its existing collision fixtures pass; NaN-specific behavior remains compiled and reviewed.
