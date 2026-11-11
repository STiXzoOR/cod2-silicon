# script/scr_main module reconciliation

Updated 2026-11-05.

**17/18 reconciled: 12 equal, 2 fixed, 3 intentional; 1 unchecked.** Mac 1.3 i386 is the primary authority. Every named Mac body was read, including the full load, scan and cleanup paths. The constant checker and triage ran for all assigned names. The legacy statements remain unchanged under the conditional branches; `unifdef -U COD2_X64` confirms the selected OFF source is byte-identical to the starting source.

`Scr_LoadScript` called `GetArray` for a script's thread table. Mac 0xaf066 calls `GetObjectA`. The two allocate different parent types (array 0x16 versus object 0x13), so native now creates the object under `COD2_X64`. Its existing NULL-filename early return remains a defined invalid-input boundary.

The three intentional rows describe LP64 representation: full-width opcode-address bounds, function handles already stored as offsets, and resetting the external animation-reference table after animtree variables are released. The existing unconditional user-1 cleanup in `Scr_FreeScripts` matches Mac 0xaf24b onward.

`Scr_InitEvaluate` is inlined in `Scr_BeginLoadScripts` at 0xaf394–0xaf3ab; `Scr_EndLoadEvaluate` is inlined in `Scr_EndLoadScripts` at 0xaf0c7–0xaf0de. Their caller implementations match.

`Scr_IsInScriptMemory` remains unchecked: no complete two-bound script-memory predicate has been established. `Scr_BeginLoadScriptsRemote` now maps to the complete consumed initialization phase documented below; this does not establish a separate remote entry or lifecycle.

The arm64 Release `cod2_macos_ded` target compiled and linked with `taskpolicy -b nice -n 19` and two jobs, including a final incremental compile of both assigned files. The correction is compiled and reviewed, not exercised. No new tests, CI or replays ran. No shared headers or other units were edited.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-script-main.noindex/`. All 18 IDs are recorded in the private `script-main.ledger.json`.

## Integrated batch verification

Integrated source `24bd887` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.

## Remaining-helper audit, 2026-11-05

Original ordered Mac source metadata and native call sites were checked again. The executable `Scr_BeginLoadScripts` has a complete local-loading lifecycle; its shared prefix does not identify a separate remote entry. `Scr_IsInOpcodeMemory` checks the program length, while the unresolved script-memory API names the separate end-of-script-buffer boundary. Its zero-address `.eh` metadata provides a signature but no executable operation. Both helpers remain unchecked; neither is closed through partial overlap or absence of callers. Native code is unchanged.

The assigned files compiled as actual configured stock and CoD2x arm64 Release client objects in a separate evidence directory, with low priority and at most two jobs. No source or fixture was edited. Previously resolved ledger evidence is preserved exactly in the full `script-remaining.ledger.json`. No full CI or replay ran.

Independent source/binary review at source freeze `ac2d6ab9292906dafe5313b8f12967a08a656021` agreed with the two compiler overload closures and sixteen remaining gaps. Eight existing compiler/parser cases passed in each stock and CoD2x configuration: four binary-condition loop cases and four parser cases, using the existing ASan/UBSan runner. These checks do not exercise all reference-count types or the unresolved debugger APIs. No new cases or assertions were added.


## Original boundary identities

Original1.0 IDB byte bodies uniquely relocate to original1.3 `Scr_IsInScriptMemory` at 0x8075e40 and `Scr_BeginLoadScriptsRemote` at 0x8076196. The former makes two unsigned comparisons against programBuffer and endScriptBuffer; the latter sets script_loading, initializes opcode lookup, allocates loadedScripts and returns. These are original executable identities, not inferred from missing symbols.

Fresh exact Mac `Scr_IsInOpcodeMemory` at 0xaed58 instead compares a programBuffer-relative offset to programLen. Mac `Scr_EndLoadAnimTrees` stores endScriptBuffer at 0xaf20e, and local load initialization clears it at 0xaf38d; these writes do not implement the two-bound predicate. Exact Mac `Scr_BeginLoadScripts` continues through the local-loading lifecycle, so the shared initialization prefix still does not establish the remote-return operation. Both rows remain unchecked. No production code, compiled objects, tests, CI or replays changed or ran.

## Complete initial script-load phase (remote22)

`Scr_BeginLoadScriptsRemote` is fixed through the complete initial state operation consumed by actual Mac/native `Scr_BeginLoadScripts`. Original1.0 0x8075b54–0x8075b71 maps uniquely to the full30-byte original1.3 body0x8076196–0x80761b3: set script_loading, initialize opcode lookup, allocate/store loadedScripts, return void. Mac performs all three operations contiguously at0xaf348–0xaf358, before the next allocation at0xaf359. Native has that exact phase. The later three arrays, program/canonical resets and animtree initialization belong to the local loader and are excluded. This supersedes the earlier whole-local-versus-remote rejection only for the complete selected state phase; no standalone remote API is invented.

Full providers were read, including developer-off return and developer-on opcode/source table allocation and clearing, all free-list allocation branches, array status0x76 and returned id. Fixed inherits the existing `Z_MallocInternal` guard-heap OOM reporting correction reached through the developer-on provider. No new defect was found. Existing native pointer-sized record allocation, diagnostics, terminal UI and ownership adaptations remain. Terminal variable exhaustion first dumps state, preserves the first error message and takes the loading-state Com_Error policy; continuation after a returned fatal handler is not equated (Mac resumes the allocator through index0, native returns0). Failed-allocation continuation and repeated-initialization cleanup are likewise not claimed.

The resulting array has a real consumer: Mac `GScr_LoadScripts` calls initialization at0x1a0353 and `Scr_LoadScript` at0x1a035f; the latter reads loadedScripts at0xaef54/0xaef92 for lookup and insertion. Native startup reaches the same typed state and consumer under its existing loading schedule. This establishes consumed state initialization, not remote transport or the whole later lifecycle.

Independent review is recorded in units/review-provenance-remote22.md; fresh complete original and Mac providers/owners are retained in recon-provenance-remote22.noindex. No source changed. No compilation, tests, CI or replay ran. Inherited corrections remain compiled and reviewed, not runtime verified.

## Script-range state boundary — 2026-11-06

`Scr_IsInScriptMemory` remains unchecked. Its complete original 0x8075e40 body reads programBuffer and endScriptBuffer and returns the conjunction of two unsigned comparisons. The retained exact script7 producer/bounds evidence was reread; the endScriptBuffer store does not supply this query. Fresh full Mac `Scr_GetFunctionHandle` 0xaf28e instead resolves filename/name state and filters its offset against programLen at 0xaf332. No actual complete endScriptBuffer-based predicate or producer-backed equivalence between those upper-bound roles was established. This is a specific state/operation gap, not closure from a missing name or caller. The approved native offset representation remains unchanged.

Private evidence: `recon-provenance-scriptget32.noindex`, with prior `recon-provenance-script7.noindex` retained. No source change, compilation, tests, CI, replay or runtime execution ran.
