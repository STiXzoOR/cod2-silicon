# F1 missile impact branching

Baseline `1c8fc45`, branch `decomp/f1-shared-wave5`. Native stock and CoD2x
behavior changes; legacy source remains unchanged under `COD2_X64=OFF`.

`G_RunMissile` now bounces a missile with zero direct damage when it hits a
damageable entity, regardless of its bounce flag. A nondamageable target still
bounces only missiles with that flag. Direct damage and accuracy accounting
run only for damageable targets. Missing surface material becomes flesh only
for a damageable client target on the zero-damage bounce path.

The Mac authority is `__Z12G_RunMissileP9gentity_s`. At `0x1ca41a–0x1ca427`,
a nondamageable target with the bounce flag jumps directly to the bounce call.
At `0x1ca45d–0x1ca464`, only a damageable target reaches the direct-damage test
at `0x1cabeb`. Zero damage goes through optional client flesh material setup
at `0x1cabf9–0x1cac11` and the bounce call at `0x1cac1c`. Nonzero damage goes
to accuracy/trajectory evaluation at `0x1cadec`, then `G_Damage`. Other targets
bypass that direct-damage block. These branches were independently interpreted;
no private source or disassembly is included here.

The existing production missile fixture gained four impact groups, each with
client/nonclient, bounce-flag and material variants (32 cases). It checks final
entity type/free-after-event, reflected velocity, direct-damage/accuracy calls,
and bounce/impact event plus material. The fixture supplies collision results;
production `G_RunMissile`, bounce math and trajectory functions choose outcomes.
The shared grenade fixture records event parameters and allows direct damage
only when explicitly expected; existing paths retain a rejecting assertion.

Before production changes, zero-damage damageable targets failed the final
entity-type check, nondamageable direct hits wrongly called damage, and a
nondamageable client wrongly received flesh material. Positive direct hits to
damageable targets already passed. After the corrections, all thirteen missile
groups pass with stock and CoD2x engine flags under ASan/UBSan with recovery
disabled. This includes the nine earlier missile regression groups.

Commands (private output directories were initially absent):

```sh
OUT="$HOME/Library/Application Support/CoD2-native-decomp/F1-missile-impact.noindex"
taskpolicy -b nice -n 19 python3 tests/lp64/game/f1_missile.py \
  --compile-db build-macos/compile_commands.engine.json --output "$OUT/green-stock"
taskpolicy -b nice -n 19 python3 tests/lp64/game/f1_missile.py \
  --compile-db build-macos-codx/compile_commands.engine.json --output "$OUT/green-codx"
```

`unifdef -UCOD2_X64` output is byte-identical to baseline. This is a bounded
impact correction, not whole-module or complete original-server parity.
Integration build/live outcomes are recorded in the wave5 report.
