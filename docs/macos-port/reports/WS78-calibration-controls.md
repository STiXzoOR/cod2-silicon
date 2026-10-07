# WS78 — Separate historical calibration from current fidelity review

2026-10-07. The full audit of integration `0f07adc` passes ABI/import checks,
inventory extraction and table extraction. Its unchanged calibration command
exits 1: `PM_FootstepEvent`, `PM_UpdateLean` and `Item_List_Paint` now score 2,
4 and 0, below the threshold of 8. WS43/WS42 restored these known-bug paths.
Demanding that repaired functions still trigger structural triage conflates
positive-control calibration with current fidelity acceptance.

The optional `--positive-control` input checks the seven required cases in an
explicit historical audit and reports current-case scores separately. Default
behavior and its failure status are unchanged. Both inputs require source
provenance and the same reference binary hash; output records the historical
file's SHA-256. Manual review notes still match the current seeded sample
exactly. No score, threshold, known-case list, stub baseline or verdict changes.

## Verification and limits

The 35 fidelity tool tests pass. Added checks prove repaired current cases do
not replace historical positive controls, missing historical cases fail, and
mismatched reference/missing provenance is rejected. A real CLI matrix uses
the preserved WS44 stock archive from source `02f760554d58abcf31f90226a3958c667d4030fe`
and the current `0f07adc` stock audit:

| Command scope | Expected and observed exit |
| --- | --- |
| Current audit, original command | 1: three known cases no longer flag |
| Historical audit with its original 30/30 review | 0 |
| Current audit with explicit historical positive control | 0: historical calibration only |
| Current audit with historical control and stale historical reviews | 1: sample mismatch |

Private command receipts are under
`CoD2-native-protocol/validation.noindex/ws78-calibration` in Application
Support. The original failing full-audit receipt remains intact.

Replaying an archived audit validates its stored calibration receipt; it does
not rerun historical extraction/scoring. Changes to that algorithm require
fresh historical extraction before relying on the archived control. This unit
does not finish current sample reviews, certify those repaired functions, or
establish whole-game fidelity. Current behavioral fixtures, ABI checks and
remaining audit findings keep their own outcomes. No engine code changed.
