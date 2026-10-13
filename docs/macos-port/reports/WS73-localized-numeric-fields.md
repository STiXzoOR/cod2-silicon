# WS73 — Localized numeric edit fields

2026-10-07, base `0f07adc`, branch `codex/localized-numeric-fields`.

Native numeric edit fields (item type 17) now find the bound dvar and format its
current float through the existing `Com_LocalizedFloatToString` with two decimal
places. A missing or non-float dvar leaves this numeric field empty, as in the
reference. Other edit-field types retain ordinary variant strings. Stock and
CoD2x share the restored branch; inactive legacy text is unchanged.

The owned Mac 1.3 `Item_TextField_Paint` tests item type 0x11 at `0x169991`, calls
`Dvar_FindVar` at `0x169c8d`, requires dvar type 1 at `0x169c9a`, and calls the
localized formatter with capacity 1024 and precision 2 at `0x169cc4`. Those
instructions were independently read from private disassembly. The existing
formatter supplies comma separators for languages 1–4, 6 and 7. This unit does
not alter locale policy or the original formatter.

The fixture runs the complete production field painter and existing formatter
with synthetic font/draw/dvar dependencies. It checks all fifteen languages,
missing/non-float dvars, ordinary text fields, cursor rendering and paint offsets.
The starting source produced 19 failed cases; corrected stock and CoD2x pass
with assertions and ASan. The actual UI translation unit and both common targets
compile with Release flags in each mode. Ten legacy configurations, five generic
native-off configurations and the changed UI's actual CoD2x SDK preprocessing
command with native disabled compare identically. `git diff --check` passes.

Run `python3 tests/fixes13/run.py --build <build> --test numeric_fields`.
Private outputs are under `~/Library/Application Support/CoD2-native-protocol/validation.noindex/numeric-*`
and `ws73-numeric/`. No game, global locale changes or licensed payloads entered
this unit. The fixture establishes numeric text and draw dispatch, not localized
keyboard editing/parsing, focus/blink/color equivalence or live rendered glyphs.
Those broader UI behaviors retain their separate acceptance requirements.
