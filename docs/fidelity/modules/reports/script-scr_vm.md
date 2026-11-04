# script/scr_vm module reconciliation

Updated 2026-11-04.

**94/112 reconciled: 85 equal, 8 intentional, 1 fixed; 18 unchecked.** The main interpreter has additional local corrections but remains unchecked because its remaining error-state differences are unresolved. All pre-existing rows were unchecked. Legacy `COD2_X64=OFF` source is unchanged.

Exact-symbol Mac 1.3 disassembly was used for every exported body. The complete interpreter and notify bodies were read, along with the native active helper graph. Tables at `0x329360`, `0x32957c`, `0x329798`, `0x3297bc`, `0x3299d8` and `0x329b70` were decoded from the binary. The constant checker and call/string triage were supporting checks, not equality evidence by themselves.

## Corrections

**Debugger stepping could return an operand byte after a boolean-cast error.** Mac `0x81b30–0x81b41` and `0x81bdb–0x81bec` advance past the two-byte branch operand before checking the cast error. Native checked the error first and returned `pos+1`; it now returns `pos+3`.

**Switch default selection remembered an earlier zero-key record.** Mac `0x84fe9–0x84ff8` selects the default only when the final record has key zero. Native remembered any zero-key record. The native helper now applies the final-record condition.

**Script-call errors consumed operands or selected a depth error too early.** Mac direct function calls check depth before consuming the target (`0x83e0a`); direct methods check the receiver first (`0x83f3f–0x83f53`). Indirect methods pop their function pointer before checking the receiver and depth (`0x84e74–0x84e9c`, `0x84767–0x8478f`). Native now preserves these priorities, stack effects, and error-index assignments before reading immediate targets or parameter counts.

**Invalid non-method function pointers lost a reference before the error.** Mac `0x83ed5` and `0x8414f` retain that value, while the method-expression errors release and pop it. Native now distinguishes those paths.

## Intentional boundaries

The documented native monotonic timeout service replaces Mac's `rdtsc`/`Sys_Milliseconds` pair. It affects reset, entry wrappers and resumption. Existing invalid-ID/cache bounds and LP64 pointer/value encodings remain. These boundaries are recorded individually in the ledger; see the WS1/WS6 and 2026-10-23 F1 script reports.

The existing volatile builtin/setter error checkpoint remains as the documented native C portability correction described in `2026-10-24-F1-error-recovery.md`. It does not establish equality for every opcode error.

## Unchecked boundaries

The Mac main interpreter and Linux `VM_ExecuteInternal` rows remain unchecked. Mac `0x8622f–0x86262` explicitly resets scratch locals, including the opcode register, on the setjmp-return path. Native automatic state outside its checkpoint remains unproved across `longjmp`. The unknown-opcode path also differs: Mac falls through into frame setup at `0x839e1`/`0x84227`, while native reports a fatal opcode diagnostic. These were not silently classified as matching.

`VM_Notify` remains unchecked for allocation-dependent state. When a notify grows an archived stack, Mac `0x82407–0x82422` does not copy its time byte; native preserves it. The allocator does not initialize reused payload, so no fixed reference byte value was established. Native deterministic initialization was retained.

The other 15 unchecked rows lack an established Mac text symbol or inline identity, or belong to the unresolved generic recovery path: the two dummy-value helpers; `Scr_AddAnim`, `Scr_AddExecEntThreadNum`, `Scr_AddIString`, `Scr_DecTime`, `Scr_ErrorWithDialogMessage`, `Scr_ExecCode`, both thread-enumeration helpers, `Scr_GetAnimTree`, `Scr_GetConstIString`, `Scr_GetFunc`, `Scr_IsEndonThread` and `Scr_StackClear`. Several have optional debugger bodies in `scr_debugger_eval.c`, outside this worker's files. No Linux-only body was used as reconstruction authority.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. `git diff --check` passed. `unifdef -UCOD2_X64` output is byte-for-byte identical to assignment base `3e06e90`, and seven inactive compiler configurations produce identical preprocessed bodies.

No tests were added or changed. No full CI or replay ran. The corrections are compiled and reviewed, not exercised; the orchestrator owns integration and runtime validation.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-script-vm.noindex/`. The complete 112-entry unit ledger is delivered separately to the orchestrator.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
