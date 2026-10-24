# F1 particle motion and collision corrections

The native particle implementation now stops slow particles on upward-facing
surfaces and preserves the original velocity-channel coordinate spaces. This
batch covers four functions in `FxPrimitives.c`; it does not close the module.

## Behavior and Mac evidence

- `Particle::ApplyImpact` stops when the normal points upward and speed squared
  is below 16, or whenever the trace fraction is zero. Native code previously
  continued bouncing in the first case and bypassed the zero-fraction check for
  fast floor impacts. In `__ZN8Particle11ApplyImpactEPK13orientation_tfPKffS4_`,
  `0xa4648–0xa4676` tests the upward normal and speed; the ordered-below branch
  at `0xa47b8` reaches the stop block. `0xa467c–0xa4686` checks zero fraction.
  `0xa47be–0xa47ce` clears `0x820` and sets `0x1000000`.
  The binary literal at `0x326260` is float 16.
- `Particle::CalcVelocityValue` converts world-space velocity into bolt-local
  space when an orientation exists. Local velocity on a bolt stays local;
  only an unbolted local velocity passes through the particle display axis.
  Native code ignored the orientation. In
  `__ZN8Particle17CalcVelocityValueEfPfPK13orientation_t`,
  `0xa3252` tests `0x20` in byte `mFlags+2` (mask `0x200000`),
  `0xa325f–0xa327e` calls `OrientationDirFromWorldDir` for a bolt,
  `0xa3345–0xa3361` handles copying, and `0xa3688–0xa36af` transforms
  the unbolted local vector.
- `Particle::CalcVelocity2Value` has the same coordinate-space branches but
  independently selects random blending with `0x100000` and world space with
  `0x400000`. Native code incorrectly reused both primary-channel bits and
  ignored the orientation. In
  `__ZN8Particle18CalcVelocity2ValueEfPfPK13orientation_t`,
  `0xa36cb` tests byte mask `0x10`, `0xa39f6` tests `0x40`,
  `0xa3a03–0xa3a22` transforms a world vector into bolt space,
  `0xa3ae9–0xa3b05` copies, and `0xa3e2c–0xa3e53` applies the display axis.
- `Particle::IntegrateVelocity2` now uses its own world-space bit `0x400000`.
  Mac checks byte mask `0x40` at `0xa24bc` and `0xa25b5` in
  `__ZN8Particle18IntegrateVelocity2EfPf`. The unusual random-blend bit remains
  `0x80000`: Mac explicitly tests byte mask `0x8` at `0xa2367`, unlike the
  sampled secondary-velocity function. This is preserved, not normalized.

Linux 1.3 was used to identify these candidates. Each correction above was
confirmed independently against Mac instructions. Reference code and assembly
remain outside the repository.

## Verification

The existing production FX fixture retains all scheduler, primitive lifecycle,
bolting and impact coverage. New cases call the actual particle implementation:
64 sampled-velocity calls cross both channels, all independent flag combinations,
and rotated bolted/unbolted coordinates; 16 secondary-integration calls cover
its separate bit contract; eight impact cases cover slow and boundary-speed
floor impacts, zero fractions, walls and downward normals.

Before the engine correction, the stock fixture completed with 119 failed
checks: 100 sampled components, 16 integrated components, and three stop cases.
Afterward the stock and CoD2x fixtures pass under ASan/UBSan with assertions
active. Commands, from this worktree:

```sh
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-primitives.noindex/green-stock"
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-primitives.noindex/green-codx"
git diff --check
```

`unifdef -UCOD2_X64` outputs for `FxPrimitives.c` at `bd18d2f` and this change
compare byte-identically with `cmp`. This is a source-preservation check, not
an i386 object comparison. No full build, live game, or reference capture was
run in this worker; integration owns those checks.

## Immediate follow-up

`Emitter::UpdateEmitFx` still differs substantially. At `0xa41ed–0xa421d`
Mac passes bolt identity to the child effect; at `0xa44b5–0xa453f` it constructs
an axis along nonzero velocity (or copies the emitter axis at `0xa4544`).
Native always passes null axis and null bolt. Its emission timing and state
updates also differ: Mac adjusts milliseconds with a rounded linear correction
at `0xa43b0–0xa43f4`, stores the next velocity at offset `0x258`, accumulates
bind displacement at `0x264`, advances the previous emit time at
`0xa433c–0xa4344`, and picks a new step at `0xa4362–0xa439b`. Native instead
uses a square root correction, writes a velocity difference into the bind
field, reconstructs time from effect start, and never picks a new step.
These confirmed emitter defects are queued separately to keep this batch
bounded. Private bounded assembly is in `F1-fx-primitives.noindex`.
