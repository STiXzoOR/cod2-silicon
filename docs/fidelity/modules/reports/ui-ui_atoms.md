# ui/ui_atoms module reconciliation

Updated 2026-11-05.

**4/4 reconciled: 3 equal, 1 intentional.** Compared all four Mac bodies at 0x1cbcf0 through 0x1cbf0c by hand, including negative-width/height UV reversal, load-bar division and upper clamp, optional white-material drawing, and renderer argument order. The constant and call/string checks were also reviewed.

`UI_DrawHandlePic` retains the existing native trace probe behind `COD2_NATIVE_TRACE_ONLY`. Mac 0x1cbcf0 has no probe. It is disabled by default and can print the first four calls when native tracing is explicitly enabled; the rendering behavior matches. No source change was needed.

The arm64 Release dedicated target built successfully. Since that target excludes these client functions, the actual production `ui_atoms.c` translation unit was separately compiled with the configured client flags, with no warnings. No UI execution or replay was run and no tests were added. Windows comparison was unnecessary: the Mac rendering behavior is not a visible platform-specific difference.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.
