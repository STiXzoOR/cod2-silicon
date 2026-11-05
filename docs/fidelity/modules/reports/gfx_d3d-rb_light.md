# gfx_d3d/rb_light module reconciliation

Updated 2026-11-05.

**6/6 reconciled: 1 equal, 5 fixed.** Mac 1.3 i386 is the authority.

Compared light derivation and ambient/fallback accumulation, grid interpolation/search and visibility, cache logging/readback and debug drawing. Binary data, not inferred directions, supplies the eight basis vectors at 0x32b2c0; native previously had a zero table.

RB_DeriveEntityLights reserves the last light for sunlight at count>=max (0xead4b) and uses the original threshold constants. RB_GetLightingAtPoint truncates biased coordinates (0xeb603 onward), preserves wrapping packed-key subtraction, uses the search predecessor (0xec095), and matches debug color imports. The existing defined first-point fallback is retained when the predecessor would be before declared storage.

Lower-plane missing cache points now use yellow at 0xeb55b. Visibility-history diagnostics regain the control-byte prefix from 0x250d28. Filename limits, six-byte records, 24MiB capacity, mode2 reads, divisible-size validation and reset/writeback were checked.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.
