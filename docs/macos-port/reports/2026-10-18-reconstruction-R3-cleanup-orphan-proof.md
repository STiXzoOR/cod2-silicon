# R3 cleanup and orphan-spill proof checkpoint

Two private cleanup proposals passed parent source review and fresh replay.
They remain unapplied and conditional on their recorded ABI and memory
assumptions. Full reconstruction and native runtime equivalence remain
incomplete. Metal work remains paused while decompilation proceeds.

The first proposal supports six additive cleanup units: 51 instructions and
179 previously unknown bytes, eight direct-control reference requirements,
and four local non-returning call requirements. All 646 original instruction
boundaries across seven inspected extents agree between Capstone and LLVM.
The whole control-flow model reaches 643 instructions and checks caller-frame,
known-frame alias, outgoing argument and active exception-token behavior.
A conflicting seventh unit retains all 231 unknown bytes; three unrooted
bytes remain unknown rather than being classified as padding.
All 32 deliberate byte corruptions fail their intended semantic assertions.

The second proposal supports one orphaned cleanup: 29 instructions covering
111 bytes replace exactly five detached records covering six bytes, supporting
105 previously unknown bytes. Its contract preserves all existing references
and requires nine direct-control postconditions. A separately implemented raw
binary reader and machine-path checker verifies 99 full-extent instructions,
256 path states, the private exception-token spill, caller-frame restoration
and outgoing-argument opacity. Both checkers reject all 11 deliberate byte
corruptions. The prospective preservation audit checks 618,340 untouched full
instruction records, all 389,501 references and all 100,481 symbol records.

Parent replay reproduced all 119 authoritative evidence files for the first
proposal and all 26 characterization/proof files for the second. The second
replay also reran its independent checker and complete metadata-preservation
audit. Frozen input and output checks passed. Both proposals preserve all
8,115 function identities and 4,621 original region identities.

These are original-byte structural proofs, not execution of the game's
exception runtime. Unknown-pointer aliasing, arbitrary callee memory behavior,
actual unwinding and native equivalence remain qualified or unproved.
Application effects, changed decompiled output and metadata preservation must
be checked again when the proposals are applied to an accepted project copy.
Owned binaries, decompiled C and detailed proof evidence remain private
outside Git.
