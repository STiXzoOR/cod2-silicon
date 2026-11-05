# gfx_d3d/r_xsurface module reconciliation

Updated 2026-11-05.

**7/8 reconciled: 7 equal, 1 unchecked.** Mac 1.3 i386 is the authority.

Seven entries match after hand comparison of accessors, signed offsets, vertex/triangle counts and vertex conversion. `XSurfaceGetVerts` at Mac 0xf9bca was compared through the rigid and weighted positive-count paths, including normal/UV branches, 1/256 and 1/65536 weights and SSE accumulation order.

That entry remains unchecked at the negative-count storage boundary: Mac tests `vertCount != 0` then uses equality-terminated loops; native skips negative counts. No loader/allocator contract establishes valid output storage for negative counts. Defined native behavior remains; no out-of-bounds writes were introduced.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
