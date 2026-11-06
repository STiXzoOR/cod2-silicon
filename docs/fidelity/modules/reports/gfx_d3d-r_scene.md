# gfx_d3d/r_scene module reconciliation

Updated 2026-11-05.

**18/18 reconciled: 12 equal, 5 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

All eighteen entries were hand compared. `R_AddStaticModelToScene` indexes 96-byte DX7 lighting records at 0xc6f07–0xc6f09 instead of advancing by bytes, and its entity-limit format no longer contains an escaped percent. `R_AddPolyToScene` truncates the lightmap index before packing the sort key at 0xc5937 and uses the Mac signed polygon-slot remainder.

`R_SetViewParmsForScene` selects the 0.01 default for an unordered near-clip dvar at 0xc5b7b–0xc5b94. `R_RenderScene` now checks both window dimensions (0xc69bc), gates early effects on draw-loop completion and full-window rendering (0xc6592), and applies late effects to full-window views (0xc6699). Fog interpolation compares previous and target technique offsets (0xc6451), uses the Mac duration denominator (0xc66b8), and truncates interpolated colors through an integer. The full debug entity-count dump at 0xc61b4–0xc6430 was restored.

`qsortDrawSurfs` now follows the complete custom unstable partition/selection algorithm at 0xc52c6–0xc5558, including the extra division by eight in its pivot/cutoff calculation and tie behavior. Native pointer-sized records remain. The entity-dump comparator intentionally orders `uintptr_t` keys instead of truncating pointer differences; identity grouping and type precedence match. The optional Windows v60 forwarding bridge is inactive in the native GL build.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
