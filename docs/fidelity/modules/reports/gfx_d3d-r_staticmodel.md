# gfx_d3d/r_staticmodel module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 equal.** Mac 1.3 i386 is the authority.

`R_InitStaticModelDynamicData` at Mac 0xfbd7a was hand compared: maximum LOD surface count determines the cache allocation, pointer-sized native storage is used, and each cache entry is zero initialized. No behavioral correction was needed.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
