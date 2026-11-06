# game/g_weapon_mp module reconciliation

Updated 2026-11-04; baseline `01c5fb6`.

**25/25 reconciled: 22 equal, 3 fixed.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The Linux-only helpers are inlined on Mac, and Linux `FireWeapon` is Mac `FireWeaponAntiLag`.

## Corrections

**Melee hits carried the wrong damage flags and means of death.** Mac 0x1c8552-0x1c85b4 and Linux pass dflags 0
and means of death 7. Native passed them swapped, so scripts saw the wrong means of death for a melee hit.

**Melee treated a no-impact surface as a hit.** Mac 0x1c82a0 and Linux continue to the next trace.

**The offhand path of `G_GivePlayerWeapon` fell through.** Mac returns straight after it, at 0x1c93e0 and 0x1c94a6.
Native went on to the alternate weapon loop.

## Verification and limits

This module was reconciled in one batch with `g_client_fields_mp`, `g_weapon_load_obj`, `g_weapon_mp`,
`g_cmds_mp`, `g_utils_mp` and `g_client_script_cmd_mp`. No new per-fix tests were written, so each fix here was
compiled and reviewed but not run on its own. The local CI steps 02 to 13 passed on this batch, and both builds
compile. `check_legacy_guards.py` found source changes only in the configuration with the port on.

Both 2,000-frame replays match the trace from the previous commit, and stock matches CoD2x, so that recording reaches none of the corrected paths. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, with the first player difference at frame 102 and the first state difference at frame 116.

Two fresh local reviews checked every source hunk against the binary and spot-checked about 30 functions marked equal. The first found a misapplied edit, which had put the unreliable send meant for `Cmd_SetViewpos_f` on `DeathmatchScoreboardMessage`, and a wrong warning string in `g_weapon_load_obj`. Both were corrected before this commit. The second found no further mismatch. Neither read every equal row, so a single row can still hide a slip.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-batch2-20261104.noindex/`.
