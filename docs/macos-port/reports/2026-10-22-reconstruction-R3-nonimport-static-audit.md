# R3 non-import unknown-byte audit

The static audit accounts for all 7,850 non-import unknown bytes without
certifying any as padding or unreachable code. The accepted generation remains
15, with 10,384 unknown bytes. The separate 2,534 import bytes remain covered by
the [import-slot reconciliation](2026-10-22-reconstruction-R3-import-slots.md);
its proposed classifications have not been applied.

## Measured coverage

The packet retains 3,800 physical ranges and all 4,621 historical region IDs,
including 821 rows with no non-import intersection. Historical rows account for
7,848 bytes; two supplemental bytes remain explicit. Exact original load
commands map 3,109 zero bytes outside sections and 4,741 bytes inside instruction
sections. Byte values, section membership and absent recorded references do not
establish global unreachability.

All 727 incoming DATA references were retained: 698 instruction sources and 29
data sources. Raw operand evidence separates immediate literals, comparisons,
based-memory displacements, a bitwise immediate, analyzer inferences and raw
data words. Six consecutive immediate-store/direct-call sequences have
SDK-qualified `glGetString` selector use. This does not establish original
library behavior, remove references or close their target bytes. The other 721
references retain their stated ambiguity.

The packet also retains 3,404 aligned or unaligned raw 32-bit matches, 7,587
boundary-side instructions and 11,771 selected instructions with agreeing
original bytes, Capstone and LLVM decoding. Raw-word matches are candidate
patterns, not certified pointers. Original symbol and unwind metadata retain
7,063 N_FUN records, 353 CIEs and 4,640 FDEs. The accepted reference/call census
contains no outgoing references or calls touching the non-import unknown set;
that census does not exhaust runtime or indirect entry.

## Verification and limits

The parent independently reconstructed byte/history partitions, the reference
census, all raw-word windows and boundary instructions from the accepted ledger
and original file. It checked selected instruction/data bytes, raw symbol/unwind
records and the six direct GL argument sequences, then separately bound those
sequences to the accepted import map and pinned SDK declarations. All 12
controls were rerun: the unchanged baseline passed and 11 intended rejection
cases failed as expected, including the deliberately ineffective mutation.
The bounded artifact verifier also passed. No duplicate whole-source campaign
was needed for this adjudication.

The worker's first attempt failed because it assumed every DATA reference source
was an instruction. The corrected attempt explicitly retained the 29 data
sources. That failed attempt, source versions and partial outputs remain private.
No imported nonreturn contract was assumed. No canonical behavior, tests,
accepted generation, ledger or denominator changed. These are accepted static
evidence and explicit unresolved classifications, not a closure milestone.

The private worker seal is
`0de81d548139b0bab483a106c6a5dfbe22f7749381ade15a1668cf12da457dc0`;
the independent parent audit seal is
`ccaa07fdc249209e6545ae08f3d47be852ae0ef5b45c802095579726f1f6e973`.
Original bytes and complete derived tables remain private. R3 and the full
validated reconstruction remain incomplete.
