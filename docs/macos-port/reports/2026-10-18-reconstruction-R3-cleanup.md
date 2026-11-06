# R3 — Compression and exception-cleanup discovery

Status: this bounded MP discovery checkpoint is verified. Full decompilation and
validated reconstruction remain incomplete. Metal remains paused. This follows
the [compiler-helper checkpoint](2026-10-18-reconstruction-R3-dispatch.md).

## Discovery changes

Generation 07 creates 12,234 exact instruction records and replaces two proved
orphan instruction records. The net gain is 12,232 records and 38,134 decoded
bytes, with another 80 bytes identified as dispatch tables. Four function bodies
gain only the supported code. All 8,115 inventory entries remain; original binary
bytes, signatures and the existing compiler-helper model are unchanged.

| MP discovery measure | Generation 06 | Generation 07 |
| --- | ---: | ---: |
| Decoded instructions | 601,538 | 613,770 |
| Unknown executable bytes | 68,148 | 29,934 |
| Unknown instruction-section bytes | 65,039 | 26,825 |
| Physical unknown intervals | 4,618 | 4,630 |
| Owned computed sites without static targets | 2,688 | 2,686 |

The 38,214 newly classified bytes retain their original ledger identities.
Partial recovery splits some gaps, so the interval count increases despite the
byte reduction. Four additional computed sites remain unowned. No padding
classification or denominator removal contributes to these results.

## Evidence and its limits

Compression recovery reproduced 52 bounded original-instruction cases, including
evidence for both instruction-boundary corrections. Five JPEG cases reproduced
the initialized default error-callback path. A separate five-byte continuation
has static ownership evidence but was not executed. Observed callback targets do
not prove an exhaustive runtime target set.

Exception-cleanup discovery accounts for 35,558 bytes using original debug and
unwind metadata, 3,135 landing-pad roots, agreeing decoders and checked control
flow. The cleanup paths preserve the incoming exception argument under the
documented i386 calling-convention assumptions. This is static evidence; no
original exception execution or native exception equivalence was demonstrated.
Exactly 1,043 new call instructions receive local no-return analysis metadata
supported by their import identities and ABI contract. Global import flags and
existing instruction metadata remain unchanged.

Four automatically generated label displays change because their addresses now
fall within corrected instructions. One existing cleanup block splits at a
newly recovered branch target. Their identities, bytes and existing references
remain intact; the exact derived changes are recorded privately.

## Export and verification

Fresh export and independent parent checks pass all 8,115 records: 7,481
decompiled bodies and 634 external entries. All evidence and pseudocode hashes
pass, and the nine project files remain unchanged by read-only export. The
checks preserve 100,672 frozen input files and the original function/region
ledgers. A receipt-ordering failure was preserved and corrected without changing
any semantic assertion or analysis state.

All 72 diagnostic probes match the full export. Six supported switch mappings
agree with the original evidence: 57 ordered slots and six default paths. Only
the two compression bodies change their pseudocode; unchanged C output elsewhere
does not establish discovery completeness. Warning-function count remains 1,261.
The diagnostic scope still contains 458 intermediate-code sites in unknown bytes
and three unresolved competing instruction boundaries. These remain excluded
from accepted code and ownership.

Further JPEG paths, exception regions, indirect targets and source correspondence
remain open. All binaries, decompiled code, traces and detailed evidence stay
private and outside Git. This commit changes no production code or Metal
behavior; private original-byte, state, ledger and export checks replace runtime
tests for this documentation checkpoint. The remaining completion requirements
are in the [reconstruction plan](../../plans/2026-10-15-validated-reconstruction.md).
