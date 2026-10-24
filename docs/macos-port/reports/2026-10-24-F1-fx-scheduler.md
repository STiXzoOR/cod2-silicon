# F1 FX scheduler spawn filters

Baseline `bd18d2f0983e9ec8d268c4e76dd81fb98c06e5f9`, shared branch
`decomp/f1-mover-fx`, checkout `decomp-f0-oracle`. Root-owned mover changes and
untracked `build-f0-stock` were preserved. No commits/index changes/game runs.

`behavior_changed: true`, native stock and CoD2x:

- `FxScheduler_PlayEffect` rejects points **inside** the scaled minimum spawn
  radius. Native previously rejected points outside it. Far-bound behavior is
  unchanged; equality at either bound is accepted.
- Previous-frame frustum culling uses spawn flag `0x400`, rather than `0x4`.
  Parsed `frustumCull` uses `0x400`; `0x4` means `orgOnCylinder`.

Mac authority: `__ZN11FxScheduler10PlayEffectEPK14EffectTemplatePKfPA3_S3_PK10FxBoltInfo`
starts at `0x61322`. Near-distance scaling/squaring at `0x61659–0x61668` is
followed by `ucomiss distSq,nearSq; ja` at `0x6166c–0x6166f`, rejecting only
when nearSq exceeds distance. `0x6141f` tests bit 4 in the second spawn-flags
byte (therefore `0x400`), calling `CullSpherePreviousFrame` at `0x61443`.
Linux 1.3 `EffectsCore/FxScheduler.cpp` agrees on both. Code was independently
implemented; no reference source or disassembly was copied into the checkout.

Extended existing `tests/platform/macos_fx_primitives.c`, which exercises the
real extracted scheduler and primitive creation/update paths. New cases check
actual active primitive and delayed queue counts, then run delayed effects
through real creation and expire all created particles. Cases cover distance
5/10/15/20/25 for range [10,20], zoom factors 1 and 2, immediate and delayed
spawn, and flags 0/4/0x400 with visible/culled responses. Frustum service calls
are counted; scheduler output is not supplied by a mock. Existing nine-primitive
lifecycle, impact, bolting and timing checks remain intact.

Before production edits, the extended fixture failed immediate-count,
delayed-queue/count and frustum-call checks on the unchanged scheduler. Existing
lifecycle tests reached these checks without failure. After the two corrections,
stock and CoD2x fixtures pass under ASan/UBSan with `halt_on_error=1`.

The existing runner now accepts optional engine compile DB and output paths,
rebases each translation unit's actual engine flags, enables assertions and
sanitizers, and compiles sequentially. Its scheduler extraction first selects
native `COD2_X64` branches using `unifdef`; raw brace extraction otherwise counts
both arms of the newly gated conditionals. The old no-argument temporary-output
mode remains available but was not separately rerun.

Exact final commands:

```sh
OUT="$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-scheduler.noindex"
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$OUT/green-stock"
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$OUT/green-codx"
```

Both returned 0 with `native FX: nine primitive lifecycles, scheduler dispatch,
bolting, impact axes and spawn filters passed`.

Legacy check: `git show bd18d2f:src/PC/EffectsCore/FxScheduler.c` into the private
output directory, `unifdef -UCOD2_X64` on baseline/current, then `cmp`: identical.
`unifdef` status 1 accepted as changed output. `git diff --check` passed. This
proves inactive-gate source preservation, not Linux i386 binary identity.

Changed files: `FxScheduler.c`, the existing primitive fixture/runner, this
report. No entire-module fidelity claim: registration, migration, all sampling,
all scheduler modes and live captures were not comprehensively reconciled.
These are client-visible effects, including gameplay-event playback through
`FX_PlayEffect`/`FX_PlaySimpleEffect` in `cg_ents_mp.c`; no server simulation
impact was demonstrated. No full build, game process or replay ran.

Discovery ruled out apparent curve defects: Mac intentionally pads the final
key with first-key values and retains original keyCount (`0xac3ed–0xac405`).
Mac migration also uses reciprocal normalization in scale range
(`0xaca2b–0xaca37` then `0xac50d–0xac541`), unlike the Linux guide. These original
behaviors were retained.
