# gfx_d3d/r_staticmodelcache module reconciliation

Updated 2026-11-05.

**10/11 reconciled: 8 equal, 1 fixed, 1 intentional, 1 unchecked.** Mac 1.3 i386 is the authority.

The complete eleven bodies were inspected. At Mac 0xe064e, the DX7 light cursor points to `D3DLIGHT9.Ambient` (+0x24). Native started at the structure base and read Diffuse using negative offsets outside the array. The corrected base and Mac temporary ARGB packing at 0xe04d1–0xe0605 also restore the green/blue lighting accumulators. Rigid transform order, DX9 64-byte and DX7 24-byte vertex construction, lighting coordinates, converter calls and buffer operations were compared.

Cache allocation, age checks, buddy splitting, recursive eviction, statistics and list reset were hand compared. `R_InitStaticModelCache` intentionally omits Mac-private usage bit 0x400000: native 0x208 replaces Mac 0x400208 at 0xe07ec for D3D9 portability. The current GL constructor only stores Usage and GetDesc reads it; allocation/locking behavior does not depend on that bit.

`R_CacheStaticModelSurface` remains unchecked only at the index-storage boundary: Mac 0xe170d copies at least twelve bytes even when `triCount / 2` is zero, then uses equality termination for signed counts. Validity of `triIndices` allocation for zero/negative pair counts is unproven. Its remaining allocation, list/node/statistics, command submission and positive paired-copy behavior were compared. Defined native skipping of nonpositive counts remains.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
