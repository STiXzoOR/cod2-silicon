# R3 generation-11 application preflight

Parent verification accepts the exact generation-11 preflight contract rebased
against accepted generation 10. The contract remains unapplied. No Ghidra
project, native code or runtime behavior changes in this checkpoint. Full
reconstruction remains incomplete and Metal work remains paused.

The four previously accepted multiowner, helper-tail, orphan-spill and
additive-cleanup proposals create 250 instruction records covering 913 bytes,
replacing five complete old records covering six bytes. The prospective
recovery is 907 previously unknown bytes; the accepted baseline still has
12,733 unknown executable bytes. Thirteen exact body unions, 89 direct-control
requirements (13 already present, 76 missing) and 15 local call-flow requirements
retain the original identities and global callee flags.

Characterization examines all 618,671 complete baseline instruction records,
including every stored field, and binds the 618,666 retained records by digest.
All 389,635 complete existing references, including 108,649 DATA references,
must survive application. All 8,115 function and 4,621 original region identities
and their historical denominators remain represented. Source proposals and
their exact qualifications are preserved; their accepted token proofs were
pinned, not rerun as part of this preflight.

The fresh parent replay checks all 250 proposed instructions using Capstone and
LLVM. Eighteen altered semantic contracts are rejected after file-hash checks,
covering bytes, boundaries, ownership, direct targets, identities, flow rules,
reference retention and weakened qualifications. Fresh characterization
reproduces seven artifacts byte for byte. The eighth, its input manifest,
differs only in one decoder executable path, with size and hash unchanged.
Characterization reuses the source algorithm; it is not an independent second
token proof.

The original integrity verifier rejected a changed Xcode decoder executable.
That failure is preserved. A separately installed decoder matches the original
18,133,888-byte executable and its complete SHA-256 exactly. Parent replay
tooling explicitly substitutes that path without changing the sealed worker
proof or the system installation. All 119 frozen outputs and 7,594 input
contents pass the resulting integrity check, including a final post-replay
check. The parent tooling changes and exact path substitution are recorded
privately; the unmodified verifier at the original path is not reported as
passing.

The helper-tail runtime unwind association remains unresolved. Private-frame
alias and callee-memory assumptions and multiowner split-pseudocode limitations
remain qualified. Existing computed calls, three withheld units and unrooted
bytes remain untouched. This contract creates no unwind metadata or invented
indirect-call targets. Application effects receive no advance approval: a future
application still requires exact metadata, full export and impact checks, plus
separate review of any incidental effects.

No project was opened or hashed during this preflight. The accepted historical
generation-10 audit is pinned rather than repeated in full. No native test suite
was rerun for this report-only checkpoint; its checks concern private metadata
and original instruction bytes, not native runtime equivalence. All owned
binaries, decompiled output and detailed proof evidence remain private outside
Git.
