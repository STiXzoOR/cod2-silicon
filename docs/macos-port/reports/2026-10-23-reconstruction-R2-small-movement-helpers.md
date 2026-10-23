# R2: four original movement helpers

Four small helpers match their original i386 instructions on 671 frozen cases
in stock and CODX revisions 0, 1, 2, 3, 4 and 6. Both the worker and fresh parent
campaign produce 4,697 exact native comparisons. No production code changes.

| Helper | Original extent | Instructions | Cases |
| --- | --- | ---: | ---: |
| `PM_GetEffectiveStance` | `0x6b6ce..0x6b6ee` | 14 | 41 |
| `PM_GroundSurfaceType` | `0x6b6ee..0x6b70a` | 13 | 224 |
| `PM_GetViewHeightLerpTime` | `0x6b70a..0x6b72f` | 16 | 246 |
| `PM_SetProneMovementOverride` | `0x6b730..0x6b745` | 10 | 160 |

Paired raw N_FUN and text symbols bind all 118 bytes; alignment gaps are
excluded. Independent parent decoding agrees with every instruction. All four
routines use cdecl stack arguments and call no imports. Absolute-zero `.eh`
symbols are not FDE evidence; the complete frame decoder finds no starting FDE.
Raw STABS and native probes establish their respective object layouts.

The corpus covers all 32 surface values with both suppression outcomes,
unrelated flag bits, stance targets and signed boundaries, zero and five nonzero
`bDown` representatives, and prone/override flag combinations. It reaches all
53 instructions and both successors of all six branches. These finite inputs
do not establish universal 32-bit equivalence or live caller reachability.

Original results execute the bound machine instructions. Native comparisons use
the whole production `bg_pmove.c` translation unit and its linked production
dependencies, actual Release compile commands, declared fixture options, and
ASan/UBSan. The tested callee-saved registers, aligned stack return, floating-point state,
guards and complete object images are checked. Native pointers are normalized
and separately verified. The void helper's return register is not compared.

The independent parent audit recomputes each helper's return and state effect,
checks complete object snapshots, and verifies all 2,444 original reads and
769 writes. Only the prone helper writes object state, at the player flags field;
the remaining writes save the original frame pointer on the stack. Native
arbitrary reads are not exhaustively traced.

Four executed original-byte mutations and four actual native source mutations
in rebuilt whole translation units produce the expected mismatches. Restored
and unchanged positives pass. A changed shift instruction on a zero input is
separately identified as ineffective; guard and unrelated-state corruptions are
rejected.

Two worker failures remain preserved: malformed harness input serialization
before any campaign, then selection of a nonexistent positive control after the
baselines completed. The continuation selects an existing frozen case and reuses
the exact completed observations and mutant binary. It neither expands the
corpus nor repeats successful baselines. The parent campaign starts fresh.

Private evidence is under `reconstruction/phase-r2/`:

- `20261023-small-movement-helpers-characterization`, inventory SHA-256
  `e383485e357022a8b7a9613282b03754f532a7c5aa091137ef2c6fd275258caf`.
- `20261023-small-movement-helpers-parent-fresh`.
- Independent checks and receipts are in phase-r3's
  `20261023-nineteenth-wave-parent-verification`.

This accepts bounded helper correspondence, not physical i386 execution or
complete movement reconstruction. Full native suites were not repeated for this
report-only change.
