# script/scr_main module reconciliation

Updated 2026-11-04.

**16/18 reconciled: 12 equal, 1 fixed, 3 intentional.** Mac 1.3 i386 is the primary authority. Every named Mac body was read, including the full load, scan and cleanup paths. The constant checker and triage ran for all assigned names. The legacy statements remain unchanged under the conditional branches; `unifdef -U COD2_X64` confirms the selected OFF source is byte-identical to the starting source.

`Scr_LoadScript` called `GetArray` for a script's thread table. Mac 0xaf066 calls `GetObjectA`. The two allocate different parent types (array 0x16 versus object 0x13), so native now creates the object under `COD2_X64`. Its existing NULL-filename early return remains a defined invalid-input boundary.

The three intentional rows describe LP64 representation: full-width opcode-address bounds, function handles already stored as offsets, and resetting the external animation-reference table after animtree variables are released. The existing unconditional user-1 cleanup in `Scr_FreeScripts` matches Mac 0xaf24b onward.

`Scr_InitEvaluate` is inlined in `Scr_BeginLoadScripts` at 0xaf394–0xaf3ab; `Scr_EndLoadEvaluate` is inlined in `Scr_EndLoadScripts` at 0xaf0c7–0xaf0de. Their caller implementations match.

Two rows remain unchecked. No executable Mac identity was established for `Scr_BeginLoadScriptsRemote` or `Scr_IsInScriptMemory`. The latter has only a zero-address absolute `.eh` symbol. Native has no body or callers for either. Similarity to a caller prefix does not establish the missing remote entry.

The arm64 Release `cod2_macos_ded` target compiled and linked with `taskpolicy -b nice -n 19` and two jobs, including a final incremental compile of both assigned files. The correction is compiled and reviewed, not exercised. No new tests, CI or replays ran. No shared headers or other units were edited.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-script-main.noindex/`. All 18 IDs are recorded in the private `script-main.ledger.json`.
