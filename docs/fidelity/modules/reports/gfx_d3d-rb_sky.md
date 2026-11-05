# gfx_d3d/rb_sky module reconciliation

Updated 2026-11-05.

**4/4 reconciled: 4 equal.** Mac 1.3 i386 is the authority.

Compared query Issue/GetData/Sleep handling and fallback256, six-index topology, generic and DX7 attributes/UVs, sun-direction matrix products, DX7 homogeneous divide, DX9 homogeneous billboard offsets and matrix setup. RB_CalcSunSpriteSamples was restarted at its exact Mac symbol because full-binary disassembly misaligned its prologue.

RB_DrawSun and RB_DrawSunPostEffects are implemented here but belong to rb_backend. Their query/tile accumulation, fallback visibility trace, sprite axes, fading and color packing were compared. Unordered visibility/flare/blind/glare branch behavior now follows 0xdf860, 0xdeb9d, 0xded87 and 0xdee00. The four rb_sky ledger rows themselves match.

The restored paths explicitly emulate the original CVTTSS2SI invalid-conversion result before color/position or segment-count narrowing, keeping native C conversions defined.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.
