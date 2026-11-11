# game/g_missile_mp module reconciliation

Updated 2026-11-04; baseline `4fb6c80`.

**12/12 reconciled: 6 equal, 6 fixed.** All six Mac exports were compared across
their complete bodies, 6,787 bytes in total, and every float literal was decoded
from the binary. The six Linux-only helpers are inlined on Mac and were compared at
their inlined sites. Mac 1.3 is the authority; Linux 1.3 supplied candidates and was
read only. The legacy source is unchanged.

## Corrections

**Explosion marks were inverted.** `G_MissileImpact` sent `0xBD` (`EV_ROCKET_EXPLODE`)
for a client or body-part hit and `0xBE` (`EV_ROCKET_EXPLODE_NOMARKS`) otherwise. Mac
0x1ca4c5-0x1ca4f4 is the reverse, so missiles left no marks on world surfaces and
left marks on players. The existing `impact_target` and `grenades` assertions encoded
the old behavior and now expect the Mac event.

**A direct hit also took splash damage.** Impact splash passed the missile as the
ignored entity. Mac passes the entity hit (`%ebx` at 0x1cacee). `G_ExplodeMissile`
correctly ignores the missile itself (0x1c96d2).

**Grenades never passed through marked entities.** `G_RunMissile` tested `flags < 0`
on a 32-bit field, which is bit 31. Mac 0x1cab6b compares the low 16 bits as signed,
which is bit 15, the bit set at `g_scr_main_mp.c`. The retrace therefore never ran.

**An ownerless missile passed NULL to `LogAccuracyHit`.** Mac 0x1cadec-0x1cae0e
indexes the owner unconditionally and only `G_Damage` maps "none" to NULL
(0x1caf51). Native would crash on an ownerless missile hitting a damageable client.

**Splash gates tested for positive.** Mac tests inner damage for nonzero at 0x1c966c
and 0x1ca56f. This only matters for a negative value in weapon data.

**`G_BounceMissile` reported an impact on NaN.** Mac takes the no-event return for an
unordered comparison at 0x1c9fca-0x1c9fd1.

## Function status

| Function | Status | Mac location |
|---|---|---|
| `G_BounceMissile` | fixed | 0x1c9db0-0x1ca0e2, 819 bytes |
| `G_ExplodeMissile` | fixed | 0x1c94f4-0x1c9795, 674 bytes |
| `G_MissileImpact` | fixed | inlined, 0x1ca3f2-0x1ca583 and cold blocks |
| `G_MissileLandAngles` | equal | 0x1c9b80-0x1c9dae, 559 bytes |
| `G_MissileTrace` | equal | inlined at each trace site |
| `G_MissileTrace_IgnoreEntity` | equal | inlined, 0x1cab79-0x1cabd9 and 0x1cad3d |
| `G_RunMissile` | fixed | 0x1ca0e4-0x1caf7a, 3,735 bytes |
| `G_RunMissile_CreateWaterSplash` | equal | inlined, 0x1ca81e-0x1ca891 |
| `G_RunMissile_Destabilize` | fixed | inlined, 0x1ca988-0x1caf76 |
| `G_RunMissile_GetPerturbation` | fixed | inlined, 0x1ca9de-0x1ca9f7 |
| `fire_grenade` | equal | 0x1c9796-0x1c99be, 553 bytes |
| `fire_rocket` | equal | 0x1c99c0-0x1c9b7e, 447 bytes |

`G_RunMissile_Destabilize` and `G_RunMissile_GetPerturbation` needed no new change;
their required fixes landed in the 2026-10-24 batch
(`docs/macos-port/reports/2026-10-24-F1-missile.md`). Native effect timing adds a
float 1.0 as Mac does at 0x1c977b-0x1c978c; Linux uses a double there, and Mac wins.

## Verification and limits

`tests/lp64/game/f1_missile.c` gains six cases: `impact-event`, `impact-splash`,
`inner-sign`, `impact-owner`, `grenade-pass` and `bounce-nan`. All six fail on `4fb6c80`
and pass in stock and CoD2x under ASan and UBSan with assertions on. All 19 cases and
the `grenades` fixture pass in both builds. Compile-time assertions tie the water-splash
stores at 0xa0, 0xd8, 0x88 and 0x74 to the native fields. Every step of the macOS CI job
passes locally except that its dependency setup step was skipped, the machine already
having those dependencies. The x86 ILP32 job could not be run on this machine.

Fresh 2,000-frame dedicated replays match in every state and player field: stock against
a build of `4fb6c80`, and stock against CoD2x. Comparison with the original trace is
unchanged, with the first state difference at frame 116 and the first player difference
at frame 102.

The replay recording has no missile hitting a player and no pass-through grenade, so
these corrections are checked by the focused tests rather than at runtime. Whole-project
parity with the original is not claimed; the known frame 102 and 116 differences remain.

One fresh local review read all source hunks against the Mac disassembly and confirmed
them. It found one stale `grenades` assertion, now fixed, and a relaxed fixture check,
now restored in that fixture's scenarios. Its remark about the end address of
`G_BounceMissile` was checked and is wrong: the final 5-byte jump ends at 0x1ca0e2.

Private evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-missile-20261104.noindex/`.
