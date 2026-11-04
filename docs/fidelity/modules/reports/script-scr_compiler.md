# script/scr_compiler module reconciliation

Updated 2026-11-04.

**196/200 reconciled: 150 equal, 41 fixed, 5 intentional; 4 unchecked.** Mac 1.3 i386 is the authority. Linux names were used to identify inline helpers, never to override Mac. The 32 exported Mac compiler functions were read, including the complete statement, local-variable analysis, loop, switch and script-linking callers. The eight dispatch tables at 0x329dc0, 0x329ecc, 0x329fd0, 0x329fec, 0x32a0e8, 0x32a1d0, 0x32a288 and 0x32a300 were decoded from the binary (414 entries).

## Corrections

**Percent-animtree used the array object ID.** Mac 0x95e58-0x95eac checks and emits `scrAnimPub.animTreeIndex`, at i386 offset 0x414. Native read `animtrees`, the separate array object handle. It now uses the selected animation-tree index and reports a missing `#using_animtree` when that index is zero.

**Developer-code links bypassed the far-call check.** Mac 0x92054-0x92064 sends accepted developer references through the sentinel check at 0x92014. Native applied that check only to ordinary code positions. Native also continued its patch loop after an error; Mac 0x9200b, 0x92078 and 0x9208d return. Both paths now match Mac.

**Operand allocations did not always update the compiler position.** Mac records each emitted byte, short, integer and float operand in `scrCompileGlob.codePos`. Native omitted the store for several constants, short-circuit jumps, debugger operands, waittill/formal cached-local operands, profile bytes and switch records. The relevant sites include 0x94951-0x94a4b, 0x94e44-0x94f79, 0x961b4-0x961ca, 0x9627b-0x96291, 0x96f05-0x96fa5, 0x978b0-0x978c1, 0x97a38-0x97a49, 0x99a7f-0x99aa8 and 0x99f81-0x99f92. Forty-one fixed ledger entries include inline helpers affected by these and the other corrections; they are not forty-one independent defects.

**Local-variable analysis kept doing work after aborts.** Mac skips child normalization when the parent has aborted, for example at 0x92b32-0x92b50 and 0x934b1-0x934cf. Native now applies that guard. Switch analysis also drops an aborting case immediately, as at 0x929c4-0x929d8 and 0x93e1b-0x93e4b, instead of continuing to analyze statements in that case. Finally, common-name merging now preserves Mac's unchecked append at 0x930a2, 0x932d3 and 0x94117; normal name registration retains its separate 64-name check.

## Native representations and limits

Five intentional entries preserve native pointer representations: full-width builtin pointers use table-backed cache indices, and packed code positions use program-buffer offsets. The Mac lookup, developer-state, call and linking branches were compared around those adaptations. No platform-specific Windows behavior was identified or selected.

Four rows remain unchecked. `AddRefToValue(VariableValue *)` and `RemoveRefToValue(VariableValue *)` are offsource overload identities; this compiler imports the separate two-argument functions. Their caller field loads were checked, but a standalone overload identity was not established. `Scr_CompileStatement` has no identified Mac symbol or proven inline entry and no body in this source.

`ScriptCompile` was read completely, including an exact-symbol disassembly restart. Its terminal marker at 0x9a3d6 sets type 6, then passes the payload at stack offset -0x20 to `SetVariableValue`; no defining payload store appears in the function. Native initializes that marker to integer 1. This discrepancy remains unchecked; the code was not changed to imitate uninitialized stack data.

## Verification

The arm64 Release `cod2_macos_ded` target compiled and linked successfully with `taskpolicy -b nice -n 19` and two parallel jobs. The build reported five compiler narrowing warnings and linker warnings. `git diff --check` passed. Preprocessing the original and modified source with `COD2_X64` undefined produced identical output, confirming that the legacy source path is unchanged.

No tests were added, and no full CI or replay was run. All corrections are compiled and reviewed, not exercised. Runtime verification is left to the parent integration task.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-compiler.noindex/`. The complete 200-ID private unit ledger is `script-compiler.ledger.json` in the orchestrator's units directory.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
