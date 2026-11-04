# bgame/bg_jump module reconciliation

Updated 2026-11-04; baseline `3355f0e`.

**14/14 reconciled: 14 equal.** Mac 1.3 is the authority; Linux 1.3 was read only. All constants were read from the
binary. No source changed.

## Verification and limits

No new per-fix tests were written. The local CI steps 02 to 13 passed, and both builds compile. Both 2,000-frame replays match the trace from the previous commit, so that recording reaches neither correction. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, first at frame 102.

To test whether arm64 `sinf` and `cosf` rounding explains frame 102, `AngleVectors` was routed through double
precision for one replay. The trace did not change, so correctly rounded `sinf` and `cosf` do not explain it, and the
change was reverted. This does not show what the i386 library returned. The walking path in `Pmove` also matches Mac
operation for operation, so the likeliest remaining source is the collision code in `qcommon/cm_*`, which has not been
reconciled. This is an inference, not a measurement.

One independent review compared the corrected functions, and spot-checked `Jump_Check` and `Jump_ApplySlowdown`, against
the Mac disassembly and found no mismatches. It did not check `Jump_ReduceFriction`, `Jump_GetStepHeight` or
`Jump_ClampVelocity`.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.
