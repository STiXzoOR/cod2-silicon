# gfx_d3d/rb_shade module reconciliation

Updated 2026-11-05.

**10/10 reconciled: 6 equal, 3 fixed, 1 intentional.** Mac 1.3 i386 is the authority.

Compared technique pass selection, fixed-function and shader arguments, lighting/constants, decoded code-texture and matrix dispatch, dynamic pool rollover, vertex/index uploads, draw arguments and cleanup. RB_SetVertexData is implemented here but belongs to the rb_state ledger.

RB_SetIndexData restores offset-dependent lock flags at 0xf6647. RB_GetCodeMatrix now multiplies view by projection for source0xcc (0xf7cfe), and applies outdoor down-bias to component2 (0xf7bf3). The matrix dispatch retains the original normalized-world-view-projection source quirk.

RB_DrawSingleTechnique restores the technique33 debug gate and declType3 special flag (0xf7e94/0xf7ea2), fog-source polarity (0xf8633), per-pass projection-to-infinity before/after draw (0xf8422), double radians conversion (0xf9025), and mapType3 color/normal/specular image overrides (0xf92ea onward). The complete state-map failure message is restored. Existing native water upload, GL prebind and null declaration/constant/material/storage handling remain defined native adaptations; these do not retire the active backend.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.
