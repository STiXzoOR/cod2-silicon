# qcommon/cm_trace module reconciliation

Updated 2026-11-04.

**38/38 reconciled: 37 equal, 1 fixed.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The brush clipper, leaf traversal, tree walk, sphere tests, transformed traces and the whole sight path match Mac
operation for operation.

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

Two differences are left in place because they need NaN inputs that the map data does not produce: the point trace skips
a NaN `s` value where Mac accepts it, and some comparisons order NaN differently.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.
