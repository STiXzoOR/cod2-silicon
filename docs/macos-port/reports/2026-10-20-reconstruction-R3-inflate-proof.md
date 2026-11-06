# R3 remaining inflate instruction proof

The parent accepted a qualified, unapplied proposal for the remaining rooted
instruction gaps in the original Mac multiplayer inflate routines. It would
create 80 records covering 279 bytes, replace eight unowned full records covering
19 bytes, and recover 260 unknown bytes. Four unrooted NOP bytes remain unresolved.
Accepted generation 11 is unchanged at 11,826 unknown bytes.

Two independent decoders agree on 2,176 records across six code segments around
three retained switch tables. Original named entries, direct branches,
fallthrough, qualified returning-call continuations and ordered table edges
provide explicit paths to every proposed instruction. Original tables, 34 ordered
slots and three defaults remain unchanged.

All 286 incoming references have raw provenance. The 14 DATA references comprise
ten string-index fields and four genuine debug-address fields; all remain.
In particular, the DATA reference into a proposed instruction is preserved at
its original byte address, without retargeting it to an instruction start.

Exactly one outgoing READ reference is proposed for removal. Its apparent
absolute load starts inside an original instruction and consumes bytes from the
following instructions. The proposal names the complete old record, its raw
operand and this exact reference; it permits no broader reference deletion.
The preservation obligation for incoming DATA is separate.

All 8,115 function bodies were checked for ownership collisions. The eight old
records are unowned, and the proposed body unions retain existing owners.
Ten original-region transitions account for the 260 bytes while retaining all
4,621 region identities and prior history. Twenty direct-control requirements
and one unresolved computed callback are explicit; no concrete callback target
is invented.

The parent replay passed 40 original-entry cases covering all ordered slots and
two out-of-range cases per switch, plus 45 initialized block cases covering all
80 candidate instructions and both outcomes at eight branch sites. Seven
semantic corruption tests rejected. Allocator return projections and initialized
local states are explicit model inputs, not real allocation or whole-function
equivalence evidence.

The parent verified 298 frozen output files and 95 input identities, independently
checked the replacement byte partition and DATA origin, and replayed all semantic
outputs. Eight consumed generation 11 metadata files match its accepted manifest.
The first parent replay stopped at a stale worker Git-head check after two
accepted report commits. A pinned external adapter verified exactly those
report-only additions; the fresh characterization differs only in recorded HEAD.
The failed run and original frozen scripts remain intact.

Private evidence remains outside Git. Worker seal SHA256:
`7c8a5121c9f8f15d4d56942416bd1a0b5b53ebd79406497c4506401e9bab098c`.
Parent acceptance SHA256:
`a4a1a2b63cb2f6bfd6b65172f6785519d34ce71eb6d07e9aab8727b89eb56205`.

No native code changed, so native suites were not rerun for this report.
Combined preflight, application and resulting metadata/pseudocode review remain
separate steps. R3–R6 and full port certification remain open.
