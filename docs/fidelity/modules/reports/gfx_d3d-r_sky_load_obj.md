# gfx_d3d/r_sky_load_obj module reconciliation

Updated 2026-11-05.

**2/2 reconciled: 2 equal.** The selected GL renderer still uses both sun loaders.

Read exact Mac `R_LoadSun` at 0xfaf04 and `R_LoadSunThroughDvars` at 0xfae78. The first clears the whole sun record, chooses the basename after either slash, truncates to 63 characters, strips at the first dot and skips an empty name. Native clears the complete widened `sunflare_t`, preserving the original logical fields rather than the old 0x60-byte pointer layout.

The second formats `sun/%s.sun`, prints the same warning only on read failure, passes the sun dvar table and its reported count to `Com_LoadDvarsFromBuffer`, updates the sun only on success and frees every successfully read file. The imported table is an array address, not another pointer slot. No source changed.

Triage and constants checks supplement the hand comparison. Exact-symbol listings resolved the full-binary prologue decoding issue. The arm64 Release dedicated target and actual client object compile. No new tests, full CI or replays ran; file loading and sun dvar updates were not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-gfx-assets.noindex/`.
