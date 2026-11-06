# F1 emitter child orientation and spacing

`Emitter_UpdateEmitFx` now preserves the Mac child-effect orientation, bolt
identity, emission spacing, and accumulated emitter state. The earlier particle
motion corrections remain intact. Native `Emitter_UpdateEmitFx` and its caller
`Emitter_Update` are corrected together; emitter creation is unchanged.

## Original behavior

The Mac function `__ZN7Emitter12UpdateEmitFxEPfPK13orientation_t` is the
independent authority for this reconstruction. The Linux 1.3 implementation
identified the candidates; no reference implementation was copied.

- `0xa405b–0xa41b8` samples at 12 ms intervals until the candidate is at least
  the configured distance from the previous emitter position plus accumulated
  bind offset. Passing samples emit a child before the following correction.
- For bolted emitters, `0xa41d4–0xa421d` transforms the candidate into world
  coordinates and passes `mBolt + 0x3c` as the child's bolt identity, with no
  explicit axis. For unbolted emitters, `0xa44b5–0xa453f` normalizes velocity
  and builds an orthogonal basis; zero velocity copies the existing renderer
  axis at `0xa4544–0xa4583`. Native previously passed neither axis nor bolt.
- `0xa4222–0xa426d` forms squared speed and the correction denominator
  `2 * speedSquared * seconds`. When nonzero, `0xa43b0–0xa440b` subtracts
  `floorf(((speedSquared * secondsSquared - stepSquared) / denominator)
  * 1000 + 0.5)` milliseconds and clamps the resulting duration to at least
  the already sampled interval. The indirect call at `0x1acc875` resolves to
  `_floorf`; binary float literals `0x326180`, `0x326190`, and `0x326210`
  are 1000, 0.5, and 0.001. The native square-root expression was unrelated
  to this calculation.
- `0xa4413–0xa44b0` recalculates the velocity and stored emitter position at
  the corrected time. This recalculated position does not include bind
  displacement; Mac stores bind displacement separately. The child already
  emitted above retains its original sampled position.
- `0xa4294–0xa42df` calculates and stores the next velocity at offset `0x258`
  (`initialVel` in the native structure). `0xa42e2–0xa4337` accumulates bind
  movement at `0x264` (`velocityDelta`). Native had instead written a velocity
  difference to the bind field and left the next velocity unchanged.
- `0xa433c–0xa435a` advances the prior emission time and elapsed duration.
  `0xa4362–0xa439b` picks the next distance using density plus `crandom()`
  times variance. Native reconstructed time from the effect start and reused
  the old distance indefinitely.

The misleading existing native field names remain unchanged to avoid expanding
this correction into a layout and call-site refactor. The complete bounded
assembly remains private in `F1-fx-primitives.noindex`.

## Verification

The production FX fixture adds seven numeric scenarios: a moving emitter,
stationary axis fallback after a prior emission, a moving bolt, a below-distance
sample, the exact representable distance boundary, three consecutive emissions,
and skipped samples that force the correction's minimum-duration clamp.
Assertions cover child count, position, axis or exact bolt pointer, stored
position, next velocity, bind offset, emission time, new step, and RNG
consumption. Expected positions and times are fixed numeric examples derived
from the Mac operations, rather than a second implementation of the loop.

The final fixture reports 57 failed checks against a private copy of the
pre-emitter production source. Both the stock and CoD2x corrected fixtures pass
under ASan/UBSan with assertions active. The existing scheduler and nine
primitive lifecycle checks still run, together with the earlier particle cases.

The first version of this report incorrectly called the stationary lifecycle
emitter a synthetic invalid setup and supplied its renderer axis manually.
That interpretation was wrong: the fixture calls real `FxScheduler_CreateEffect`.
Both native and Mac creation leave renderer axes zero; the Mac update computes
them before the first child emission. The artificial fixture assignment has been
removed, and the normalized child-axis assertion is unchanged. The independent
stationary numeric case still tests a rotated fallback axis.

The distance-boundary case uses the representable distance after float position
subtraction, rather than assuming decimal 1.2 survives that operation.

## Caller ordering correction

Mac `__Z13FX_AddEmitterP15EffectPrimitivePA3_fPKfii` zeroes the new object at
`0x5b381–0x5b3a6`, initializes the particle, then stores angles at
`0x5b5c9–0x5b5eb`; it does not create renderer axes. `__ZN7EmitterC1Ev` only
calls the particle constructor and installs its vtable. The update caller,
`__ZN7Emitter6UpdateEv`, supplies the missing initialization:

- `0xa4c65–0xa4ccf` preserves the previous local origin and updates motion.
  If all three components remain equal, `0xa5136` reaches the damping path
  `0xa4cdf–0xa4d14`, multiplying angular velocity by 0.7 (literal `0x326264`).
- Both moving and stationary paths reach `0xa4d19–0xa4d79`: update stored
  angles using frame milliseconds times 0.01, then call `AnglesToAxis`.
  This happens regardless of model draw flag `0x10`.
- Size evaluation precedes `0xa4ed9`, where `0x10` gates model origin,
  scale, and the bolted world-space axis conversion at `0xa4f16–0xa4f55`.
  Native had emitted children first and only calculated axes inside that gate.
- Bolted bind input is initially zero. For a model with a prior emission time,
  `0xa50c2–0xa512a` calculates `(boltOrigin - storedBindOffset) *
  ((now - emitLastTime) * 0.001)`. The previous native code instead calculated
  a renderer-position derivative and applied it even without the model flag.
  This unusual original multiplication is preserved deliberately.
- `0xa4f79–0xa4f8f` calls `UpdateEmitFx` after these updates and returns true.
  There is no zero-radius physics-stop flag or RGB/alpha update in this emitter
  body. The native code had added both. Bolt lookup failure returns false at
  `0xa4fe5`; the native caller now uses the existing matching bolt helper.

The new `CheckEmitterUpdate` exercises real scheduler creation and normal FX
updates: model flag on/off crossed with bolt present/absent, 90-degree model
and world axes, zero-sized model behavior, unchanged emitter color, stationary
versus moving angular velocity (7 versus 10 degrees), and a moving cached bolt
whose child position and accumulated offset distinguish the original bind
formula and model gating. The existing real creation → update → stationary
child lifecycle also runs without any axis injection.

Removing the artificial assignment alone failed the normalized-axis assertion.
With all added caller scenarios, the pre-fix body produced **35 failed numeric
checks plus that assertion** (`red-update-final.log`). Both final stock and
CoD2x runs pass with ASan/UBSan (`green-update-stock.log`,
`green-update-codx.log`). The only source edit after those builds was removing
extra blank lines in the disabled legacy branch to restore exact OFF output;
no native instructions or test assertions changed.

```sh
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-emitter.noindex/green-update-stock"
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-emitter.noindex/green-update-codx"
git diff --check
```

`unifdef -UCOD2_X64` plus `cmp` confirms `FxPrimitives.c` still matches
`bd18d2f` byte-for-byte with native code disabled. This does not replace the
unavailable original i386 object comparison. No full build, game run or visual
reference capture was performed by this worker; integration owns those checks.
This does not claim the whole EffectsCore module is reconciled.

## Review correction: defined i386 integer behavior

The independent review identified an out-of-range correction conversion when
a bolted emitter has speed 0.01, step 10, and a nonzero world-origin offset.
The integrated sanitizer fixture reproduced a conversion of approximately
-4.16667e10 to int. Mac uses cvttss2si at `0xa43e0`, wrapping subtraction at
`0xa43e8`, and a signed minimum clamp at `0xa43f2–0xa43f4`. Native now uses
the same conversion convention as the existing script VM: values outside
int32 range and NaN yield INT_MIN, followed by explicit unsigned 32-bit
arithmetic for the loop's integer increments, subtraction and stored time.

The regression pauses its mock clock after one child emission so that the
resulting zero-duration iteration can be observed without an endless test.
This preserves the original zero-progress behavior; it does not add a loop
progress policy or change density. Mac FX_AddEmitter also seeds the offset
from bolt origin at `0x5b6ae–0x5b6bf`, so the review's separate spacing/rate
candidate is original behavior rather than a new fidelity regression.

The bounded regression passes under stock/CoD2x ASan/UBSan after the fix;
both full builds pass and the final OFF source still matches 83aacfb.
