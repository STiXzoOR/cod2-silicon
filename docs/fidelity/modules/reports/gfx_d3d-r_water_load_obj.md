# gfx_d3d/r_water_load_obj module reconciliation

Updated 2026-11-05.

**2/2 reconciled: 1 equal, 1 fixed.** Both functions still serve the selected GL renderer.

Read exact Mac `R_WatersEquivalent` at 0x109afc. Dimensions and domain sizes compare exactly; amplitude and normalized wind direction use float 0.001 thresholds, gravity and speed differences widen before the double 0.1 threshold, and the float square root widens for the double 1e-10 denominator offset before narrowing. Comparison order and unordered tolerance behavior match.

Read exact Mac `R_CreateWaterSetup` at 0x109c00. Native copies the widened record, allocates 8-byte complex and 4-byte frequency elements, preserves wind-speed squaring/division and float/double spectrum operation order, consumes two Gaussian values per element and passes unsigned-short map dimensions with the `watersetup%i` name. The repeated amplitude factor is present in Mac and remains intact.

Mac 0x109e81/0x109e8c takes the spectrum branch when the wind dot product is unordered. Native previously zeroed those coefficients. The native predicate now includes unordered values while keeping zero and negative ordered values in the clearing branch.

The arm64 Release dedicated target and actual client object compile. Five OFF preprocessing configurations and actual SDK OFF preprocessing, retaining `__LINE__`, match baseline c1bb3b0. No new tests, full CI or replays ran. The correction is compiled and reviewed, not exercised; no generated map or GL frame was observed. No shared allocation or image provider changed. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-assets.noindex/`.
