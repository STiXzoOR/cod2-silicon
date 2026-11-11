# gfx_d3d/r_model module reconciliation

Updated 2026-11-05.

**33/33 reconciled: 27 equal, 5 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

All thirty-three bodies were hand compared, including complete rigid/weighted skinning commands, preprocessing, bounds and debug geometry.

Mac has independent warning counters at 0xd40b00/04/08/0c/10 for cached vertex allocation, pre-skin surfaces, scene DObjs, static surfaces and XModels. Native incorrectly shared one. The guarded paths now use the separate counters and full cache, temp-buffer, scene-material and scene-surface diagnostics.

Weighted DX7 position transforms at 0xd2324–0xd232b and 0xd23b8–0xd23bf group `(x + y) + (z + translation)`; native used sequential additions. The initial and blend transforms now follow the SSE order. DX9 fourth-component weighting, first-bone frame rotation, rigid paths, surface masks and pointer-sized command strides were checked. The 72 integers in the debug-box corner table match Mac data at 0x32ad80.

`R_LockSkinnedCache` now passes unconditional DISCARD (0x2000) on the Apple native path, matching Mac0xd0030/d0054. The reconciled GL buffer provider now implements flags, so the earlier ignored-flags justification no longer applies. The existing null-buffer guard and non-Apple portable flag selection remain intentional native boundaries. Native temp-skin storage and the model/init/render-command users share `DxGlobals`; command padding remains defined native storage. The row stays intentional for those explicit adaptations; the Apple flag defect is corrected, compiled and reviewed.


## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.

## Integrated batch 15 verification

Engine bcafce5 and fixture-only follow-ups ae7b674, 5642801 and 4e73a89 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
