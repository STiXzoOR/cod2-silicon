# client/cl_console_mp reconciliation

Updated 2026-11-04.

**48/48 reconciled: 38 equal, 7 fixed, 3 intentional.** This includes the 46 module rows and the two complete console callers `CL_AddConsoleInfoColor` (0x15d56c) and `CL_ConsolePrint` (0x15f344), whose original ledger ownership was `universal/com_math`. The parent owns their ledger reassignment. No global ledger was edited.

Every complete Mac body was read against the active native source, including inlined helpers and the rendering/input dependencies. Constants and call/string triage were supplementary checks. Symbol-start `otool` corrected a misleading full-image prologue at `CL_RegisterHudMsgIconMaterial`.

## Corrections

- **Print-type changes sent the pending row to the wrong window.** `CL_ConsolePrint_AddLine` at 0x15ed81 and `CL_DeathMessagePrint` at 0x160610 flush `con.prevType`; native flushed the incoming type.
- **Asian console wrapping was absent.** Restored the complete 0x15ed8c/0x15efe3–0x15f343 path: font selection, glyph-width lookahead, SEH character decoding, two-byte ring storage, and pixel/character wrapping for game, bold and subtitle messages. The ASCII branch now also follows Mac's negative-width decision and integer conversion after `ceilf`.
- **Message drawing omitted several animation branches.** `Con_DrawMessageWindow` (0x15d6f0) now preserves reversed groups with identical start times (0x15db6a), the bottom-up pre-scroll pass (0x15d865), expiry at equality (0x15d970), and Mac's distinct top-down and bottom-up float operation orders.
- **Argument hints copied before checking space and used the wrong origin.** The 1024-byte staging buffer is checked before extension stripping (0x15e8db). The list now anchors beneath the argument's measured field prefix and preserves the resulting `leftX` (0x15e9bb–0x15ebce). Independent review caught the retail scan crossing the field terminator when a quoted argument immediately follows its command. Native tokenizers accept that input; the scan now stops at NUL or the 256-byte field bound and skips that hint if no separator exists. This deliberately preserves defined native storage instead of reproducing the Mac/Windows out-of-bounds read.
- **Input hints omitted a blank row and selected detail too early.** Restored the extra font-height advance at 0x15ff90 and the whitespace requirement for an exact match among several prefix matches (0x15ffb7–0x1601ee).
- **Console dumps removed internal blank rows.** Mac skips leading blanks only (0x15c79b), then writes every subsequent row (0x15c885).
- **The output scrollbar had extra clamping and different arithmetic.** Restored the uncapped fraction and operation order at 0x15d2f9–0x15d39a, including the build-label baseline calculation.
- **Kill-message wrapping and icon encoding differed.** Restored the entry-width comparison with `strlen(victim) + 3` at 0x16078b, the flip argument's low-byte interpretation, and metadata OR encoding without masking high bits. Icon metadata can cross a logical row inside the actual ring; native still refuses a write beyond its 65536-short allocation. Existing null-name handling remains defined.

## Native contracts retained

`common_types.h` selects the active split headers; its large legacy type block is disabled. The arm64 compiler layout confirms `MessageLine` is 164 bytes with times at 156/160, `field_t` is 280 bytes with buffer at 24, `MessageWindow` is 32 bytes, and `qkey_t` is 16 bytes. Named pointer/member access handles widened console/window/input layouts. Generated imports address the typed `clients`, `PlayerKeyState`, console field and history allocations. The documented WS38 `keys[160].down` adaptation remains intentional.

The native font boundary was traced through `UI_RegisterAssets` → `CL_RegisterFont` → `R_RegisterFont` → `R_LoadFont` → `FS_FOpenFileRead`. Existing `$font`/`$consolefont` aliases and the LP64 font-file loader remain. `UI_GetFontHandle`, normalized scale, text metrics and the renderer's SEH decoder support the restored engine wrapping. `refexport_t` and `SCR_DrawConsoleString` retain typed font handles; no backend reconstruction or unsupported glyph-coverage claim is made.

Existing CoD2x prompt/build text, wider name/value columns, 60-match limit and kill-message double-color parsing remain. Stock detailed-dvar hints already include the exact default/latched labels and domain/type/range information. The macOS arm64 build label and defensive invalid-font-height fallback remain.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built successfully under `taskpolicy -b nice -n 19` with two jobs. That target excludes client code, so the complete final console translation unit was separately compiled in stock and CoD2x modes with the engine compilation flags. Both passed; only existing narrowing warnings remain.

Five port-off preprocessing configurations match base `03ebea4b8fb5fcd9be234fed267293c6c60e16fc` exactly. This file has no `__LINE__` expansions. `git diff --check` passes. No new tests, full CI, replays or runtime rendering/input checks were run. Corrections are compiled and reviewed, not exercised. No assigned row remains unchecked. Invalid pointers, overflowing engine counters and out-of-range color conversions are outside the valid engine contracts claimed equal here.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-console.noindex/`. The 48 private verdicts are in the orchestrator's `client-console.ledger.json`.
