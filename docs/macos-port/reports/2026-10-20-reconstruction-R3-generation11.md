# R3 generation-11 decompilation checkpoint

Generation 11 applies the accepted
[preflight contract](2026-10-20-reconstruction-R3-generation11-preflight.md) to a
fresh private copy of generation 10. It recovers 907 previously unknown
executable bytes and preserves all 8,115 function identities and 4,621 original
region identities. Full reconstruction and native runtime equivalence remain
incomplete; Metal work remains paused.

The exact application creates 250 instruction records covering 913 bytes,
replacing five exact records covering six already-classified bytes. Thirteen
body unions add 913 bytes. The resulting listing has 618,916 instructions and
11,826 unknown executable bytes across 4,469 physical intervals. No original
denominator entries or bytes are removed, and no padding is classified.

All 618,666 retained full instruction records compare exactly. All 389,635
existing complete references, including 108,649 DATA references, survive.
The 89 control-reference requirements and 15 instruction-local non-returning
requirements hold without changing global callee flags. Parent review separately
accepts six automatic memory-read references and 35 generated label changes,
traced to original operands, decoded boundaries and preserved DATA references.
Post-export effects match the accepted effect object exactly.

The full read-only export contains 7,481 internal C records and 634 external
records. Every internal C and assembly/control-flow/reference hash is checked.
All 492 fresh impact probes match their full exports, including all 463 inherited
probes; none fails, times out or remains limited. One C output changes and
7,480 remain byte-identical. The changed output displays an original immediate
as the integer 12000 instead of a synthetic address expression. Its stored
instructions, references, discovery evidence and recovered control-flow records
are unchanged. The differing high-level intermediate-operation text is retained
in full; no normalization or generated-C equivalence is claimed.

All ten original ordered switch contracts, 81 slots and ten defaults survive.
Nine recovered decompiler mappings retain 75 slots and nine defaults; the existing
JPEG mapping limitation remains unresolved. The C-comment warning census is
unchanged: 1,261 functions and 1,621 warning lines, including 175 functions with
unrecovered-jump-table warnings. All 2,873 owned computed-flow records remain;
2,692 still have no recorded target. Comparable probe scopes retain 49
unknown-byte decode sites and three competing boundaries. The 29 added probe
functions expose ten additional uses of stored instructions outside their
assigned bodies; these remain diagnostic.

Parent saved-state, export, impact and ledger checks passed. Separate worker and
parent fresh read-only replays each reproduced all 7,499 discovery and metadata
files. The final serial integrity scan verified 33,701 frozen evidence items,
232,873 inputs and all nine project files without changing the sealed snapshot.

Independent checks decode all 250 proposed instructions with Capstone and the
explicitly relocated, content-identical LLVM decoder. Eighteen semantic
corruptions are rejected. The original preflight and its qualifications remain
immutable. An initially overstrict reference-source check is retained with its
correction: automatic correct references remain DEFAULT; the specified fallback
source applies only when an explicit addition is needed.

Missing exception-metadata associations, finite modeled execution domains,
private-frame and callee-memory assumptions, split decompiler output, unresolved
computed calls and unrooted NOPs remain qualified. Existing source proofs are
pinned evidence, not newly executed original-game scenarios. All owned binaries,
decompiled output, projects, traces and detailed proofs remain private outside
Git. Native runtime code is unchanged; verification for this checkpoint targets
the private reconstruction artifacts.
