# F1 emitter integration

Baseline `83aacfb`. Native Emitter_UpdateEmitFx and Emitter_Update restore child orientation,
attachment, emission spacing, and original timing/state updates. The caller
now calculates axes before emission, damps stationary angular velocity, and
preserves the original model gate and attachment movement calculation.
The existing production FX fixture observes 57 failed checks before correction
and passes with stock and CoD2x engine flags under ASan/UBSan afterward. The caller follow-up adds 35 numeric failures and reproduces the
normalized-axis assertion through actual creation, with no artificial render
axis assignment. Stock and CoD2x ASan/UBSan fixtures pass after correction.
The module report describes the numeric scenarios and Mac instructions.

Stock and CoD2x client/dedicated builds pass. The engine source remains
byte-identical with COD2_X64 undefined; whitespace checks pass. Live checks
and review are recorded below. Broader EffectsCore reconciliation continues.
Private evidence is under F1-fx-emitter.noindex and F1-shared-wave7.noindex.

Before the caller follow-up, stock and CoD2x Toujane clients each completed one combat cycle (34.1 and
34.0 seconds), exercising primary/pistol firing, reload, movement, frag and
smoke, and exited 0. Both used private homes and the GUI lease, which was
released after each run. Screenshots show the world, weapon and active smoke.
Both private qconsole_mp.log files have no script errors; the existing missing
`sound/misc/beep.wav` message remains. These are native visual smoke checks,
not original-renderer capture comparisons. No additional server replay was
needed for this client-effects-only batch.

After the caller correction, both integrated sanitizer fixtures passed. Stock
and CoD2x client/dedicated builds passed, and both clients completed another
34.3-second combat cycle and exited 0. Screenshots were inspected for world,
weapon and active smoke; logs had zero script runtime errors and only the
previously noted missing beep. The final channel-helper reuse preserves the
same interpolation/evaluation order; targeted fixtures and both full builds passed again
for that final source, without another duplicate combat cycle.

Actual ce-code-review completed with one P2 finding (#1), defining the
out-of-range correction conversion and 32-bit arithmetic. Root reproduced
the sanitizer failure and applied the reference conversion/wrapping behavior
inline. Review receipt: `/tmp/compound-engineering-501/ce-code-review/20261024-wave7-7713qqdu`.
The independent density/rate candidate was rejected after checking Mac
creation as well as both update bodies; the module report records why.

Finding #1 is resolved: the new bounded regression failed first with UBSan
float-cast-overflow, then passed in stock and CoD2x under ASan/UBSan after
the explicit conversion and wrapping arithmetic. Both full client/dedicated
builds passed again. Final OFF comparison against 83aacfb and whitespace
checks pass. No justified review finding remains unresolved; original
zero-progress behavior is documented rather than silently redesigned.
