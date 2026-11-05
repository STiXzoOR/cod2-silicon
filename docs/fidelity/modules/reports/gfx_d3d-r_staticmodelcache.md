# gfx_d3d/r_staticmodelcache module reconciliation

Updated 2026-11-05.

**10/11 reconciled: 8 equal, 1 fixed, 1 intentional, 1 unchecked.** Mac 1.3 i386 is the authority.

The complete eleven bodies were inspected. At Mac 0xe064e, the DX7 light cursor points to `D3DLIGHT9.Ambient` (+0x24). Native started at the structure base and read Diffuse using negative offsets outside the array. The corrected base and Mac temporary ARGB packing at 0xe04d1–0xe0605 also restore the green/blue lighting accumulators. Rigid transform order, DX9 64-byte and DX7 24-byte vertex construction, lighting coordinates, converter calls and buffer operations were compared.

Cache allocation, age checks, buddy splitting, recursive eviction, statistics and list reset were hand compared. `R_InitStaticModelCache` intentionally omits Mac-private usage bit 0x400000: native 0x208 replaces Mac 0x400208 at 0xe07ec for D3D9 portability. The current GL constructor only stores Usage and GetDesc reads it; allocation/locking behavior does not depend on that bit.

`R_CacheStaticModelSurface` remains unchecked only at the index-storage boundary: Mac 0xe170d copies at least twelve bytes even when `triCount / 2` is zero, then uses equality termination for signed counts. Validity of `triIndices` allocation for zero/negative pair counts is unproven. Its remaining allocation, list/node/statistics, command submission and positive paired-copy behavior were compared. Defined native skipping of nonpositive counts remains.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. Every changed renderer translation unit was also compiled directly with its native compilation-database command, because the dedicated target omits renderer objects. Corrections are compiled and reviewed, not exercised. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
