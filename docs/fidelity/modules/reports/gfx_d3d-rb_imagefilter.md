# gfx_d3d/rb_imagefilter module reconciliation

Updated 2026-11-05.

**4/4 reconciled: 2 equal, 1 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

Compared tap parity, exponentials, paired centroid/normalization/trimming, residual filter-chain loops, viewport/aspect radius calculations, ping-pong targets and glow pass flags. Raw constants and unordered branch selection were checked.

RB_GenerateGaussianFilterChain uses a strict X maximum comparison: 0x105e56 sends equality to residual square-root calculation, whereas native used >=. The Y comparison remains as in Mac. The existing selected GL exact-extent texture adaptation retains unit UVs and the half-pixel -0.5 adjustment instead of the original power-of-two sampling extent.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.
