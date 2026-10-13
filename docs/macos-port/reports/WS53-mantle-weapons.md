# WS53 — Weapon inactivity during mantles

2026-10-07; base `a7ef355` (WS48/WS50 atop the 0.2.1 integration baseline).

FA02's two absent mantle checks are restored in the native weapon-change
helpers. The Mac reference's `PM_Weapon` at `0x7d0ce` requests a change to
weapon zero when `Mantle_IsWeaponInactive` succeeds (`0x7d456`). Its second
check at `0x7d842` prevents the finishing change from reselecting the command's
weapon while that predicate remains true.

The first check preserves the reference's state/timer exclusions: offhand and
binocular states stay in their own paths; firing/melee with remaining time
wait, while reload and rechamber can be interrupted. The existing production
mantle predicate and transition table decide which mantles keep the weapon
usable. The new caller declares the predicate's real `Bool` return type.

The newly reachable unarmed completion also needs the ordinary weapon-zero
definition: Mac `0x7e75e`–`0x7e835` enters raising, emits the weapon-change
event, applies definition-zero timing/conditions and starts the unforced
animation event. Native completion previously returned directly to idle for
zero. A second failing regression exposed that shortcut before its correction.
Zero is excluded from alternative-weapon switching, as in `0x7e770`.

## Verification

`tests/movement/weapons.py` compiles the entire production weapon and mantle
translation units into an ASan/UBSan fixture using actual Release engine flags.
Synthetic boundaries supply animation/event services and weapon definitions;
the mantle predicate, transition table, weapon timers, change helpers and
top-level `PM_Weapon` run from production source.

- Original source fails the expected first mantle lowering in both stock and
  CoD2x. The intermediate fix also failed the independent unarmed raising-time
  assertion; both red results are retained.
- Corrected stock and CoD2x pass a held-fire sequence through lowering,
  unarmed transition, mantle exit, raising and resumed fire; 28 combinations
  of mantle enable/flag/transition; and 92 state/timer/delay combinations.
- Both changed/native companion translation units compile with actual Release
  commands. Focused cross-TU ABI audits report two TUs, zero errors and zero
  mismatches in each mode.
- Scoped stub gates report two existing candidates and zero new in each mode.
- Ten legacy preprocessing configurations show zero differences. The legacy
  source extracted with `unifdef -UCOD2_X64` is byte-identical to the baseline.
  This supplements, and does not replace, the unavailable Linux i386 artifact
  comparison.

Evidence remains private in
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/` and
`/tmp/cod2-weapon-*.log`. CONTRIBUTING includes the focused runner in both
build modes. No game was launched by this unit. Full composed build/runtime
acceptance remains necessary; this is not a complete weapon-state parity audit.
Other existing weapon-selection flags and unrelated animation-event differences
were not generalized from the bounded mantle evidence.
