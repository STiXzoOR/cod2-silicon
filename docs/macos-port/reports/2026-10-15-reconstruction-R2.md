# R2 — Original-instruction scalar math comparison

Status: four-function pilot complete; further subsystem reconstruction and
whole-game validation remain open. Private receipts are under
`reconstruction/phase-r2`; no binaries or derived observations are committed.

## Confirmed mismatch and repair

The native `LerpAngle` implementation previously adjusted an already-rounded
angle difference. The original i386 instructions adjust the destination angle
first and then recompute the difference. Those operation orders differ at
float32 rounding boundaries. The native-only path now preserves the observed
order; the legacy implementation remains under the existing compile-time gate.

The new oracle executes the hash-pinned original Mac MP instructions through
the existing bounded Unicorn harness and compares exact float32 bits with the
real production math source compiled using engine flags. It verifies text-symbol
identity, original harness self-tests, complete case identities and absence of
modeled imports during math execution. Missing, duplicate, nonfinite or invalid
observations cannot pass. Reports must be new files outside the checkout.

## Validation and limits

- Initial evidence preserved 80 mismatches, all in `LerpAngle`, among 3,670 cases.
- After repair, stock and CODX each pass all 3,670 exact-bit comparisons under
  ASan/UBSan: 425 `AngleNormalize360`, 425 `AngleNormalize180`, 538
  `AngleSubtract`, and 2,282 `LerpAngle` cases. No math import models executed.
- Eight source-only oracle tests pass, including rejection of matching outputs
  after a modeled import and acceptance of an unchanged nonzero self-test
  import baseline.
- Camera fixtures pass all seven cases in stock and CODX. Both complete ARM64
  application builds pass. The legacy preprocessor check passes for the changed
  source across ten configurations; full Linux x86 object comparison was not run.
- Independent four-role review found no remaining correctness issue. Its
  missing import-gate test finding was resolved before committing.

The input domain is bounded finite angles, signed zeros, subnormals, half-turn
boundaries and seeded samples. NaN/infinity, out-of-range casts, arbitrary vectors,
unbounded subtraction loops and full-game behavior are not certified. A finite
sample campaign does not prove universal function equivalence.
