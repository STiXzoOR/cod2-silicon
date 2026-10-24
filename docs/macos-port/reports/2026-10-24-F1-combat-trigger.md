# F1 nonclient damage callback and trigger filter fixes

Baseline `184775985e2d85ff8e6aab3f53ef8cadbf3eb44c`, branch
`decomp/f1-combat-trigger`, checkout
`/Users/stix/Projects/cod2-native-wt/decomp-ref-mtld3d`. HEAD/branch/clean state
were verified before edits. Worker writes are limited to:

- `src/PC/game_mp/g_combat_mp.c`
- `src/PC/game_mp/g_trigger_mp.c`
- `tests/lp64/game/f1_damage.c/.py`
- this report

No staging, commits, full builds or game processes. Root owns canonical review
and integration. Existing builds were untouched.

## Behavior changed

`behavior_changed: true`, native stock and CoD2x only.

- Nonclient `G_Damage` pain callbacks now receive the already computed normalized
  `localdir`, as death callbacks already did. Previously pain received the
  original direction. Input direction storage is unchanged.
- The two MOD mask filters in `Activate_trigger_damage` now compare MOD as
  unsigned and shift `1u`, matching the Mac range gates. `Use_trigger_damage`
  intentionally calls activation with MOD `-1`; it now bypasses these masks
  without executing a negative shift, resets trigger health when eligible,
  skips the queued trigger event, and retains delayed-free behavior.

No cached-health-after-notify change or other speculative combat behavior was
implemented. No reference source/disassembly was copied into the checkout.

## Evidence

Read the Linux 1.3 `game/g_combat_mp.cpp` and `game/g_trigger_mp.cpp` guide for
these paths; it agrees with Mac. Mac assembly is the authority:

- `__Z8G_DamageP9gentity_sS0_S0_PKfS2_iii13hitLocation_ti` starts at `0x1c4272`.
  `0x1c4369` loads the address of the local direction into EDI;
  `0x1c4376` calls `Vec3NormalizeTo` with that destination. The pain dispatch
  places EDI into callback argument 5 at `0x1c443a` and calls at `0x1c445d`.
- `__Z23Activate_trigger_damageP9gentity_sS0_ii` starts at `0x1b7de8`.
  The flag-8 filter compares MOD to 14 and uses unsigned `jbe` at `0x1b7e38`;
  the flag-256 filter compares to 13 and uses unsigned `ja` at `0x1b7f0f`.
  The shifts at `0x1b7f1a` and `0x1b7f6f` are reachable only within those
  unsigned ranges. Negative MOD values therefore do not reach either shift.
- Activation checks the threshold at `0x1b7dfd–0x1b7e09`, accumulation at
  `0x1b7e67–0x1b7e7e`, skips G_Trigger for MOD -1 at `0x1b7e84–0x1b7e87`,
  resets health at `0x1b7eee`, and retains delayed-free dispatch at `0x1b7efd`.

## Tests and observed red-before-green

Inspected existing `tests/lp64/game/grenade_death.c`, `grenades.c`,
`tests/lp64/game/run.py`, and searches for production `G_Damage`/trigger tests.
The former tests player-death grenade handling; other fixtures mostly stub
`G_Damage`. No existing fixture covered either changed behavior.

Added a focused fixture linking the actual `G_Damage`, `Use_trigger_damage`,
`Activate_trigger_damage`, and `com_math.c` normalization implementation.
Script services record notifications, the real pending-trigger array records
activation, and typed pain/death callbacks inspect arguments. No normalization
result is supplied by a stub.

Before production edits, the stock fixture ran six separate cases:

- `pain`: failed the expected normalized x=0.6 assertion for input `{3,4,0}`.
- `use8`: actual `Use_trigger_damage` produced UBSan `shift exponent -1 is
  negative` at `g_trigger_mp.c:381:31`.
- `use256`: same actual-use path produced the same UBSan failure at
  `g_trigger_mp.c:393:31`.
- `death`, `filters`, `thresholds`: passed on the baseline.

After the minimal production changes, all six pass in stock and CoD2x under
ASan/UBSan with `UBSAN_OPTIONS=halt_on_error=1`. Coverage includes:

- Nonunit direction normalization in pain; original direction bytes unchanged.
- Unchanged normalized death direction, inflictor/attacker, weapon, hit location,
  time offset, health and notification arguments.
- Actual trigger use with each affected mask flag, health reset, delayed-free
  call and absence of a queued trigger event for MOD -1.
- Each individual filter flag across every MOD 0–14 against independently
  listed MOD memberships, plus queued entity/use-count fields for accepted MODs.
- Damage just below/equal to threshold and accumulation just below/equal to
  its required amount, including health preservation/reset outcomes.

Exact final commands:

```sh
OUT="$HOME/Library/Application Support/CoD2-native-decomp/F1-combat-trigger.noindex"
taskpolicy -b nice -n 19 python3 tests/lp64/game/f1_damage.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$OUT/green-stock"
taskpolicy -b nice -n 19 python3 tests/lp64/game/f1_damage.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$OUT/green-codx"
```

Both returned 0, all six cases passed. The runner rebases source-root paths,
uses engine compiler flags with assertions/sanitizers, compiles sequentially,
and stores logs/objects only in the requested private output directory. The
pre-existing ignored `packed` attribute warning on `g_HitLocNames` remains.
Compiler lane released after verification.

## Legacy preservation and limits

For each of `g_combat_mp.c` and `g_trigger_mp.c`:

```sh
git show 184775985e2d85ff8e6aab3f53ef8cadbf3eb44c:src/PC/game_mp/$FILE > "$OUT/$FILE.baseline.c"
unifdef -UCOD2_X64 "$OUT/$FILE.baseline.c" > "$OUT/$FILE.baseline.legacy.c"
unifdef -UCOD2_X64 src/PC/game_mp/$FILE > "$OUT/$FILE.current.legacy.c"
cmp "$OUT/$FILE.baseline.legacy.c" "$OUT/$FILE.current.legacy.c"
git diff --check
```

Both comparisons and diff check returned 0 (`unifdef` changed-output status 1
accepted). This proves exact inactive native-gate source preservation, not the
unavailable Linux i386 object/binary comparison.

This unit reconciles the named callback/filter behavior, not all of either
file. Player damage scaling, radius traces, script-side health mutation,
concurrent entity lifetime and every trigger type remain outside its coverage.
No full engine build, live game or replay was run; overall multiplayer fidelity
remains incomplete.
