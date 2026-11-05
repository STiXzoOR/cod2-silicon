# gfx_d3d/r_xsurface module reconciliation

Updated 2026-11-05.

**7/8 reconciled: 7 equal, 1 unchecked.** Mac 1.3 i386 is the authority.

Seven entries match after hand comparison of accessors, signed offsets, vertex/triangle counts and vertex conversion. `XSurfaceGetVerts` at Mac 0xf9bca was compared through the rigid and weighted positive-count paths, including normal/UV branches, 1/256 and 1/65536 weights and SSE accumulation order.

That entry remains unchecked at the negative-count storage boundary: Mac tests `vertCount != 0` then uses equality-terminated loops; native skips negative counts. No loader/allocator contract establishes valid output storage for negative counts. Defined native behavior remains; no out-of-bounds writes were introduced.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
