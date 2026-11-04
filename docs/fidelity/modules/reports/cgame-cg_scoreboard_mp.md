# cgame/cg_scoreboard_mp module reconciliation

Updated 2026-11-04.

**10/10 reconciled: 6 equal, 4 fixed.** Every assigned Mac body was read, including all objective-wrapping, team-list and scrollbar blocks in `CG_DrawScoreboard_ScoresList`. `CG_ScoreboardDisplayed` was restarted at its exact entry after full-binary disassembly lost alignment. The legacy source is unchanged.

The six column records at Mac 0x36e6e0 are voice/status/name/score/deaths/ping, with width fractions .05/.05/.47/.13/.13/.17. Native put the name first and used different fractions. All record types, ordering, float bits, headings and alignments now match. The row shrink loop at 0x1c5436 continues until text fits; native stopped at scale .02. The team-banner ping at 0x1c5a0a belongs in column type 4 (ping), rather than the deaths column.

`CG_DrawScoreboard_ScoresList` now uses the exact Mac header baseline constant (14*.84f, bits 0x413c28f5 at 0x1c5d76), repeated row-height additions at 0x1c5c67, and scrollbar branches at 0x1c6391/0x1c6424. An expired fade now returns 0 after clearing the killer name: Mac 0x1c76cb returns the null `CG_FadeColor` result, while native incorrectly returned 1. The fallback map suffix comparison now starts at length-3 as Mac 0x1c763c does, before removing the four-character extension. The previous length-4 comparison usually left `.bsp` visible.

Defined native adaptations remain: typed client-info access and invalid-index checks, a registered-banner-dvar fallback before initialization, six correctly sized border records, and a short-name guard before suffix access. Mac 0x1c69ed derives a text count by subtracting a stack-buffer address from a null whitespace marker when an objective token exceeds its line width. Native retains its bounded token fallback because that count depends on the original ILP32 storage address; no invalid pointer arithmetic was introduced. The native ignored float return convention of `ScoresList` remains.

The team colors preserve incoming alpha, materials and border rectangles agree, and both scroll commands match their dvar-driven clamps. All four assigned client translation units compile with actual arm64 Release commands in stock and CoD2x configurations. The dedicated Release target builds. Ten legacy configurations report zero source mismatches; `git diff --check` passes. No new tests, replays or full CI were run. The corrections are compiled and reviewed, not exercised at runtime.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-interface.noindex/`.
