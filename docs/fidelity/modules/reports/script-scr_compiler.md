# script/scr_compiler module reconciliation

Updated 2026-11-05.

**199/200 reconciled: 152 equal, 41 fixed, 6 intentional; 1 unchecked.** Mac 1.3 i386 is the authority. Linux names were used to identify inline helpers, never to override Mac. The 32 exported Mac compiler functions were read, including the complete statement, local-variable analysis, loop, switch and script-linking callers. The eight dispatch tables at 0x329dc0, 0x329ecc, 0x329fd0, 0x329fec, 0x32a0e8, 0x32a1d0, 0x32a288 and 0x32a300 were decoded from the binary (414 entries).

## Corrections

**Percent-animtree used the array object ID.** Mac 0x95e58-0x95eac checks and emits `scrAnimPub.animTreeIndex`, at i386 offset 0x414. Native read `animtrees`, the separate array object handle. It now uses the selected animation-tree index and reports a missing `#using_animtree` when that index is zero.

**Developer-code links bypassed the far-call check.** Mac 0x92054-0x92064 sends accepted developer references through the sentinel check at 0x92014. Native applied that check only to ordinary code positions. Native also continued its patch loop after an error; Mac 0x9200b, 0x92078 and 0x9208d return. Both paths now match Mac.

**Operand allocations did not always update the compiler position.** Mac records each emitted byte, short, integer and float operand in `scrCompileGlob.codePos`. Native omitted the store for several constants, short-circuit jumps, debugger operands, waittill/formal cached-local operands, profile bytes and switch records. The relevant sites include 0x94951-0x94a4b, 0x94e44-0x94f79, 0x961b4-0x961ca, 0x9627b-0x96291, 0x96f05-0x96fa5, 0x978b0-0x978c1, 0x97a38-0x97a49, 0x99a7f-0x99aa8 and 0x99f81-0x99f92. Forty-one fixed ledger entries include inline helpers affected by these and the other corrections; they are not forty-one independent defects.

**Local-variable analysis kept doing work after aborts.** Mac skips child normalization when the parent has aborted, for example at 0x92b32-0x92b50 and 0x934b1-0x934cf. Native now applies that guard. Switch analysis also drops an aborting case immediately, as at 0x929c4-0x929d8 and 0x93e1b-0x93e4b, instead of continuing to analyze statements in that case. Finally, common-name merging now preserves Mac's unchecked append at 0x930a2, 0x932d3 and 0x94117; normal name registration retains its separate 64-name check.

## Native representations and limits

Five intentional entries preserve native pointer representations: full-width builtin pointers use table-backed cache indices, and packed code positions use program-buffer offsets. The Mac lookup, developer-state, call and linking branches were compared around those adaptations. No platform-specific Windows behavior was identified or selected.

`Scr_CompileStatement` remains unchecked. Neither original Mac source-line metadata nor the executable compiler entry inventory identifies its debugger statement-compilation entry. The complete operation would compile a statement and emit an abort; ordinary statement dispatch alone does not establish it.

The two value-reference overload rows are now equal. Exact-entry Mac disassembly and original stabs identify their complete inline operations in `scr_variable.h`, rather than just similarly named calls. At 0x92860–0x92883, header line 246 loads the payload and type of both named `VariableCompileValue` locals and dispatches to the two-argument `AddRefToValue`. The compiler source resumes at 0x92884. Header line 252 identifies the matching release operations at 0x9400b–0x94019 and 0x94825–0x94833. Their enclosing constant-evaluation branches and optional boolean cast were followed to establish which value is released. Native field loads and calls implement the complete operations; no standalone wrappers or source changes are needed.

`ScriptCompile` retains its established intentional marker initialization. Mac 0x9a3d6 sets type 6 without defining the payload at stack offset -0x20; native initializes integer 1. The previously reconciled load-table consumers use marker presence/type, preserving defined native behavior instead of imitating prior stack bytes.

## Verification

The arm64 Release `cod2_macos_ded` target compiled and linked successfully with `taskpolicy -b nice -n 19` and two parallel jobs. The build reported five compiler narrowing warnings and linker warnings. `git diff --check` passed. Preprocessing the original and modified source with `COD2_X64` undefined produced identical output, confirming that the legacy source path is unchanged.

No tests were added, and no full CI or replay was run. All corrections are compiled and reviewed, not exercised. Runtime verification is left to the parent integration task.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-compiler.noindex/`. The complete 200-ID private unit ledger is `script-compiler.ledger.json` in the orchestrator's units directory.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.

## Remaining-helper audit, 2026-11-05

The two overload closures use Mac source metadata and complete inline operations; Linux supplied helper names only. All previously resolved ledger entries retain their exact status and evidence. Five unchanged source files compiled into ten separate arm64 Release objects using the configured stock and CoD2x client commands, with only checkout/output paths remapped. Builds used low priority and at most two jobs. No source, fixture, assertion or legacy line mapping changed. No full CI or replay ran in this audit. Private evidence is in `recon-script-remaining.noindex`, and the complete four-module ledger is `script-remaining.ledger.json` in the orchestrator units directory.

Independent source/binary review at source freeze `28e7d452399e38b2c296b8b4230e3eb8017b8c52` agreed with the two compiler overload closures and sixteen remaining gaps. Eight existing compiler/parser cases passed in each stock and CoD2x configuration: four binary-condition loop cases and four parser cases, using the existing ASan/UBSan runner. These checks do not exercise all reference-count types or the unresolved debugger APIs. No new cases or assertions were added.

## Original statement-entry provenance

The previously recovered original Linux 1.0 `sub_8075710` maps to the complete 1.3 body at 0x8075d58–0x8075d9f. It invokes `EmitStatement` with zero/zero/null context at 0x8075d7c, then invokes `EmitOpcode` with 128 at 0x8075d98. This refines the missing identity to a complete statement-plus-abort entry; ordinary statement emission is insufficient. `Scr_CompileStatement` remains unchecked because a complete primary Mac owner has not been established. No interpreter code or established verdict changed. No compilation, tests, CI, replay or runtime exercise ran.
