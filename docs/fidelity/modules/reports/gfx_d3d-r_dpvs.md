# gfx_d3d/r_dpvs module reconciliation

Updated 2026-11-05.

**17/17 reconciled: 6 equal, 11 fixed.** Mac 1.3 i386 is the authority.

All seventeen entries were hand compared, including the complete world, cell and portal walks.

- At 0xee535, nonpositive cull distances clear while NaN survives. At 0xeefdb–0xef00e, side bytes use signed float bits. Frustum bounds reject unordered distances (0xeed3a/0xef24c); far-corner containment accepts them (0xef39c).
- `R_FilterEntityIntoCells_r` prints the reference-capacity warning at 0xee68b–0xee690 instead of raising a fatal error. The native capacity guard remains.
- `R_ChopPortalWinding` copies an on-plane vertex without emitting an intersection (0xeea02–0xeea34), and now has a defined wraparound distance slot for 128 vertices.
- `R_AddWorldSurfaceWithCull` tests occluders even with zero frustum planes (0xef092–0xef0fa). Leaf/coarse AABB paths submit their static models (0xefab3–0xefae5).
- Dynamic models choose preliminary/current bounds using `cullState`, update pose when required, then test current bounds again (0xefc51/0xf0408). Brush draw gates and cull-group surface submission match the Mac paths.
- Active occluder storage holds 1024 native pointers, matching the Mac slot count at 0xf2424. Orthographic views bypass far-plane setup (0xf2847).
- Portal queue priorities use the winding maximum (0xf117d–0xf142c); the copied near plane uses its minimum (0xf1949–0xf1a55). Both monotonic scans were recovered separately.
- Bevel rectangles, inverse projection, redundant-normal rejection, plane construction and debug edges were restored from 0xf1bc0–0xf1e69. Projected polygon area, conditional projection and clip-child decisions match 0xf1fe1–0xf207c, including the deterministic last-projected-Y bounding-area quirk at 0xf21ee. The debug polygon uses its binary-checked cyan/quarter-alpha color.
- The further-cell list now holds 1024 pointer-sized slots; exact occluder and convex-hull diagnostics were restored.

`R_GetFurtherCellList_r` and side-normal generation match for nonempty bounded convex windings. Existing null-storage guards remain. Counts beyond winding/cell storage, and expanding windings beyond the 128-vertex caller buffers, have no established valid allocator contract; this audit does not claim defined parity for those overflows. Current GL rendering remains selected.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
