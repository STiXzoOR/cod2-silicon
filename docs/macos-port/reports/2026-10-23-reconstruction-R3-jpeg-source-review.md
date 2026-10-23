# R3: independent review of JPEG recovery changes

An independent static review found no evidenced actionable defect in the four
frozen decompiler source changes tested by the
[real-protocol preflight](2026-10-23-reconstruction-R3-jpeg-protocol.md).
The parent verified the complete sealed review and freshly checked 45 source
and evidence bindings. No source or installed tool changes in this slice.

The review traces snapshot ownership and destruction, capture before analysis,
SeqNum cloning and reset, original CFG reachability, address-space containment,
write barriers, both added guard-recognition paths and downstream range use.
It checks surrounding implementation rather than treating passing fixtures as
the source-level argument. The exact four candidate hashes match both the
console and protocol copies.

Retained console ablations demonstrate the erased-write failure, including
reintroduction of an incomplete table when either original-path check is removed.
The protocol counterexamples retain their writes in high IR and demonstrate
explicitly unresolved indirect sites. These remain distinct bounded proofs.
The reviewer inspected the existing typed-path and lifecycle cases and their
retained failures; no executable campaign was rerun by the reviewer.

Snapshot construction is bounded by O((V + E) log V), with O(V + E) storage, for
operations/block sentinels V and edges E. Each path query traverses O(V + E);
candidate/predecessor combinations multiply that work. No measured performance
regression was established, and no whole-corpus performance claim is made.
Broader testing should retain per-function timings and attempts.

Private evidence is under `reconstruction/phase-r3/`:

- Review: `20261023-jpeg-source-adoption-review`, inventory SHA-256
  `aef46de5dda72c318ca64d8da354e4a48654a36ca50691a2b16db446e0a53fcf`.
- Fresh integrity check: `20261023-jpeg-source-review-parent-integrity`,
  inventory SHA-256
  `1176b516c6cd18f0fffd408907543b5f58d5ca86b6f2fc7b0e94bc6d2cd18c11`.
- Parent command receipts: `20261023-eighteenth-wave-parent-verification`.

Initial reviewer reads and inline source text are retained in the tool
conversation; saved receipts contain their arguments, times, exits and complete
output. The final integrity checker is saved and replayable.

This independent review does not complete formal source-adoption review,
whole-generation candidate regression or installed-tool adoption. Universal
alias/architecture correctness and complete JPEG semantics remain unproved.
Native full suites were not repeated for this report-only change.
