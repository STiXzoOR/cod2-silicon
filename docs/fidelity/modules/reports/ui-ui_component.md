# ui/ui_component module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 equal.** Compared `UI_Component_Init` with Mac 0x1b54f6 by hand: the `cls.uiStarted` gate, full component-state clearing, integer video-size conversion, 8/16/16 metrics, developer gate, and both material registrations with flags 0x30 and track 1 match. Native clears its full pointer-sized component structure, corresponding to the Mac 0xac-byte structure. No source change was needed.

The full-binary dump misdecodes the prologue. The exact-symbol `otool -arch i386 -tV -p __Z17UI_Component_Initv` dump starts with the correct frame setup and was used for the verdict. The material strings were also read directly from the binary.

The arm64 Release dedicated target built successfully. The actual production client `ui_component.c` translation unit was separately compiled under the configured flags, with no warnings. The UI was not exercised and no tests were added. No visible platform-specific behavior required Windows comparison.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.
