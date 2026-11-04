# ui/ui_shared_mp module reconciliation

Updated 2026-11-05.

**103/103 reconciled: 77 equal, 20 fixed, 6 intentional.** Mac 1.3 i386 is the authority. Every assigned row has a named Mac body; no verdict relies on a missing identity. The comparison includes the native helper behind `Scroll_Slider_SetThumbPos`, which the triage parser reports as missing because its native name ends in `_impl`.

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
