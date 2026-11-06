# Player prediction mover and ADS repair

`CG_PredictPlayerState` now reconciles the platform position from snapshot
physics time to the preceding render time before calculating prediction error
and yaw correction. It also initializes the movement flag before the
no-new-command branch and clears the ADS perturbation latch when reticle zoom
is unavailable. All three changes are restricted to `COD2_X64`.

Baseline: canonical checkout at `d497f60905e580743828cbee6492d9b252bfbdcf`,
initially clean. `behavior_changed: true`. No shared headers or CoD2x-specific
rules changed.

## Reference evidence

Independently inspected the private Mac 1.3 function JSON for
`CG_PredictPlayerState`, image-relative ID `1dcafa`, entry VA `0x1ddafa`.
Only behavioral descriptions and addresses are recorded here:

- VA `0x1de1a2`–`0x1de1c9` supplies `oldTime` as the fourth argument and
  `physicsTime` as the third argument of the first mover adjustment. Its yaw
  result is converted into the short-angle domain and accumulated immediately
  afterward. The port had reversed those two times.
- VA `0x1dde91` branches to `0x1ddea1` when the snapshot command time is at
  least the newest command time; `0x1ddea1` zeroes the register that controls
  the later mover adjustment. The port jumped over its `moved = 0` assignment.
- VA `0x1ddd07` branches on a false reticle-zoom result to `0x1ddbd9`, which
  clears the ADS latch. The port only cleared it when the ADS flag was absent.

No private source or assembly was copied into the repository.

## Focused checks

Inspected `tests/camera/run.py`, `tests/camera/camera.c` and the model fixture
runner pattern. The existing camera fixture does not execute player prediction.
Added `tests/camera/prediction.c` and `tests/camera/run_prediction.py`, which
include the full production prediction translation unit with native engine
types and synthetic external dependencies. Assertions cover resulting origin,
prediction error, yaw delta, and ADS offset/latch state across repeated calls.
The no-command cases also assert completion of player-state transition and
entity-state publication, including copying the published next state into the
current state.

The platform stub translates at 100 units/second and rotates at 250 degrees/
second; movement itself advances one axis by one unit. This isolates time
selection from the separately tested movement implementation.

Before editing engine code, both stock and CoD2x baseline runs failed four
cases with assertion aborts (exit `-6`):

| Case | Regression exposed |
| --- | --- |
| `mover` | Wrong prediction error from reverse platform compensation |
| `no-command` | Origin adjusted despite no predicted movement |
| `ads-reset` | Reticle zoom loss leaves the latch set |
| `ads-reentry` | Returning zoom fails to produce a new ADS offset |

The final fixture additionally covers snapshot command time strictly greater
than the newest command time (`acknowledged-ahead`), alongside the equal-time
`no-command` case. That added case was run against the unchanged baseline
source after the repair and also aborted on the unexpected origin adjustment.
This final baseline run was stock only; the earlier four reds were observed in
both configurations.

Five controls passed before and after: `no-mover`, `missing-command`, `zoom`,
`ads-flag`, `ads-zero-max`. After the engine repairs all ten final cases passed in
both configurations under ASan/UBSan. The mover case also checks the expected
yaw delta of 2275 short-angle units, origin `(110,101,100)`, and prediction
error `(2,0,0)`.

The fixture compiler uses `-ftrivial-auto-var-init=pattern` to expose the
uninitialized movement flag deterministically. ASan/UBSan do not diagnose
uninitialized scalar reads themselves, and an ordinary build's stack contents
cannot provide a stable regression oracle. This is fixture-only instrumentation;
production compilation retains its original engine flags.

Final commands from the canonical checkout (the baseline command returned 1
with five failures; both repaired commands returned 0). The initial nine-case
baseline evidence remains in `red-stock` and `red-codx`; final baseline evidence
is in `red-final-stock`:

```sh
taskpolicy -b nice -n 19 python3 tests/camera/run_prediction.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-prediction.noindex/red-final-stock" \
  --baseline d497f60
for mode in stock codx; do
  build=build-macos
  if [ "$mode" = codx ]; then build=build-macos-codx; fi
  taskpolicy -b nice -n 19 python3 tests/camera/run_prediction.py \
    --compile-db "/Users/stix/Projects/cod2-native-wt/decomp-f1-script/$build/compile_commands.engine.json" \
    --output "$HOME/Library/Application Support/CoD2-native-decomp/F2-prediction.noindex/green-final-$mode"
done
```

The actual production translation unit also compiled successfully with both
engine compile database commands, rebased to this checkout with private output
paths and dependency-file generation disabled. The engine compilation did not enable
pattern initialization or sanitizers. Logs and objects are under the same
private `F2-prediction.noindex` evidence directory.

`unifdef -UCOD2_X64` projections of the baseline and repaired engine file compare
exactly with `cmp`; private files are `base-off.c` and `current-off.c`.
`git diff --check` passes. This is source preservation, not proof of Linux i386
object identity.

## Limits and integration

Changed paths: `src/PC/cgame_mp/cg_predict_mp.c`, the two new prediction fixture
files above, and this report. No commits or index changes were made by this
worker. The orchestrator owns CI registration, review and integration gates.

No behavior change lacks a focused regression case. Full builds, live moving
platform/ADS gameplay, original-runtime replay and Linux object comparison were
outside this bounded worker task and remain unverified here. The synthetic
fixture establishes the repaired prediction/control-flow slice; it does not
establish full player prediction, collision, interpolation, random distribution,
or every-frame fidelity.
