# F1 cylinder endpoint and update order

`behavior_changed: true`

Native `Cylinder_Update` now forms its endpoint from the local origin plus
length times the renderer's forward axis. A bolted cylinder transforms that
local endpoint exactly once. It no longer borrows Tail's historical-motion
endpoint calculation or advances its origin through particle velocity.

Changed paths:

- `src/PC/EffectsCore/FxPrimitives.c`: only `Cylinder_Update` changes.
- `tests/platform/macos_fx_primitives.c`: focused cylinder cases and correction
  of the cylinder-only valid-attachment motion expectation.
- This report.

Worktree: `/Users/stix/.codex/worktrees/faithful-fx-cylinder/cod2-native`, branch
`decomp/f1-fx-cylinder`. Clean starting HEAD was verified as
`3eff16fd9c7c50baa6185402d5325959e998c1b9`. No commits were made.

## Original behavior reconciled

Authority is the complete bounded Mac 1.3 `__ZN8Cylinder6UpdateEv` assembly
already available privately in `F1-fx-tail-motion.noindex`. No reference code or
assembly was copied into the repository. The full function's branches and
calls establish this order:

1. Check start time; normalize time, clamp above one and below zero
   (`0xa549c–0xa54d9`, lower clamp at `0xa5a75`). Preserve the existing native
   invalid-bolt guard and cache refresh/failure invalidation
   (`0xa5792–0xa57e9`, cached orientation at `0xa5bc4`).
2. Sample radius 0, optionally blending its random channel with flag `0x8000`
   (`0xa54f4–0xa5596`, random branch `0xa57ea–0xa5925`). Copy it to radius 1
   unless `nonUniformScale` is set (`0xa5596–0xa55a9`). Nonuniform radius 1
   uses its own channels and flag `0x10000` (`0xa5932–0xa5a70`, nonrandom branch
   `0xa5be7–0xa5c66`). The old native path always sampled the second radius
   and selected its random blend with the rotation flag `0x40000`.
3. Sample length, optionally blending with flag `0x20000`
   (`0xa55af–0xa56f2`, nonrandom branch `0xa5aac–0xa5b38`), then update RGB
   and alpha (`0xa56fa–0xa5705` / `0xa5b40–0xa5b4b`). There is no zero-radius
   early return or invisible-flag write in the original; color and endpoints
   still update with both radii zero.
4. Copy/transform origin, then form endpoint from local origin plus length
   times `mRefEnt.axis[0]`. Bolted construction and one transform are at
   `0xa5715–0xa5780`; unbolted construction is at `0xa5b5b–0xa5bb2`.
   Return true. The original has no particle-motion, tail-history, or
   rotation-update call, nor writes to the render axis or rotation.

The native implementation uses the existing channel sampler and retains the
existing attachment code. No other production function changes. Axis and
rotation are deliberately preserved; this is supported by bounded instruction
comparison and focused native assertions, not an original execution capture.

## Tests and results

Existing fixture and `2026-10-24-F1-fx-tail-motion.md` were inspected first.
All existing lifecycle, creation/orientation, emitter overflow, emitter motion,
tail, and attachment-failure cases are retained. Only the valid cylinder branch
of the shared attachment fixture now expects local origin `(10,20,30)` and
world origin `(80,210,330)` rather than the particle-motion values; the other
primitive expectations remain unchanged.

Added `CheckCylinderUpdate` covers unbolted and rotated/translated bolted
frames, non-axis-aligned render forward `(.36,.48,.8)`, nonzero local origin
`(10,20,30)` and length 10, zero frame time and successive perpendicular
velocity changes, unchanged tail history/axis/rotation, uniform and nonuniform
radii, independent randomized radius/length channels, rotation-bit independence,
zero-radius color/flag behavior, future-start rejection and both normalized-time
clamps. Endpoint expectations are independent fixed numbers: `(13.6,24.8,38)`
unbolted and `(75.2,213.6,338)` bolted; random length 15 has separate fixed
expected endpoints. Production initializers and update functions run directly.

Private evidence directory:
`~/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex`.
Commands below run from this unit's worktree, one compiler at a time.

Before changing production, the strengthened fixture against `3eff16f` failed
with **68 assertions** (66 cylinder checks and two cylinder-only attachment
expectations), runner exit 1. No sanitizer failure was reported. Exact command:

```sh
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex/red-stock" \
  > "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex/red-stock.log" 2>&1
```

After correction, both exact commands below passed, exit 0, with ASan/UBSan and
the actual stock/CoD2x engine compile flags. They retain the full existing
fixture rather than substituting a copied implementation.

```sh
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex/green-stock" \
  > "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex/green-stock.log" 2>&1
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex/green-codx" \
  > "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-cylinder.noindex/green-codx.log" 2>&1
git diff --check
```

OFF evidence: `git show 3eff16f:src/PC/EffectsCore/FxPrimitives.c` and the working
file were each passed through `unifdef -UCOD2_X64` using Python subprocess stdin
(accepting unifdef exit 0 or 1), saved as `base-off.c` / `current-off.c` in the
private evidence directory, and compared with `cmp`: byte-identical. A separate
byte comparison of text before and after `Cylinder_Update` proves all other
production text unchanged. `off-and-scope.log` records both passes. This proves
disabled source preservation, not original i386 object equivalence.

## Unresolved and unverified

No full build, game, VM, renderer capture, original binary execution, Linux
multilib comparison, or broad gate was run. Root owns integrated validation and
review. Original capture parity and complete EffectsCore parity are not claimed.
No known mismatch remains in this bounded Cylinder update comparison; surrounding
creation/draw/cull functions were not independently reconciled in this unit.

Considered and not built: no added guards, fallbacks, shared-header changes,
rotation integration, or generalized particle-appearance path. They are outside
scope; the generalized appearance helper would add rotation/animation behavior
absent from the original cylinder function.
