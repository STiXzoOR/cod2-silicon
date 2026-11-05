# gfx_d3d/r_outdoor module reconciliation

Updated 2026-11-05.

**2/2 reconciled: 2 equal.** Mac 1.3 i386 is the authority.

Both bodies were hand compared. Outdoor bounds, scale and inverse scale, 512×512×256 binary dimensions, world-to-outdoor matrix, 512² texture generation and trace flag 0x2001 match. The native hit-height interpolation uses `ceiling + (floor - ceiling) * fraction`, matching the Mac SSE order. The selected trace import is a direct nonnull provider.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
