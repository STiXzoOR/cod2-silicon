# script/scr_variable module reconciliation

Updated 2026-11-04.

**162/173 reconciled: 144 equal, 14 fixed, 4 intentional; 11 unchecked.** All 124 named Mac functions were compared in full. Another 38 Linux-only rows were identified as complete operations in Mac callers, including both `Scr_GetEntryUsage` overloads. Linux was used for helper names and intent only. The live changes are confined to `COD2_X64`; preprocessing with it undefined is identical to the starting source.

The operator tables at 0x329d40 and 0x329d70 and debug-string table at 0x329d00 were decoded from the binary. All dispatch cases match. The 24 type names, five class-map records, float absolute-value mask and equality epsilon were also read from binary data. The epsilon is exactly `9.999999974752427e-07f`. Full-binary disassembly misaligned `GetVarType`; exact-symbol disassembly confirmed its actual entry at 0x8837c. Exact-symbol disassembly also confirmed the unusual paths below.

## Corrections

**Initialization and freeing wrote the wrong fields.** Mac `Var_Init` at 0x88448 initializes the free list without touching `nextSibling`. Native additionally cleared it. Mac empty-object cleanup at 0x886d0, 0x8cf25, 0x8f735 and 0x8e99e clears all four status bytes. Native `FreeVariable` cleared one byte, leaving name bits behind. The native helper now clears the full word.

**Dormant index helpers omitted sibling insertion.** Mac `GetNewVariable` at 0x89b63–0x89b94 inserts the allocated index into the parent's sibling ring before returning. Native `GetNewVariableIndexInternal` returned directly from `Internal3`; `GetVariableIndexInternal` inherited the omission. The native index helper now links the child. Existing public value-ID wrappers already linked correctly.

**Field parsing changed temporary memory state.** Mac resets temporary storage directly at 0x88dfc, lowercases the terminating NUL as well as the name at 0x88f84, and requests exactly `strlen(name)+4` bytes at 0x88fc5. Native promoted temporary hunk memory before the reset, omitted the NUL from lowercasing, and requested an extra byte followed by a rewind. Those differences are removed from the live native path.

## Defined native adaptations

**Concatenation preserves Mac's left-length check.** At 0x9042d Mac compares the left length in EDI with 0x2000, although it already computed the combined length. Native had checked combined length instead. Native now uses the original acceptance check, with stack storage sized for the combined string through `__builtin_alloca`; the existing MSVC compatibility shim maps it to `_alloca`. Length addition uses `size_t`. `SL_GetStringOfLen` allocates its NUL-inclusive length plus a four-byte header, and `MT_GetSize` rejects allocations above 0xffff, so valid strings contain at most 0xfffa non-NUL bytes. Together with the accepted left length, this bounds temporary storage at 0x11ffb (73,723) bytes. Allocation happens only for accepted string operands, and normal return or a script-error `longjmp` releases its stack frame without heap cleanup. This intentionally avoids the Mac fixed-buffer overrun; results depending on that corruption cannot be equivalent.

**Entity array copies avoid an invalid destination write.** Mac `Scr_EvalArrayRef` sets its destination pointer to zero at 0x8f1b7 and writes through offset 4 at 0x8f32e if a second copy is needed. `ClearArray` does the same at 0x919b2/0x91bdb. Native's existing `fieldId` guard returns or clears the detached copy without that invalid write. Both ledger rows are intentional. Lookup, read-only checks, type errors, reference handling and ordinary copy-on-write paths were compared.

**Direct missing-key removal remains a no-op in native.** Mac `RemoveVariable` continues with sentinel index zero at 0x8df4f and calls `MakeVariableExternal` at 0x8df6a. Native retains its guard against unlinking the free-list sentinel. `ClearArray` has its own safe missing-integer-key behavior in Mac at 0x91c4e/0x91e9e; native now expresses that explicitly with `SafeRemoveVariable_core`.

Native encoded vector/code-position storage, pointer-sized thread-debug records and opt-in allocation diagnostics are retained. Full reference and sibling cleanup paths, the recursive object-usage calculation and the complete thread-dump scan/group/output were compared, including the existing adjacent-collision hash-read ordering at 0x8cc5e.

## Unchecked identities

These Linux-only rows have no established Mac implementation or caller identity: `CopyEntity`, `Scr_CopyEntityNum`, `Scr_AddStringSet`, `Scr_InitStringSet`, `Scr_ShutdownStringSet`, `Scr_DumpScriptVariablesDefault`, `Scr_FindAllVariableField`, `Scr_GetChecksum`, `Scr_GetEntClassId`, `Scr_GetEntNum` and `Scr_IsThreadAlive`. They remain unchecked; Linux similarity alone is insufficient. `Scr_FindAllVariableField` has a native debugger implementation outside this unit. The separately named debugger `Scr_GetArrayIndexValue` also lacks an established Mac identity; its behavior is not covered by the inlined array-name decoder verdict.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with `taskpolicy -b nice -n 19` and two jobs. The existing 28-case variable fixture suite passed under AddressSanitizer and UndefinedBehaviorSanitizer, including collision churn, packed fields, reference cleanup, copy-on-write, absent-key clearing, unaligned vectors and thread dumps. No new tests, full CI or replays were run.

The complete native translation unit also compiled with `-Werror=vla`. Windows stack allocation follows the existing MSVC compatibility mapping; `cl.exe` itself was not available on this Mac.

The field-file parser changes, preservation of stale free-slot sibling words, full high-status-word clearing, dormant index wrappers and extended concatenation acceptance were compiled and reviewed but not independently exercised. Runtime integration remains with the orchestrator.

Private evidence and full ledger: `~/Library/Application Support/CoD2-native-decomp/recon-script-variable.noindex/`. The unit ledger is `_agents/decomp-recon/units/script-variable.ledger.json` in the private orchestration workspace.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.

## Remaining identity follow-up, 2026-11-05

All eleven unresolved variable helpers remain unchecked. Exact-symbol review
rejects a tempting getter substitution: `Scr_GetEntityIdRef` at
0x8889e–0x888c3 returns the packed entity number and class number. Its ordered
STABS records identify its own original source lines 4185, 4190, 4197 and 4198.
The body does not establish either the standalone integer-result `Scr_GetEntNum`
API or the class-character lookup `Scr_GetEntClassId`; it never reads
`scrClassMap.charId`. The inverse class-character lookup does not supply the
latter operation either.

The optional debugger's `Scr_FindAllVariableField` is a live implementation,
with count and fill calls in `scr_debugger_ui_watch.c:902–932`. Its native body
at `scr_debugger_eval.c:490–511` enumerates child IDs after a type gate. The Mac
absolute-zero exception symbol supplies only a signature, not the original
complete enumeration/output contract. Its presence in native code is neither
closure evidence nor a reason to implement the Linux reconstruction's different
operation. The other eight missing variable identities retain their existing
evidence; no new executable mapping was found.

No source, fixture, or established verdict changed. No compilation, CI, tests,
or replay ran. This bounded identity investigation establishes no new runtime
coverage. Private exact-symbol bodies, ordered symbol metadata and the full
546-row unchanged-or-refined script packet are in
`CoD2-native-decomp/recon-identity-script3.noindex` and the orchestration
`units/identity-script3.ledger.json`.
