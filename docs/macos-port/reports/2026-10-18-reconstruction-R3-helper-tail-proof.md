# R3 helper-tail structural proof checkpoint

Two helper-function tails have a qualified private structural proposal covering
137 instructions and 533 previously unknown executable bytes. This proposal is
unapplied. It neither completes reconstruction nor establishes native runtime
equivalence. Metal work remains paused.

All 695 instruction boundaries across the two complete named extents agree
between Capstone and LLVM. Proposed body unions match the original named
extents and replace no existing instructions. The contract contains 52 direct
reference postconditions and nine conditional local non-returning call
operations, without changing global callee flags.

The two candidate exception tables have unique structural matches within the
explicitly tested format and named-extent population. However, neither function
has a matching original frame-description entry, and no original entry
references either candidate table. **The runtime unwind association remains
unresolved.** The proposal creates no unwind metadata or exception references.

The caller-frame audit propagates exact stack positions through all instructions
and checks every prologue, epilogue, protected call and landing root. It derives
outgoing argument regions from actual operands and checks all 201 known-frame
memory intervals for width and overlap. Outgoing argument bytes become unknown
after calls until explicitly overwritten. This includes indirect accesses
through frame-derived register aliases.

Twenty-three deliberately corrupted byte variants fail the intended structural,
exception-token, catch-depth, stack-frame or memory checks. Static branch
coverage remains conditional on the recorded ABI, caller/callee memory and
hypothetical exception-transfer assumptions. It does not prove actual exception
execution, loop termination or arbitrary callee behavior.

The parent freshly replayed all 46 core evidence files and five frame-audit
files with exact equality, reran all 23 negative controls, and checked the
7,528 frozen inputs and 468 frozen outputs. All checks passed. The parent
accepted the qualified structural proposal on 2026-10-18.

This evidence remains separate from the already accepted four-unit generation-10
application contract. All owned binaries, decompiled C, detailed proofs and
traces remain private outside Git.
