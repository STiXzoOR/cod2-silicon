# cgame/cg_info_mp module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 1 equal, 1 fixed.** Both complete Mac bodies were read, with `CG_LoadingString` restarted at its exact symbol because the full-binary disassembly misaligned its prologue. The legacy source is unchanged.

The waiting-for-server message used y=452 in native. Mac 0x1e4597 passes float bits 0x43db8000, or y=439. Windows 0x4d3645 passes the same value, so this is not a Mac-specific placement. Native now uses 439 and the Mac unlimited-width argument 0 at 0x1e4547. Centering retains the Mac subtract-then-half operation order.

The menu response gate, repeated script-menu close, sound-stop boundary, loading flags, map-name fallback, signed-byte lowercase loop, hunk-usage early return and four-phase dots were compared. `CG_LoadingString` agrees on its 1024-byte copy, optional diagnostic and screen update. Existing native sound and UI backend owners remain in use.

All four assigned client translation units compile with actual arm64 Release commands in stock and CoD2x configurations. The dedicated Release target builds. Ten legacy configurations report zero source mismatches; `git diff --check` passes. No new tests, replays or full CI were run. The correction is compiled and reviewed, not exercised at runtime.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-interface.noindex/`.
