# gfx_d3d/r_specularityimage module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 fixed.** The selected GL renderer still uses the generated specularity image.

Read exact Mac `Image_BuildSpecularityMap` at 0x109f2c. The 32 columns, 256 levels, float division/addition before widening, double trigonometric/logarithmic operations, widened `0.1f` log input, fixed final exponent 30 and byte rounding/32-byte stride match.

Mac 0x109f5a/0x109f62 takes the multiply branch for unordered normalized values. Native previously sent NaN through the upper clamp, producing an ordinary finite exponent. The native clamp now preserves this unordered branch and the negative zero lower clamp. When the resulting power value is NaN, native explicitly writes zero, matching the low byte of Mac's invalid `cvttss2si` result at 0x10a01a without an undefined C float-to-int conversion. Finite values retain the original arithmetic order.

The arm64 Release dedicated target and actual client object compile. Five OFF preprocessing configurations and actual SDK OFF preprocessing, retaining `__LINE__` expansions, match baseline c1bb3b0. No new tests, full CI or replays ran. Corrections are compiled and reviewed, not exercised; no cross-libm last-bit or whole-frame claim is made. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-assets.noindex/`.
