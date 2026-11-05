# ui/ui_main_mp module reconciliation

Updated 2026-11-05.

**64/65 reconciled: 46 equal, 15 fixed, 3 intentional; 1 unchecked.** Mac 1.3 i386 is the authority. Every assigned symbol was identified and its body read. Triage and constant checks supported the comparison; the active stock native body was preprocessed with the engine compile database. Exact-symbol disassembly was also checked for nine functions, and the active-menu, key-handler, server-column and owner-draw jump tables were decoded from the binary.

## Corrections

**Fonts, keys and translated strings.** `UI_GetFontHandle` reversed the bold-font size threshold for font enum 4 (Mac 0x14f1ac). `UI_CheckExecKey` accepted navigation/action keys without an `onKey` entry, unlike Mac 0x14f9ae. `UI_SafeTranslateString` now skips lookup after a leading 0x15, as 0x14faa3 does. Successful favorite additions again format the translated message with the address (0x14fbd1).

**Browser initialization and ordering.** Initial sorting uses column 10 (0x150ec4), the fallback title says 1.3 (0x15221f), and the PB filter reads `pb` after the killcam filter (0x1524e8). The insertion search now keeps the tested midpoint and inserts equal servers there (0x15257d-0x152711). Status parsing preserves the current value/null cursor and continues searching duplicate dvar rows (0x150074-0x150178). The refresh path reloads the source and current count after callbacks (0x15481d, 0x1548b4).

**Player searches.** Completed and timed-out status requests return to cancellation/refill in the same pass (0x154317-0x154351). Native had skipped refill, allowing early completion or repeated timeouts. The final summary now uses slot `numResults`, matching 0x1544d8, instead of overwriting the preceding result.

**Menu lifetime and dispatch.** Ingame menus are loaded by `CG_Init` before its first `SCR_UpdateScreen`, rather than eagerly by `UI_Init`. The removed eager call originated in the root reconstruction commit, as did the extra popup HUD write. The actual remote-client path shuts down UI/cgame, restarts the hunk, runs `CL_StartHunkUsers -> CL_InitUI -> UI_Init`, then `CL_InitCGame -> CG_Init -> UI_LoadIngameMenus`. Video restart follows the same UI-before-cgame ordering. `UI_LoadMenus` provides the transient list which `UI_AddMenuList` appends to the display context; `g_ingameMenusLoaded` resets in `UI_Init`. Popup cases 9/10 now omit the HUD write, matching 0x1512bb; quick-message case 8 retains it. The key-handler table at 0x33ae70 selects net game type with 0xf5, not 0xf4.

**Missing filter data and draw arithmetic.** Mac `serverFilters[0]` at 0x36cf80 contains the All description and an empty base directory; native's private static was all zero. Both draw consumers also accept index 1, whose words at 0x36cf88/0x36cf8c are zero padding. Native now declares that second zero row explicitly. No generated data/import provider owns this private static. Owner-draw text now passes rectangle alignments (0x153ab3), dims colors through double multiplication by 0.8 (0x153d14), and follows the talker label's addition order (0x153a18).

**Menu scripts.** Restored the PB start/join gates and `setPbClStatus` dispatch (0x154938, 0x155e2f, 0x1564cd). These call the existing engine functions; the documented `pb_null_adapter.c` backend remains a no-op. Profile lists use allocation/free type 3 (0x155215, 0x155251); creating a profile resets feeder selection before selecting the new profile (0x155840). Deletion reloads profile state after the callback. The movie-list scratch buffer is 4096 bytes, matching the request at 0x155a16, rather than 2048.

**Connection and download screens.** Restored detached WWW download display (0x157719), map display-name assignment (0x157d0f), and the missing server-message wrap/draw loop at y=299 with 22-pixel rows (0x15795e-0x157cc8). Download text uses its supplied center, puts the initial copied line at yStart+340 (0x157052), and translates paired time units in Mac order (0x157176, 0x15762a). Independent review also found that before a transfer starts, zero total size still uses the four-field copied/of/total text at 0x156fb2–0x156fec. The native condition now retains that text; the two-field unknown-size form remains restricted to started transfers, as at 0x1575c9–0x1575ee.

## Adaptations and unresolved storage

The two command wrappers retain opt-in native trace prints. `UI_FeederItemText` retains its null status-line guard. Existing trace/gamepad hooks, CoD2x download scale/spacing, and map/profile bounds guards remain. Browser insertion shifts the live array rather than copying Mac's extra inactive slot, which can cross declared storage at the limit. The filter table's second row is an explicit padding-to-defined-storage adaptation. No original binary, disassembly or reference source is committed.

`UI_OwnerDraw` remains **unchecked** despite its ordinary draw corrections. At Mac 0x154012, a talking client absent from the rebuilt player list produces index -1 and reads a string starting 32 bytes before `playerNames`, inside preceding asset storage. Native has the inherited negative-index fallback, but LP64 asset bytes do not establish an equivalent valid-string contract. That fallback is unchanged; this report does not claim it is reconciled or safe.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built at background priority with two jobs. It excludes this client UI translation unit, so the actual production `ui_main_mp.c` was separately compiled with remapped configured stock and CoD2x engine flags; both succeeded. Existing narrowing warnings remain. All five `COD2_X64=OFF` guard configurations match the base; the checker's sole expected mismatch is its native-on configuration. This source contains no `__LINE__` expansions.

The existing `tests/lp64/ui/run.py` suite passed with both stock and CoD2x databases, including sanitized binding, listbox/paint, browser and input-route fixtures. The first runs failed because the browser mock recognized `punkbuster` while Mac and the real `LAN_GetServerInfo` provider use `pb`. Only that mock key changed; every assertion remains, and both initial failure logs were preserved. No tests were added.

The browser fixture exercises the default and both PB filter choices. It does not exercise the complete browser search/insertion flow, popup lifecycle, profile management, connection messages, download ETA layout, or talker fallback. Those changes are compiled and reviewed, not runtime-validated. Integration and full runtime validation remain with the orchestrator.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-main.noindex/`. Private verdicts cover all 65 assigned IDs in the unit ledger.
