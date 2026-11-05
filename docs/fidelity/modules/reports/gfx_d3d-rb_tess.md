# gfx_d3d/rb_tess module reconciliation

Updated 2026-11-05.

**7/7 reconciled: 5 equal, 1 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

Compared particle-cloud view axes, draw arguments and buffers, entity dispatch, polygon and triangle tessellation, skinned/rigid model paths, cached draw routing, vertex layouts, attributes, UVs and index winding. Several functions implemented here have rb_backend/rb_state ledger ownership.

The dispatch table at 0x32b8d0 maps entity type5 to rail (0xffae2) and type9 to cylinder (0xff56f); native had an empty type5 and rail at type9. Restored both and reconstructed the cylinder LOD/segment clamp, trig recurrence, paired radii, six-index strips and final seam from the original entry blocks. Sprite minimum-screen-radius projection at 0xff478, oriented sprite double radians, atlas columns/rows and rail color byte order also now follow Mac.

The AddQuadStamp/AddLine variants use enum-width overflow save/restore instead of an eight-byte void-pointer access to a four-byte declaration field. Generic quad positions follow Mac's left-plus/minus-up grouping, and generic line overflow follows direction construction. Existing defined native world-radius storage, zero-atlas defaults and null-material handling are retained.

The restored paths explicitly emulate the original CVTTSS2SI invalid-conversion result before color/position or segment-count narrowing, keeping native C conversions defined.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
