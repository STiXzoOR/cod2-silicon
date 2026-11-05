# ui/ui_shared_mp module reconciliation

Updated 2026-11-05.

**103/103 reconciled: 76 equal, 21 fixed, 6 intentional.** Mac 1.3 i386 is the authority. Every assigned row has a named Mac body; no verdict relies on a missing identity. The comparison includes the native helper behind `Scroll_Slider_SetThumbPos`, which the triage parser reports as missing because its native name ends in `_impl`.

## Corrections

| Mac body | Correction |
| --- | --- |
| `Script_SetBackground` 0x16562e; `Item_Paint` 0x16fe0e | Material registration uses the item's image track, including style-6 dvar backgrounds. |
| `Controls_SetConfig` 0x165ec2 | Queue `in_restart`, matching the reference, after applying bindings. The unregistered `bindingsave` string originated in reconstruction commit 63de35a. Current client `Sys_Init` registers `in_restart`; its callback shuts down and reinitializes input, including the native raw-mouse lifecycle. There was no binding-save provider to preserve. |
| `Menu_CheckOnKey` 0x16654a | Visible item key handlers additionally require focus or static flag 0x100000. |
| `Menus_Close` 0x1669d8; `Menus_CloseAll` 0x166b90 | An absent-stack menu loses flags 6 without running its close script. Close-all visits menu definitions in order. |
| `Menu_SetPrevCursorItem` 0x16ed18; `Menu_HandleKey` 0x16f560 | Restore failed-wrap cursor handling, left-arrow previous-item navigation, and redispatch of the same mouse click after leaving an edit field. |
| `Item_SetFocus` 0x16b386; `Item_MouseEnter` 0x16c63e | Restore parent occlusion rejection and correct X/Y transforms. Text hover uses its text rectangle and baseline, and listbox hover runs outside that rectangle. |
| `Item_Text_AutoWrapped_Paint` 0x166eac | Use height+1 or height+4 line spacing, a strict space-break threshold, and the reference right-alignment anchor. |
| `Item_TextColor` 0x169418; `Item_OwnerDraw_Paint` 0x16a712 | Restore the text-style-1 pulse phase. |
| `Item_TextField_Paint` 0x16994a | The value draw uses the reference focus pulse/forecolor calculation without a second fading pass. Cursor eligibility uses dynamic visibility/focus flags. |
| `Item_TextField_HandleKey` 0x16ee5e | Preserve the reference navigation/accept fallthrough for extended character bytes 0x9a, 0x9b and 0xbf after a successful write. |
| `Item_ListBox_OverLB` 0x16bb24 | Restore horizontal scrollbar geometry and all five arrow/thumb/page flags. The final page-region extent deliberately retains the reference's absolute end coordinate. |
| `Item_ListBox_Paint` 0x167664 | A selected text row without scrollbars fills width minus 4 rather than width minus 20. |
| `Scroll_ListBox_ThumbFunc` 0x16d15a; `Scroll_ListBox_AutoFunc` 0x16d33e | Restore one repeat event per callback, reload time after the handler, and return immediately for a stationary horizontal thumb. |
| `Window_Paint` 0x168304 | Preserve ordered fade comparisons when alpha is unordered. |

The command and binding tables were checked by script: all 28 command names, handler identities and their order match, as do all 56 binding command strings and four integer fields. Style/type jump tables at 0x33b330, 0x33b350 and 0x33b374 were decoded from the binary before reviewing dispatch.

## Preserved native behavior

The six intentional rows retain optional native paint diagnostics, engine case folding and invalid-parent handling in `Script_SetFocusByDvar`, clearing the native bind-item pointer after a completed operation, and the documented binding-name/`EXE_KEYWAIT` display in place of the Mac's unlabeled `FIXME` draw. Existing defined paint-offset/cursor limits and missing-enum checks remain. The WS42 third-binding replacement, type-0 OOB hit restriction and numeric localization repairs remain in place.

The existing numeric-field fixture incorrectly marked static flags as focused. Its unchanged cursor assertion failed once after the production gate was corrected. The fixture now provides a valid parent and dynamic flags, and extracts the real `LerpColor` helper along with the real paint function. No cases or assertions were added or removed.

## Verification and limits

The arm64 Release `cod2_macos_ded` target builds. Because it excludes this UI translation unit, `ui_shared_mp.c` was also compiled explicitly using each configured production client command, stock and CoD2x: both succeed with no diagnostics. Builds and fixture runs used low priority, with no more than two jobs.

Existing `tests/lp64/ui/run.py` passes in both separately configured builds. Existing `tests/fixes13/run.py --test numeric_fields` also passes in both builds after the fixture setup correction. The latter exercises numeric localization, cursor drawing and paint offsets; it does not assert the pulse colors. The other nineteen corrected functions were compiled and reviewed, without exercising their corrected paths. No new tests, full CI or replays were run.

Five legacy preprocessing configurations match the base. `unifdef -UCOD2_X64` output is byte-identical for both changed C files, including whitespace. Neither changed C file uses `__LINE__`. There are no shared-header dependencies or edits to other UI owners' files.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-ui-shared.noindex/`. The complete unit ledger is `ui-shared.ledger.json` in the orchestrator's private units directory. Nothing remains unchecked.

## Integrated batch verification

Integrated production source 02a2156 plus existing-fixture correction ac63727 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Corrected text rectangle unordered rejection (provenance-uihit23)

Menu_HandleMouseMove0x16dfa0 changes from equal to fixed. In the second-pass
type0/text-nonnull branch, Mac0x16e26f–0x16e2ab uses four UCOMISS/JB comparisons
to reject outside or unordered corrected-text coordinates. Native rejected only
ordered `<` or `>` violations and could therefore admit an unordered text bound.
The COD2_X64 predicate now uses four negated `>=` comparisons in the same
X-lower/X-upper/Y-lower/Y-upper order, preserving the rounded upper additions
and equality behavior. The complete old OFF text remains unchanged.

Independent review established a defined-storage witness with a finite enclosing
item rectangle and cursor, but separate textRect.x quiet NaN, supplied through
the actual Item_SetTextRect0x19235e field-copy owner. Other text fields are finite,
alignment5 leaves the text fields in place, and the visible type0/textnonnull
item passes earlier capture/edit/bind/dvar and outer-rectangle gates. Mac skips
the item; old native can enter Item_MouseEnter and change flags4 to5. Null scripts
and static flag0x100000 isolate that effect from later focus processing. The
actual Item_SetTextExtents textalignx float path0x166cfc/0x166d75 to setter0x166d92
also preserves independent X provenance with finite textscale and dimensions,
without a NaN-to-integer conversion. This is a static function-entry storage
witness; no parsed-asset, gameplay, frequency or runtime occurrence is claimed.

The initial item-window rectangle predicate already matches Mac and is outside
this repair; the separate PointInBounds2D alias does not inherit the text-specific
defect. Previously approved UI ownership/layout changes remain intact.

The changed TU was manually compiled with both applicable configured client
commands, stock and CoD2x, at SHA256
`d14bc0d8bb85f6c3c6fb5663202a976c9da280e06730734064368b2a28f86a45`.
Both succeeded. Those databases contain no dedicated UI TU, so none is claimed.
Full unifdef OFF output is byte-identical and the source contains no __LINE__,
#line or assert tokens. Review: review-provenance-uihit23.md. Repair evidence:
recon-provenance-uihit23.noindex; independent witness:
recon-provenance-menuhover22.noindex. Compiled and reviewed, not exercised.
No tests, CI, full build, replay or runtime verification.
