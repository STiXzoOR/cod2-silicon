# gfx_d3d/r_xsurface_optimize module reconciliation

Updated 2026-11-05.

**3/3 reconciled: 3 equal.** Mac 1.3 i386 is the authority.

All three bodies were hand compared. Vertex strides 36/64, 32-index padding, creation/retry flags, release/unlock paths and LOD registration arguments match. Native import slots 0x168/0x174 correspond to the Mac 0x16c/0x178 entries. The retry/failure state is backed by the selected native data flag; no behavior change was needed.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
