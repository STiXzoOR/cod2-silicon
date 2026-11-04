# game/g_client_script_cmd_mp module reconciliation

Updated 2026-11-04; baseline `01c5fb6`.

**62/62 reconciled: 55 equal, 5 fixed, 2 intentional.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The method table of 59 rows was compared mechanically against the binary and matches.

## Corrections

**`setWeaponSlotWeapon(slot, "none")` raised a script error.** Mac 0x1c0a80 treats `none` as an empty slot. Native
tested for an empty string instead, so `none` reported an unknown weapon.

**`finishPlayerDamage` wrote the wrong temp entity field.** Mac 0x1c3418 and Linux store the client number in
`s.clientNum`. Native wrote `s.attackerEntityNum`.

**`openMenu` and `openMenuNoMouse` read their argument too early.** Mac reads the name only for a connected client.

`setViewModel` restores the usage text at 0x1c02ee.

## Retained differences

`PlayerCmd_SetClientDvar` bounds its cleaning loop to the buffer, as `GScr_SetDvar` does. Mac can write past it.
When `finishPlayerDamage` gets no point, Mac places the temp entities with an uninitialized position; native falls
back to the entity origin. `ClientPlaying` is a Linux helper with no Mac symbol and no native caller.

## Verification and limits

This module was reconciled in one batch with `g_client_fields_mp`, `g_weapon_load_obj`, `g_weapon_mp`,
`g_cmds_mp`, `g_utils_mp` and `g_client_script_cmd_mp`. No new per-fix tests were written, so each fix here was
compiled and reviewed but not run on its own. The local CI steps 02 to 13 passed on this batch, and both builds
compile. `check_legacy_guards.py` found source changes only in the configuration with the port on.

Both 2,000-frame replays match the trace from the previous commit, and stock matches CoD2x, so that recording reaches none of the corrected paths. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, with the first player difference at frame 102 and the first state difference at frame 116.

Two fresh local reviews checked every source hunk against the binary and spot-checked about 30 functions marked equal. The first found a misapplied edit, which had put the unreliable send meant for `Cmd_SetViewpos_f` on `DeathmatchScoreboardMessage`, and a wrong warning string in `g_weapon_load_obj`. Both were corrected before this commit. The second found no further mismatch. Neither read every equal row, so a single row can still hide a slip.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-batch2-20261104.noindex/`.
