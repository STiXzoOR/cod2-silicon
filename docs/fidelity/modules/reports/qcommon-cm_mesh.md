# qcommon/cm_mesh module reconciliation

Updated 2026-11-05.

**11/11 reconciled: 7 equal, 4 fixed.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

## Point-triangle correction, 2026-11-05

The inlined point trace now accepts an unordered `s` value at its lower tolerance gate. Mac 0x676fb-0x67704
explicitly accepts unordered comparisons before rejecting ordered values below -0.001. Native's
`!(s_val >= -0.001f)` rejected NaN; the native-only path now uses `s_val < -0.001f`.

The complete point-triangle contract was compared in the exact-symbol `CM_TraceThroughAabbTree_r` body
0x6754e-0x67ca1. This includes culling and recursive routing, partition checks and triangle stride, ordered
start/end plane rejection, the 0.125-biased fraction and zero clamp, the existing fraction gate, unbiased
hit interpolation, both barycentric dot products and all tolerance gates, fraction/normal stores, and loop
continuation. Mac constants are the binary floats at 0x3261c8 (0.125), 0x326228 (-0.001), and 0x32622c
(1.001). No further point-triangle difference was found. The two public wrappers at 0x67ca2-0x67d13 and
0x67d14-0x67d85 also retain the matching content gate and ordered fraction-decrease material update.

The point helper changes from equal to fixed; the recursive owner's fixed evidence gains this correction.
The other nine ledger verdicts and their exact evidence are retained from the earlier audit. This follow-up
does not claim a fresh comparison of every capsule or border helper operation.

All four actual Release `cm_mesh.c` objects compiled using the stock and CoD2x client/dedicated flags.
All five existing collision suites passed in both variants with ASan/UBSan. No new tests or cases were added;
the NaN-specific branch is compiled and reviewed, but the existing fixtures do not exercise it. No full CI,
replay, or original-Mac execution was run for this follow-up. The OFF projection is byte-identical to the
base, and all five inactive source-guard configurations are unchanged. This file has no `__LINE__` uses.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cm-mesh-unordered.noindex/`.

## Earlier partition correction, 2026-11-04

**Partitions are retested while bit 15 of the counter is set.** All three Mac sites compare the zero-extended 16-bit check
with the sign-extended global counter (Mac 0x67593, 0x67dcc, 0x68305). When bit 15 is set the two can never match, so a
partition reached twice in one trace is tested twice. The vertex and edge checks still skip repeats, so only the plane
test runs again. The last triangle at an equal fraction sets the normal, which makes the retest visible. Native compared
both as unsigned and always skipped the repeat.

## Earlier verification and limits, 2026-11-04

No new per-fix tests were written. The local CI steps 02 to 13 passed, and both builds compile.

Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the `original` recording, the counts stay at 3,781 `origin` and 1,257 `velocity` differences, first at frame 102. That recording was made by the Linux dedicated server in the oracle container, not by the Mac binary, and its first differences are platform differences rather than defects. A model of frame 102 using x87 80-bit arithmetic reproduces the recording bit for bit on both clients, and a model using the SSE steps in the Mac code reproduces the native trace bit for bit. At frame 116, Linux `Sys_SnapVector` uses `rint` where Mac 0xaed28 truncates. Native follows Mac, and the velocity differs by exactly 1.0 on those axes.

One independent review compared the partition checks, `CM_TestTempCapsulePosition`, `CM_TraceTempCapsule` and `CM_TraceThroughBrush` with the Mac disassembly and found no mismatches. It did not check callers or the `trace_t` field offsets. The corrections themselves are compiled and reviewed but not run on their own.

The earlier audit left the point trace's NaN `s` rejection in place and mentioned other unspecified NaN comparison
differences. The named point-triangle defect is corrected above. This follow-up makes no new claim about the
unspecified comparisons outside that complete point-triangle contract.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
