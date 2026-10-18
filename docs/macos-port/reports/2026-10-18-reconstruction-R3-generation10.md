# R3 generation-10 decompilation checkpoint

Generation 10 applies the previously accepted four-unit cleanup contract to a
fresh private copy of generation 09. It recovers 1,088 previously unknown
executable bytes and preserves all 8,115 function identities and 4,621 original
region identities. Full reconstruction and native runtime equivalence remain
incomplete; Metal work remains paused.

The exact application creates 343 instruction records covering 1,130 bytes,
replacing 17 detached records covering 42 already-classified bytes. Thirty
body unions add 1,353 bytes, including 223 previously decoded but unowned bytes.
The resulting listing has 618,671 instructions and 12,733 unknown executable
bytes across 4,480 physical intervals. Original denominator history is retained.

All 618,328 untouched full instruction records compare exactly. All 108,649
DATA references are preserved. Only three specifically proved spurious control
references are removed; 153 required control-reference tuples are checked.
Eleven instruction-local non-returning overrides leave global callee flags
unchanged. Parent review separately accepts six automatically generated memory
references and 74 dynamic-label changes, each traced to exact original operands,
new code boundaries or the explicitly removed references. Post-export effects
must match the accepted effect object and its hash exactly.

The full read-only export contains 7,481 internal C records and 634 external
records. Every internal C and assembly/control-flow/reference hash is checked.
All 463 impact probes match their full exports, including all 427 inherited
probes; none fails, times out or remains limited. Twelve C outputs change and
7,469 remain byte-identical. Nine unexpected C effects required a separately
reviewed scope extension with fresh before-and-after probes.

All twelve changed functions retain identical stored full function metadata,
assembly, control-flow and references. Their disappearing synthetic address
bases coincide with newly recovered code; original operands and explicit
address derivations explain those constants. Decompiled output also changes
inferred types and temporary-variable reuse. High-level intermediate-operation
differences are confined to pointer arithmetic, integer addition and casts;
all twelve recovered control-flow records remain identical. These observations
do not certify generated-C or whole-function semantic equivalence.

All ten original ordered switch contracts, 81 slots and ten defaults are
retained. Nine native decompiler mappings retain 75 slots and nine defaults;
the existing JPEG mapping limitation remains unresolved. The C-comment warning
census is unchanged: 1,261 functions and 1,621 warning lines, including 175
functions with unrecovered-jump-table warnings. All 2,873 owned computed-flow
records remain; 2,692 still have no recorded target. No warning or unresolved
category was removed to improve the reported result. Comparable probe scopes
still retain 49 unknown-byte decode sites and three competing-boundary sites.

Parent verification passed the exact-state, export, impact and ledger checks.
A fresh read-only Ghidra replay reproduced all 7,499 evidence files. The final
serial integrity scan verified 41,112 frozen evidence items, 191,493 inputs and
all nine project files. An earlier concurrent integrity check failed while
Ghidra was open; that attempt is preserved, and the post-replay check passed
without changing any sealed artifacts.

The separately accepted multiowner, helper-tail, orphan-spill and six-cleanup
proposals are not applied here. Their next application needs exact metadata
preconditions checked against the accepted generation. All owned binaries,
decompiled output, projects, traces and detailed proofs remain private outside
Git. No native runtime behavior changes in this checkpoint.
