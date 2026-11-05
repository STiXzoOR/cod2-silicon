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

## Batch 19 follow-up

The remaining generic nonlocal live-frame/scratch and bounded unknown-opcode continuation gaps are addressed in script-scr_vm-recovery.md. The retained builtin/setter checkpoint is an intentional native adaptation and must not be described as original longjmp equality.

## Internal interpreter alias bookkeeping — 2026-11-05

Current totals: **97/112 reconciled: 85 equal, 9 intentional, 3 fixed; 15 unchecked**. The Linux-named `VM_ExecuteInternal(function_stack_t)` is the same internal interpreter covered by Mac row 0x8393a and the batch19 [recovery follow-up](script-scr_vm-recovery.md). Its evidence still cited two blockers that had already been repaired under that primary row. Native `VM_Execute(function_stack_t)` directly selects the reviewed C interpreter; this is distinct from the three-argument external-call overload.

Reread the active forwarding path, volatile live frame fields, scratch resets and bounded unknown-opcode continuation against the exact original entry/recovery ranges. The alias now carries the established fixed verdict, including the preserved builtin/setter checkpoint, timeout and frame-table bounds adaptations and prior runtime limitations. This is an update to the duplicate identity, not a fresh claim that every interpreter path was exercised. The two dummy helpers and optional debugger identities remain separately unresolved.

No engine code changed and no CI, builds or tests were run. Earlier evidence and independent review apply only to their documented scope; the parent performed this alias reconciliation directly after the worker usage cutoff.

## Dummy-object recovery — 2026-11-05

Current totals: **98/112 reconciled: 85 equal, 9 intentional, 4 fixed; 14 unchecked**. This follow-up fixes the complete `GetDummyObject` operation in opcode `0x26`; `GetDummyFieldValue` remains unchecked. It does not certify the whole interpreter.

Mac opcode `0x26` gets self at `0x85771` and checks `IsFieldObject` at `0x85782`. Failure reaches the shared object-error path through `0x867b3`. When evaluation or script loading permits that error policy to return, the direct branch to `0x86269` retains the opcode. The raw tables at `0x329b70` and `0x3299d8` then select dummy-object recovery at `0x86695–0x866b7`. Original `scr_vm.cpp` line metadata 291–292 identifies its complete clear-temp/`GetObjectA` operation.

Native previously returned from its shared error helper and retained the invalid object ID and pending error. Its native-only opcode branch now preserves that shared policy first. If it returns, native calls `RuntimeError` with the current position, index, first error message and dialog message; only after that call returns does it clear the three error fields, clear `tempVariable` and obtain its object with `GetObjectA`. Neither position nor operand-stack top changes. Normal nonlocal error exits and the established builtin/setter checkpoint are preserved.

The current stock and CoD2x caller graph does not enter the interpreter during the two modes: notify evaluation compares values and queues or terminates threads, script loading ends before gameplay execution, and optional debugger execution is a stub. The entry wrappers do not enforce that restriction, so the supplied-state difference still required correction. Opcode `0x2b` is a different field-helper return path: Mac `0x8567a–0x856a6` calls `Scr_GetVariableField`, whose permitted error return is zero at `0x8ba05–0x8ba10`. It does not perform dummy-object cleanup and is unchanged.

This correction is source/binary-reviewed, **uncompiled and unexercised**. No builds, tests, replay, CI or Actions ran, and no tests were added. Source audits alone confirm `COD2_X64=OFF` selected text is byte-identical to `dc087a73`, all five inactive configurations preserve their preprocessed bodies and actual diagnostic line expansions, and `git diff --check` passes. The legacy helper call remains in `#else`. Private exact-symbol bodies and the focused reachability report are in `recon-fast-script.noindex` and the orchestrator's `units/fast-script-reachability.last.md`.

## Returned debugger-local error position — 2026-11-05

Opcode `0x84` reads a U16 local name and advances to opcode+3 at Mac `0x8555a–0x85560`, then stores `FindVariable(localId,name)` at `0x85572`. If the lookup fails and evaluation or script loading permits the error policy to return, `0x858ea–0x858f1` jumps directly to `0x84268`. That block advances position by one more byte before fetching. Native previously resumed at opcode+3; its native-only missing-local branch now increments `pos` immediately after the shared error helper returns, matching Mac opcode+4. Successful lookup still advances only three bytes. Nonlocal error exits occur before the new increment.

Raw little-endian table bytes were decoded from the primary Mac binary: main table `0x3297bc` maps opcodes `0x21/0x22/0x23/0x84` to `0x85669/0x8580e/0x84675/0x8555a`; pre-error table `0x329b70` maps all four to `0x86663`; post-error table `0x3299d8` maps all four to `0x86ab5`. The latter block clears `tempVariable`, loads its ID into `fieldValueId` and releases/pops the current index through `0x84593/0x842e4`. These table entries do not establish an executed dummy-field path. Opcodes `0x21–0x23` call external `Scr_EvalArrayRef`/`Scr_EvalArrayIndex`; returned errors continue directly through their ordinary provider-result store/pop, while normal longjmp clears the saved opcode. Opcode `0x84` uses the direct position-advance path above. `GetDummyFieldValue` remains unchecked; no dummy substitution, RuntimeError call, error clearing, table dispatch or checkpoint behavior was changed.

The parent's flag-lifecycle audit found no current stock or CoD2x interpreter entry during evaluation/script loading, but the entry wrappers do not enforce an exclusion. This narrow correction preserves the proven supplied-state behavior. Prior build, fixture and replay evidence in this report applies to earlier source and does **not** cover this latest source change. The correction is source/binary-reviewed, **uncompiled and unexercised**. No builds, tests, CI, replays or Actions ran; no tests were added. Source-only checks preserve OFF selected bytes and all five inactive preprocessed bodies with actual diagnostic line expansions against `c3780ce9`; `git diff --check` passes. Durable commands/results are recorded in the private `units/fast-dummy-field-verification.json` receipt.
