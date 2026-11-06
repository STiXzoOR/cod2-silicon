# F1 builtin and entity-field error recovery

Baseline: `6c6edd0`. Native-only correction in `src/PC/script/scr_vm.c`.
No changes to notify matching, imports, dvar lifetime or transport behavior.

The native interpreter resumed after a builtin's `longjmp` without releasing
its arguments or producing the undefined result expected after a failed call.
Its modified automatic variables also had indeterminate values under C's
`setjmp` rules. A real compiler/VM fixture reproduced incorrect returns for
builtin functions and entity methods; repeated invalid method receivers caused
an ASan BUS in `VM_PushInt`. This explains a route to the earlier packet-send
crash without blaming the dvar pool: the `showpackets` global follows the VM
stack in the linked BSS, and its corrupted high word was a script integer type.
The exact live write to that global has not been captured with a watchpoint.

The fix saves the live call state in a volatile checkpoint before a builtin
or entity-field write opcode. On recovery it restores the interpreter state, calls the existing
`RuntimeError`, clears its error fields, releases outstanding parameters and partially constructed return values, then
returns undefined for calls. Field writes consume the assignment value without
adding a result. Increment/decrement recovery also releases the temporary field
value and skips the opcode’s trailing byte. It adds no error frame and retains the existing error-index,
developer, terminal-error and abort policies. Nested script callbacks continue
to use their existing recovery frames.

Mac evidence: `__Z10VM_Execute16function_stack_t` dispatches recovery through a
table at `0x3299d8`. Its builtin opcodes `0x3e` through `0x4b` all select
`0x86994`; `0x869e5–0x86a14` release outstanding parameters, and `0x8699e`
selects the slot after the public stack top, then `0x866ee` sets undefined.
This establishes the cleanup operation. It does **not** establish exact Mac
behavior after an external builtin longjmp: the actual setjmp-return path
`0x8622f` clears the opcode register at `0x86244`, so that path appears to skip
the recovery table. The Linux 1.3 source expresses the intended opcode recovery
but is read-only and was not copied. This change is therefore a native C
portability/error-recovery correction, not a claim that the Mac's compiled
longjmp path was identical. Other opcode error recovery remains outside scope except the entity-field write
paths made reachable by restoring read-only callbacks in this wave.

`tests/lp64/script/f1_error_recovery.py` reuses the existing production
compiler/VM host fixture. Thirteen ASan/UBSan cases exercise fixed-count and generic
six-argument builtin functions and valid entity methods, an invalid method
receiver, and a nested callback, including builtins that construct an array before
reporting an error. Four field cases link the production `g_spawn_mp.c` table,
registration, read-only callbacks and generic field access through the actual
compiler/VM. They repeatedly reject `spawnflags`, `classname` and `model`
assignments, plus `+=`, `++` and `--`, through local entity and `self` access.
Unrelated transform/client/HUD callbacks are asserting host stubs. Repeated
calls use strings and arrays. Checks
cover undefined results, a later successful callback, unchanged allocated
variable count, bounded stack, and restored parameter/frame/error counters.
The entity fixture holds an engine reference so legitimate entity destruction
is not mistaken for a leak.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/F1-error-recovery.noindex/`.
The `baseline/` directory uses a private `git show 6c6edd0:src/PC/script/scr_vm.c`
copy and fails the original six cases; `green/` now passes all thirteen with
CoD2x flags and `stock/` passes all thirteen with stock flags. The baseline invalid
receiver fails with ASan BUS in `VM_PushInt`; the other cases detect a defined
result instead of undefined. No proprietary source or game data enters Git.

Reproduce the candidate check:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/script/f1_error_recovery.py --compile-db build-macos-codx/compile_commands.engine.json --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-error-recovery.noindex/check"
```

For a private baseline comparison, the same runner accepts `--vm-source PATH`.
`--reuse-objects` reuses its previously generated host adapters and other script
objects, but always recompiles the selected VM and the new fixture.

`unifdef -UCOD2_X64` output matches the baseline source byte-for-byte;
`git diff --check` passes. The Linux i386 binary comparison remains unavailable.
The focused fixture's RuntimeError observer returns normally; the existing
StopGameType host fixture separately exercises the production abort/drop path.
The existing `f1_runtime.py` suite passes all 20 cases against CoD2x flags,
including generic errors, excessive parameters, notify/wait matching, thread
churn and nested calls. Several existing error cases deliberately intercept
RuntimeError, so their pass does not establish general opcode recovery.

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/script/f1_runtime.py --compile-db build-macos-codx/compile_commands.engine.json --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-error-recovery.noindex/runtime"
```

Review follow-up found two gaps in the initial six-case correction. A builtin
that creates an array before reporting an error has already consumed its
arguments; recovery must release `inparamcount` results too. The new partial
function and method cases crashed under ASan before that correction. Restored
read-only entity callbacks could report errors outside the builtin checkpoint;
the generic field assignment case accumulated stack values, and compound
assignments subsequently evaluated undefined. All thirteen cases pass on both
stock and CoD2x after the correction. The nested partial-result and direct
`self` assignment cases also passed the initial correction; they are retained
as characterization, not claimed as red regressions.

Root’s live verification and complete client/dedicated builds are recorded in
the shared-wave integration report. These tests do not establish complete
recovery for every script opcode.
