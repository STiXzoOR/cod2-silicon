# R3 display and initializer cleanup conflicts

Parent verification accepts two qualified, unapplied cleanup proposals. They
create 110 instruction records covering 405 bytes, replace three malformed
records covering 11 bytes and replace one four-byte data listing. The prospective
unknown-byte recovery is 390. Accepted generation 11 remains unchanged at
11,826 unknown bytes.

Original named extents, FDE records and ten ordered LSDA roots support the two
cleanup graphs. Capstone and LLVM agree on 651 instructions across both complete
extents, plus the three alternate old boundaries and the call overlapping the
data item. Existing control references and fallthrough records introduce no
external roots into these graphs; possible runtime computed targets remain
outside that conclusion.

Two primary bodies gain 457 and 19 previously unowned bytes, including existing
unowned instructions. These gains are distinct from the 405 newly decoded bytes.
All 8,115 function and 4,621 original-region identities survive. Existing
fragment bodies remain exact, and all 100,541 symbols and original histories
remain represented.

A fresh parent query opened a separate copy of the nine accepted project files
read-only, with analysis disabled. Its three outputs match the worker query
byte for byte, including the global surface of 169,286 data listings. All nine
source and copy files remain unchanged. The repeated query uses the same
reviewed script; it is not a second query implementation.

The displaced generic pointer listing lies inside an original call operand.
Its exact type, settings, components, comments, context and symbol are frozen.
The incoming DATA reference and the outgoing DATA reference from that byte
address must survive even when the address becomes an instruction interior.
The original label identity must also survive. A new call reference cannot
substitute for any of these records.

All 389,717 original references, including 108,649 DATA records, are preserved.
The proposal adds 31 direct-control references and two absolute-memory READ
references. The parent independently checked the exact byte partition, all
function bodies, complete reference preservation, raw string-index origins and
the single data-listing removal.

Fresh semantic replay reproduces all 29 files and passes 2,596 finite cases plus
16 corruption tests. The original instructions execute in Unicorn and agree
with a separate concrete interpreter on traces, stack writes, call arguments,
registers and outgoing exception tokens. Every cleanup instruction and local
edge is exercised. Frames, exception transfer and ordinary imported services
are supplied models; actual unwinding, library heap effects, unrestricted
aliases and whole-function/native equivalence remain unproved.

Private worker seal SHA256:
`85028dc4a784f8c9cc3efc657991a563ea70ba3409d681775a53b0730357f272`.
Parent acceptance SHA256:
`44b4347f20dc9faea8a74ec2f3e0e4cfdc747ba0c620b73ca8559d68e88f0331`.

No native code changed; native suites were not rerun for this report. Future
preflight and application must verify actual instruction metadata and preserve
the interior label and references. This proposal is excluded from generation
12's already accepted preflight. Full reconstruction remains incomplete.
