# gfx_d3d/rb_imagetouch module reconciliation

Updated 2026-11-05.

**2/2 reconciled: 2 equal.** Mac 1.3 i386 is the authority.

Compared semantic-zero ordering, descending card-memory and semantic tie breaks, mapType3 selection, currentFeedbackImage lifetime, and the 8x8 white feedback draws/flush. The native qsort comparator follows the original ordering. No source correction was needed.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.
