# qcommon/cm_test module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 9 equal.** Mac 1.3 is the authority; Linux 1.3 was read only. No source changed.

## Verification and limits

No new per-fix tests were written. The local CI steps 02 to 13 passed, and both builds compile.

Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the `original` recording, the counts stay at 3,781 `origin` and 1,257 `velocity` differences, first at frame 102. That recording was made by the Linux dedicated server in the oracle container, not by the Mac binary, and its first differences are platform differences rather than defects. A model of frame 102 using x87 80-bit arithmetic reproduces the recording bit for bit on both clients, and a model using the SSE steps in the Mac code reproduces the native trace bit for bit. At frame 116, Linux `Sys_SnapVector` uses `rint` where Mac 0xaed28 truncates. Native follows Mac, and the velocity differs by exactly 1.0 on those axes.

One independent review compared the partition checks, `CM_TestTempCapsulePosition`, `CM_TraceTempCapsule` and `CM_TraceThroughBrush` with the Mac disassembly and found no mismatches. It did not check callers or the `trace_t` field offsets. The corrections themselves are compiled and reviewed but not run on their own.

Two differences are left in place because they need NaN inputs that the map data does not produce: the point trace skips
a NaN `s` value where Mac accepts it, and some comparisons order NaN differently.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.
