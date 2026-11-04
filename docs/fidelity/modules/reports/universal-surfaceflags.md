# universal/surfaceflags module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 2 equal.** No code correction was needed.

Compared complete `Com_SurfaceTypeFromName` at Mac 0xbcad0: case-insensitive default handling, the 22-name search, surface-bit extraction and -1 miss. Compared `Com_SurfaceTypeToName` at 0xbcb46: index-minus-one range check, indexed return and default fallback.

A private script compared all 54 native `infoParms` names and all four integer fields with the Mac table at 0x348820. All rows match, including both zero terminators. This confirms the existing typed native table rather than reintroducing address-shaped legacy initializers.

The arm64 Release dedicated-server build passed at low priority with two jobs. Functions were hand-compared and data was checked directly; no runtime surface-name test, full CI or replay was run by this worker. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-text.noindex/`.
