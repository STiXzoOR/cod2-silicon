# VM_Execute recovery follow-up

Updated 2026-11-05. Assignment base: `1510933`. One owned row:
`mac:0x0008393a`, fixed with the boundaries below. The earlier
[interpreter audit](script-scr_vm.md) remains the evidence for the 135 ordinary
dispatch entries; this follow-up addresses its two remaining recovery gaps.

## Live state and nonlocal transfer

Exact-symbol Mac `__Z10VM_Execute16function_stack_t` keeps the five live frame
fields in its stack argument: position at EBP+8, local ID at +12, local count
at +16, top at +20 and start top at +24. Their latest stores remain available
when `setjmp` returns at `0x83962`. The branch at `0x83964` reaches
`0x8622f–0x86262`, which instead clears eight scratch slots, including field
ID, object ID and nested-thread count, and clears the opcode register ESI.
After `RuntimeError`, the unsigned comparison at `0x862af` bypasses the error
recovery table and fetches the next byte from the retained position. Recovery
must not dispatch the previous opcode's cleanup merely because a table entry
exists.

Native previously retained modified, nonvolatile automatic frame variables
across this jump, which gives indeterminate values under C's setjmp rules.
The five live fields are now volatile, including through every helper pointer
that updates them. There are no casts discarding volatile qualification. The
uncheckpointed recovery branch explicitly resets scratch state before reading
it again. The immutable `varPub` binding and input frame are unchanged after
setjmp; the result ID is assigned only on immediate-return success paths.
Opcode and case-local temporaries are freshly initialized after recovery or
are abandoned when their helper invocation is unwound.

`VM_ArchiveStack` remains unchanged. Its only output-ID store follows its last
call. The qualified caller therefore uses an ordinary temporary and commits
the returned ID only when archival returns; a transfer inside archival cannot
lose a partial output-ID update.

The active native entry graph is `VM_Resume` or the external execution wrapper,
then this interpreter. Script-call frames share its recovery slot; nested host
callbacks enter another interpreter and recovery slot. Error helpers and
parameter/object/terminal APIs converge on `Scr_SetErrorMessageAndJump`, which
owns the first-message, evaluate/loading, terminal and longjmp/drop decisions.
`RuntimeError` owns the later abort/drop decision. Those policies, public frame
and parameter counters, and error-level entry/exit balance are unchanged.
Evaluation during notify matching does not execute another interpreter; game
initialization ends script loading before executing structs and gametype code.
No optional debugger execution body was reconstructed by this work.

## Unknown opcodes

Mac `0x839e1 → 0x84227 → 0x83f8d` has a defined continuation while the public
frame table has capacity. It saves start top and its current type in the caller
frame, writes marker 8, increments nested-thread count, saves and clears the
local count, advances the public frame/count and stores the live local ID. It
then fetches the next byte without reading a call target. Native now performs
those same stores. It deliberately leaves the other global frame fields as
they were, as the binary does; it does not read indeterminate C storage.

Malformed streams that exhaust the declared 32-frame table retain the native
fatal diagnostic. Mac would advance beyond that storage. This is a precise
boundary, not a claim of parity for out-of-bounds execution. Arbitrary invalid
code pointers or bytecode outside its allocated buffer are likewise not
established as meaningful VM input.

## Preserved adaptations

The documented volatile builtin/setter checkpoint still restores its saved
state, consumes failed arguments/partial results and supplies undefined for a
failed call. Its cleanup is an intentional native recovery adaptation, not a
claim about Mac's zero-opcode longjmp path. The monotonic timeout service is
unchanged. The two dummy-lvalue/object helper identities, notify allocation
state and other unassigned rows retain their earlier verdicts.

## Verification

The final arm64 Release `cod2_macos_ded` target builds with nice 19 and two
jobs. Existing ASan/UBSan fixtures pass: `f1_runtime` (20 cases),
`f1_error_recovery` (13), `f1_vm` (7), `vm_semantics` (15), and the full-compiler
nested-array fixture. The existing production-error StopGameType host fixture
also passes as the runtime runner's prerequisite. No tests, cases or
assertions were added or changed; no full CI or replay ran.

The runtime fixture's generic boolean error and negative wait resume through
this interpreter; the builtin/setter fixture exercises its retained recovery
adaptation. Several other runtime error cases deliberately leave through the
observer and establish diagnostics before continuation only. The baseline's
20 runtime cases already passed, so these are characterization and regression
coverage, not a new red/green demonstration of the C storage issue. Scratch
reset after an error in a nested script thread and the new unknown-opcode
continuation are compiled and reviewed, not specifically exercised.

`unifdef -UCOD2_X64` output is byte-identical to `1510933`. Five inactive
preprocessor configurations retain identical bodies and actual diagnostic
line expansions. `git diff --check` passes. Hosted ILP32 binary-byte parity
was not built. Private evidence is in
`~/Library/Application Support/CoD2-native-decomp/recon-vm-recovery.noindex/`.

## Integrated batch 19 verification

Production f65aaad and the catalog/existing-fixture follow-ups through 3f45bb4 passed both dedicated builds, full local CI02–13, source guards, actual OFF line expansion checks including the C++ bridge and Objective-C owner, unit parent/independent reviews and independent combined-source review. Integrated production/build/fixture files exactly match the reviewed private composition. Both 2,000-frame dedicated replays exactly match both 819d282 baselines in players, state hashes and entities; all six Linux logic fields agree, maximum origin/velocity drift0.11792/2.0.

Existing raster/saved-screen checks and 18 depth cases passed ASan/UBSan. The depth fixture uses the actual typed vertex-shader owner and provider accessors, real borrowed-buffer AddRef/Release methods, existing outer depth defaults and actual fence/index/dispatch providers. Its fallback mode explicitly covers the approximation path, not shader-free native validation. GL cases and result assertions are unchanged. The catalog was generated twice identically: only the restored r_mode/r_displayRefresh registration sites change factual metadata; other changes are source provenance. Initial CI02 failed stale coverage and its complete retry passed after regeneration. The existing Swift loader then passed after updating its revision literal and display consensus expectation; no assertions/cases were added or removed. Fresh source/resource review confirms the preserved copy/restrictions and consumer contract. No tests, cases or assertions were added.

This batch closes 24 capability, defined-boundary, GPU budget, render-state, shader factory, mutex/Optimize exception, VM recovery, startup splash, indexed-draw and retired platform-boundary entries. Two formerly equal splash no-op lifecycle verdicts become intentional Windows-style native ownership. Indexed drawing restores GL context state and separate logical caches around the explicit approximation; extended formats use the original driver and independent color buffers. The inherited multi-color scratch alias found during fresh review is corrected. Licensed/source-shader approximation and bounded storage adaptations remain explicit; no visual-equivalence claim follows from whole-row intentional closure. Original fresh-context state defaults are explicitly restored where native Reset retains its context. Exception cleanup/output ordering, malformed opcodes and unreached registration/state paths remain compiled and reviewed where fixtures did not execute them. Existing builtin/setter VM checkpoints are an explicit native adaptation, not original zero-opcode longjmp equivalence. Actual startup visibility, sampled-texture journal entries, fallback-to-normal transitions, multiple converted colors and their reallocation/reset branches remain compiled/reviewed only. Dedicated replay does not exercise rendering; this is not a complete rendered-frame acceptance.
