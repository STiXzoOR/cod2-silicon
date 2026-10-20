# R3 generation-12 decompilation checkpoint

Generation 12 applies the accepted initializer and inflate contract to a fresh
private copy of generation 11. It recovers 491 previously unknown executable
bytes while preserving all 8,115 function identities and 4,621 original region
identities. Full reconstruction and native runtime equivalence remain incomplete.

The application creates 156 instruction records covering 516 bytes and replaces
11 exact records covering 25 already-classified bytes. Three owner body unions
add 516 bytes. The resulting listing has 619,061 instructions and 11,335 unknown
executable bytes across 4,453 physical intervals. No denominator entries or bytes
are removed, and four excluded unknown NOPs remain unknown.

All 618,905 untouched full instruction records and 108,649 DATA references
survive exactly. Only the previously proved erroneous READ reference is removed.
All 43 required direct references survive; parent review separately accepts one
automatic READ and 29 generated label effects, retaining original symbol identity
and DATA targets where labels change their display. The total is 389,760
references. The one local CALL_RETURN retains original CALL operations with only
the expected RETURN override; global callee flags and context remain unchanged.
The allocator callback remains targetless.

Complete export retains 7,481 internal C records and 634 external records.
Sixteen diagnostic C outputs change and 7,465 are byte-identical. Parent review
traces these changes to immediate values and generated labels, including one
merged-variable rerender, with 19 original operand/reference witnesses. The
16 functions retain exact stored instructions and recovered control-flow files.
Their differing high-level operation text is preserved; generated-C or whole
function equivalence is not certified. The three repaired owners have unchanged
C output.

All 499 impact probes match their complete exports, retaining all 492 prior
probes. All ten original ordered switch contracts, 81 slots and ten defaults
survive. Nine recovered decompiler mappings retain 75 slots and nine defaults;
the existing JPEG mapping remains unresolved. In the same inherited 492-function
scope, unknown-byte high-level decode sites fall from 49 to zero and competing
boundaries from three to zero. The 459 stored outside-body sites remain a scoped
diagnostic. The warning census is unchanged at 1,261 functions and 1,621 lines,
including 175 functions with unrecovered-jump-table warnings.

Parent saved-state, export, impact and ledger checks passed, including ten
semantic negative controls. Separate worker and parent fresh read-only replays
each reproduced all 8,115 export statuses, 7,481 C hashes, 7,499 discovery and
metadata files, and 2,495 probe files. All seven fresh semantic verifiers passed.
Only measured decompiler elapsed seconds are normalized. All nine source and
copy project files remain byte-identical. The final integrity scan verifies
52,601 frozen evidence items and 28,129 targeted inputs. A failed attempt to
open OS-read-only disposable project files is preserved; the working replay
makes only its private copy writable and verifies exact byte preservation.
Parent acceptance receipt SHA-256: `6cccf8d9c0f79f3cb0a73f1a3f09cb8a5b4c65dd27ed558441a5ba01ea190d2c`.

All original binaries, decompiler output, projects, detailed proofs and traces
remain private. Finite modeled imports, ABI/frame domains, unwinder assumptions,
unresolved computed targets and remaining unknown bytes retain their limitations.
The source proofs are pinned evidence, not newly executed original-game scenarios.
Native runtime code is unchanged; this checkpoint verifies private reconstruction
artifacts and does not establish completion of R3–R6 or Metal certification.
