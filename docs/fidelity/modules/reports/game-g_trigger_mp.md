# game/g_trigger_mp module reconciliation

Updated 2026-11-04; baseline `c05b9ff`.

**22/22 reconciled: 16 equal, 6 fixed.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The Linux-only helpers are inlined at the Mac spawners.

## Corrections

**`trigger_lookat` had the wrong contents.** Mac 0x1b7532 stores 0x20000000, which Linux names
`CONTENTS_TRANSLUCENT`. Native stored the generic trigger mask.

**The `wait` test differed on NaN.** All four spawners skip the flag on an unordered compare, as Linux
`wait <= 0` does.

`tests/lp64/game/trigger_fields.py` fails on the previous commit and passes both builds.

## Verification and limits

This module was reconciled in one batch with `g_scr_mover_mp`, `g_spawn_mp`, `g_trigger_mp`,
`g_svcmds_mp`, `g_team_mp`, `scr_const_mp` and `player_use_mp`. The local CI steps 02 to 12 passed on
this batch, and the source-based fixture step passed on a rerun after the last fix below. Both builds
compile. `check_legacy_guards.py` found source changes only in the configuration with the port on.

Both 2,000-frame replays agree with each other and move closer to the original Mac recording. Against the original, the differences in `viewangles`, `pm_flags` and `pm_time` fell from 1,228, 80 and 104 to zero, `velocity` fell from 1,313 to 1,257, and `origin` fell from 3,786 to 3,781. The first player difference stays at frame 102, a last-bit float in `velocity[1]`, and the first state difference stays at frame 116. The trace first differs from the previous commit at frame 648. Which fix moved these counts was not isolated.

One fresh local review checked every source hunk against the binary and spot-checked about 22 functions
marked equal. It found the item table error in `Player_UpdateCursorHints`, which the first comparison
had passed, so a single `equal` row can still hide a slip. It did not check `g_team_mp`, `scr_const_mp`,
`Reached_ScriptMover`, `InitScriptMover`, `ScriptEntCmd_RotateVelocity`, `AddIP`, `G_LoadStructs`,
`SP_worldspawn`, `Scr_GetEnt` or `Scr_GetEntArray`.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-batch-20261104.noindex/`.
