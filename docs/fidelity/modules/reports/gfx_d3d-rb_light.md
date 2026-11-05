# gfx_d3d/rb_light module reconciliation

Updated 2026-11-05.

**6/6 reconciled: 1 equal, 5 fixed.** Mac 1.3 i386 is the authority.

Compared light derivation and ambient/fallback accumulation, grid interpolation/search and visibility, cache logging/readback and debug drawing. Binary data, not inferred directions, supplies the eight basis vectors at 0x32b2c0; native previously had a zero table.

RB_DeriveEntityLights reserves the last light for sunlight at count>=max (0xead4b) and uses the original threshold constants. RB_GetLightingAtPoint truncates biased coordinates (0xeb603 onward), preserves wrapping packed-key subtraction, uses the search predecessor (0xec095), and matches debug color imports. The existing defined first-point fallback is retained when the predecessor would be before declared storage.

Lower-plane missing cache points now use yellow at 0xeb55b. Visibility-history diagnostics regain the control-byte prefix from 0x250d28. Filename limits, six-byte records, 24MiB capacity, mode2 reads, divisible-size validation and reset/writeback were checked.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
