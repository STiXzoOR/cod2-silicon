# R2: preserve original acceleration signed-zero selection

The native `PM_Accelerate` maximum now selects `wishspeed` when the operands
compare equal, matching the original Mac i386 `MAXSS` instruction. With
`stopspeed=+0` and `wishspeed=-0`, the previous native `fmaxf` selected positive
zero and changed the resulting velocity bits. The correction applies only to
`COD2_X64`; the legacy expression and the minimum operation remain unchanged.

The public regression contains 16 literal original results covering both zero
signs crossed with acceleration and frame time zero/one. It checks all three
velocity components as integer bits, so ordinary floating-point equality cannot
hide the mismatch. Each test invocation runs in a fresh process.

Independent parent verification reproduced exactly four failing cases in each
variant before applying the production change. Afterward:

- Fresh whole-production Release probes matched all **405 retained independent
  original observations in stock and CODX**: 389 baseline cases and 16 zero ties.
  This run executed fresh native code; it did not re-execute the original oracle.
- The parent independently checked complete initialized object images, unchanged
  bytes outside velocity, ABI captures, original raw bindings, stock/CODX
  agreement, and every public table row against the original observations.
- Stock/CODX client and dedicated Release builds passed. Both movement and weapon
  revision matrices passed under ASan/UBSan, covering revisions 0, 3, 4, 6 and -1.
- The legacy guard check and all four actual SDK compile-entry comparisons
  preserved `COD2_X64`-off preprocessing. These are not artifact-byte comparisons.
- Simplification retained the minimal patch. Focused correctness and independent
  local adversarial review found no actionable issues. The attempted Claude
  review produced no review because its OAuth refresh failed.

The actual Linux x86 artifact gate remains blocked by the fixed upstream
baseline's existing compilation failure. Negative-zero reachability from live
game callers, hardware floating-point exception parity and whole-game equivalence
remain unproved. This is a bounded correction, not completion of R2 or the port.

Private evidence under `reconstruction/phase-r2/`:

- `20261023-acceleration-native-correction`: proposal, original bindings, worker
  red/green evidence and preserved setup failures. Inventory SHA-256
  `dd30b17e94ff345217afd75bc5f526c67e8298f68a5b27d2c4ff6c4502ecdf81`.
- `20261023-acceleration-correction-parent-verification`: independent replay,
  builds, matrices, source hashes and review receipt. Seal SHA-256
  `6a3301aea85e2765b4377a7baceeb45631171ac7fdca03ab0ec2327271797c0b`.

The parent also retained its initial inventory-check failure: the checker treated
the manifest's deliberately null self-entry as an ordinary file hash. The revised
audit verifies the manifest's separately supplied digest before that exception;
no evidence was changed.
