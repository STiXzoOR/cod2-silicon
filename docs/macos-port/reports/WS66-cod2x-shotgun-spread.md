# WS66 — Bounded CoD2x shotgun spread policy

2026-10-07. Worktree `codex/cod2x-shotgun`, original intake base `12f78f3`,
integration base `ab691c0` after retaining WS65's broadcast policy. Implements
WS56 CR06 for the current release. CR03 server lifecycle/version ownership and
complete native CoD2x hosting remain pending; this change advertises no new
revision and changes no connection identity or admission policy.

## Behavior and source authority

Read-only CoD2x reference `/Users/stix/Projects/cod2-native-refs/CoD2x`, clean
HEAD `d8c54695a5239ac99d1dfe96212b809b1a54bfee`, version 1.4.6.8:

- `src/shared/weapons.cpp:48–86` supplies two ordered eight-pellet distributions
  and the rotation contract. The native implementation independently expresses
  their coordinates as signed tenths of the spread radius.
- `:163–217` uses the patterned distribution only when
  `g_shotgun_spread_fix` is enabled and the spread weapon has eight pellets.
  It consumes one global random value for the group's angle, then one for the
  variant; values below 0.5 select the first variant. Every pellet uses that
  shared choice. The endpoint uses `fMinDamageRange`, the weapon basis, and
  the tangent of the angular spread. Other counts and the disabled switch use
  the original random endpoint generator, consuming two random values per
  pellet. These are ordered numerical behavior facts, not copied source.
- `:249` registers the switch enabled with CHEAT | CHANGEABLE_RESET
  (`0x1080`); `:259` installs the spread hook. Windows
  `src/mss32/hook.cpp:219–232` and Linux `src/linux/hook.cpp:42–53` initialize
  the weapons module through server initialization. The spread hook has no
  negotiated-version, `g_cod2x`, or admission condition.

Native `g_main_mp.c` now owns the switch and registers it through the existing
`G_RegisterDvars_impl` path. `g_weapon_mp.c` contains a static endpoint helper
and the narrow spread branch, both gated by `COD2_X64 && COD2_CODX`. The original
`Bullet_Endpos`, anti-lag rewind/restore, and `Bullet_Fire_Extended` damage,
trigger, glass, and rifle recursion code remain intact. Stock/off-switch,
non-eight spread counts, and non-spread weapons keep their original path.
No shared layout or existing function signature changed.

## Verification

Existing `tests/fixes13` and `tests/movement/weapons.py` were inspected; they
did not cover the authoritative bullet spread/damage chain. Added
`tests/cod2x/run_shotgun_spread.py` and `test_shotgun_spread.c` without editing
the shared fixes13 runner. The new runner remaps actual engine commands to
this worktree, retains Release optimization/FP flags, enables fixture assertions,
and writes generated sources, executables, objects, and logs outside Git.
Every compilation/check ran serially under `taskpolicy -b nice -n 19`.

The pre-implementation CoD2x baseline `12f78f3` failed in the actual
`Bullet_Fire` path: an enabled eight-pellet shot consumed 16 random values
when the independently specified group contract required two. Registration
was explicitly skipped for that characterization because the base lacks it.

Both stock and CoD2x ASan/UBSan fixtures pass 135 cases. They exercise the full
production weapon translation unit, `FireWeaponAntiLag` hip/ADS dispatch,
`Bullet_Fire`, and the actual static `Bullet_Fire_Extended`. Synthetic trace
services expose endpoints; damage expectations derive independently from
geometric distances and configured damage ranges. Coverage includes both
variants and the 0.5 selection boundary, five group rotations, zero/nonzero
angular spread, 256/512/2048-unit ranges, two weapon bases, all eight ordered
endpoints, RNG consumption and the next RNG value, counts 0/1/7/8/9,
off-switch controls, the 8192-unit non-spread path, normal dvar registration
default/flags, anti-lag restoration, and glass recursion.

Changed `g_main_mp.c` and `g_weapon_mp.c` compile with the actual stock and
CoD2x client/dedicated Release commands: four translation units per mode,
optimization retained and `-ffp-contract=off`. Scoped function ABI audits in
both modes report four translation units, zero errors, zero mismatches, and
zero unprototyped floating calls. Against `ab691c0`, the inactive CoD2x gate
check passes 17 configurations; fixes13 legacy comparison passes 10;
`tools/abi/legacy.py` passes five `COD2_X64`-off configurations and all four
actual SDK commands with that macro removed in each mode. An initial SDK
comparison exposed shifted legacy assertion identifiers; placing the new
declarations after the existing assertions restored identical compiler input.

Private outputs are under
`/Users/stix/Library/Application Support/CoD2-native-ws66/`, including
`baseline-12f78f3`, `stock-verified`, and `codx-verified`. Input compile databases
are the existing `CoD2-native-protocol/validation.noindex/{stock,codx}` engine
databases. Source inspection notes and generated reference inputs stay private.

## Limits

The trace, view-angle, history, and dvar registration services are synthetic;
real collision geometry, hitboxes, cheat enforcement, dvar re-registration,
server startup/restarts, authenticated network gameplay, and gameplay feel
were not exercised. This proves the bounded production endpoint/damage and
registration dispatch, not a full engine link or complete CoD2x server parity.
No game, benchmark, screenshot, install, global configuration change, live
server admission, or 0.3/Metal/Config Studio work ran. Linux i386 artifact
parity and full binary-dependent ABI gates remain parent integration checks.
