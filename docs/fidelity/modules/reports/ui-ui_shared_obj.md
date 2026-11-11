# ui/ui_shared_obj module reconciliation

Updated 2026-11-05.

**112/112 reconciled: 99 equal, 8 fixed, 5 intentional.** Mac 1.3 i386 is the primary authority. Every assigned Mac body was read; no Linux implementation was used as authority. Native pointer-width and typed-window adaptations remain in place. The inactive legacy source is unchanged.

## Corrections

`Menu_PostParse` now preserves the existing rectangle alignment when replacing fullscreen coordinates with 640 by 480. Mac 0x1a8c9e copies both alignment fields; native previously passed uninitialized stack values.

`ItemParse_dvar` now validates and allocates the item's type data before reading its dvar token, as Mac 0x1a9b4d does. The dvar callback sets edit limits to -1 after the name is read. `ItemParse_type` no longer repeats that initialization: Mac 0x1abea0 validates type data and returns. Repeating a type declaration after `dvarFloat` previously erased its limits.

`ItemParse_columns` now commits a column's position, width and maximum characters together. Mac 0x1ac096 through 0x1ac13b parses all three values into locals first. Native previously changed earlier fields when a later token failed.

The EOF diagnostics in `Menu_New` and `Item_Parse` regain their trailing newlines (Mac 0x1a9240 and 0x1a947e). `Menu_New` regains the localized out-of-memory error at 0x1a92a1. `UI_LoadMenus` and `UI_LoadMenu` regain the full default-menu failure text and control-byte prefix at 0x1ad8f4 and 0x1ad9af; the oversized-menu diagnostic regains its prefix at 0x1ad928.

## Intentional boundaries

`ItemParse_visible` follows Windows 0x544b10. Its item keyword entry at PE VA 0x5dd458 points to that callback, which parses the integer and sets visibility bit 4. Mac 0x1ac355 additionally hides controls named by `@MENU_SHADOWS` or `ui_sc_enable`; neither suppression is present in Windows. Native now keeps those controls available. Native viewport flag propagation remains in `Window_SetDynamicFlags`.

`RegisterFontHack` is implemented by the native `UI_RegisterLocaleFont` helper. The nine-entry Mac table at 0x33bec0 was decoded again: languages 6–12 and 14 select the documented locale directories; 13 and other values use the supplied path. The safe literal `%s` formatting of supplied paths remains as documented in `WS70-locale-font-selection.md`. `UI_ParseMenuInternal` retains that adaptation and typed native asset storage; its other asset dispatch, field writes, failure paths and handle cleanup match Mac 0x1acac4.

`Item_ValidateTypeData` and `MenuParse_itemDef` retain typed allocation sizes and typed window clears, including the prior d4b7503 adaptation. `UI_Alloc` forwards to `Hunk_AllocAlignInternal`, which clears every requested byte. Thus the parser's implicit defaults have zero-filled backing; this reconciliation does not recreate indeterminate native storage. The 65 item and 32 menu keyword/callback pairs, their order and null terminators match the binary tables at 0x34c8a0 and 0x34cbc0.

## Verification and limits

The local arm64 Release `cod2_macos_ded` build passed with two low-priority jobs. It excludes the UI parser, so the actual production client translation unit was also compiled using separately configured stock and CoD2x flags. Both compiled, with only the existing `strlen` narrowing warning in this file. Five generic OFF source-body configurations match the base, and actual SDK preprocessing with `COD2_X64` removed is byte-identical under stock and CoD2x flags. This file contains no `__LINE__` expansions.

The existing `tests/lp64/ui/run.py` suite passed under both configured stock and CoD2x builds, including its AddressSanitizer/UndefinedBehaviorSanitizer fixtures. It exercises UI handlers, listbox input/painting, version formatting, browser filters and input routes; it does not exercise these parser corrections. No fixtures were changed and no new tests, full CI or replays were run. These changes are compiled and reviewed; parser failures, rendered menus, shadows controls and full game execution were not exercised here. Existing retail edge hazards remain outside defined-input equality: `ItemParse_textfile` reads into a 4096-byte static buffer without a length bound, and the overlong-key diagnostic in `UI_MapLoadInfo` can refer to an uninitialized key on its first iteration. Their existing source behavior was compared and left in place; neither hazard was exercised or newly copied.

No assigned entries remain unchecked. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-ui-parsing.noindex/`; it is not committed.

## Integrated batch verification

Integrated production source d26c2d3 plus existing-fixture correction 9391076 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.
