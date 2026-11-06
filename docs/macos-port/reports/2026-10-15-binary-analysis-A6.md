# A6 — comprehensive disposition audit

2026-10-15. The declared offline analysis pass is delivered for three owned Mac
i386 binaries: MP 1.3 STABS reference, Steam MP and Steam SP. This is complete
inventory/export accounting, not complete validated reconstruction of CoD2.
Other platforms and unavailable variants remain excluded in the A0 inventory.

| Target | Inventory records | Decompiled bodies | No decoded body | External imports | Unique source candidates |
| --- | ---: | ---: | ---: | ---: | ---: |
| MP reference | 8,111 | 7,477 | 0 | 634 | 4,861 |
| Steam MP | 8,653 | 7,950 | 1 | 702 | 4,690 |
| Steam SP | 7,971 | 7,350 | 1 | 620 | 3,972 |
| Total | 24,735 | 22,777 | 2 | 1,956 | 13,523 |

The private final ledger joins every inventory record to export outcome,
source correspondence, warning classification and verification limitations.
Inventory, export and mapping key sets match exactly; there are no silently
omitted records. JSONL and SQLite forms are retained. No whole-function semantic
equivalence is inferred from a symbol match or a passing domain fixture.

A2's executable accounting remains authoritative: 7,157,267 instruction-section
bytes, 6,754,314 decoded instruction bytes, 401,761 unknown bytes and 1,192 data
bytes. All 48,301 Rizin function candidates have recorded dispositions; 5,045
reference candidates remain unresolved. Decoded instructions outside function
bodies and indirect targets remain explicit in A2 evidence. No runtime target
collection occurred.

The additional gap audit classifies every previously unknown executable-range
interval by byte pattern, retaining each hash. Zero/NOP-like patterns are only
padding candidates; none is certified unreachable and none is removed from the
unknown denominator. These executable-range totals include section padding and
therefore differ from the instruction-section denominator above.

Comment-aware pseudocode triage found 3,266 records with actual decompiler warning
comments, including 1,652 with unresolved-control-flow warnings. Categories can
overlap. The two no-body records also retain bad-instruction/flow diagnostics.
A3's raw warning-marker count included game diagnostic strings; this refined
measurement does not overwrite its historical receipts.

Cross-variant comparison contains 10,693 named symbol groups: 7,461 occur in
multiple targets and 4,542 in all three. There are 681 multi-target groups with
identical instruction hashes, and 16 default-named records tracked separately.
Neither matching symbols nor instruction bytes establish semantic equivalence
across different imports, global state and address layouts.

Remaining reconstruction gaps include 2,002 STABS parse errors, opaque Direct3D
class layouts, 7,301 records without an active non-static source-definition match,
unknown executable bytes, indirect control flow and unvalidated native lifetime
behavior. Source matching excludes static definitions and the port targets MP;
unmatched records do not directly count missing implementation. A5 documents
passing offline checks and the pending live checks. Metal integration itself
has not advanced beyond U1's recovery and integration map.

## Integrity and privacy

All 92,915 files in the frozen A3 manifest were rehashed with zero mismatches.
Both originals and private copies of all three inputs still match their A0
hashes. Runtime source-tree object remains
`4ec6695155930f24b78a31fb389755e9085cd6f8`.

The separate A6 manifest binds 905 analysis/receipt files (1,156,041,876 bytes),
including joined ledgers, types, correspondence and test metadata. Compiled and
generated build artifacts and AST caches are excluded; build JSON/text/log
metadata is retained. Its SHA-256 is
`f2024b399172717ba7be62a315e895809a627a625d0d8504dc6041bc1a4b4d49`.
The external workspace root remains owner-only (`0700`). No binaries,
pseudocode, private logs or symbol/type dumps were staged in Git.

## Next implementation decision

Resume [the U1 integration map](../metal-integration-03.md) at U2a: establish the
opt-in Metal build boundary and a deployment-compatible SDL dependency, then
U2b/U2c backend dispatch and resource lifecycle. Keep existing GL behavior as the
baseline. Apply the private references to each renderer slice and validate live
retirement, map transitions and screenshot parity as those paths become usable.
Address relevant control-flow/type gaps when they block a concrete slice;
exhaustive unrelated SP/library reconstruction need not delay the MP renderer.
