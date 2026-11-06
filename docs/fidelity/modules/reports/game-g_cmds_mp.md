# game/g_cmds_mp module reconciliation

Updated 2026-11-04; baseline `01c5fb6`.

**30/30 reconciled: 21 equal, 5 fixed, 4 intentional.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The Linux-only command helpers are inlined in Mac `ClientCommand`.

## Corrections

**Chat could miss players.** `G_Say` loops to `level.maxclients` at Mac 0x1a671b. Native looped to
`numConnectedClients` over slot indexes, so a player in a higher slot was skipped.

**`give` differed in two ways.** With no amount it heals to `stats[2]` at 0x1a6f48. The weapons loop gives a slot
weapon only above the old weapon index when a slot is empty, at 0x1a6be4-0x1a6c51. Linux agrees on both.

**A map restart vote ran the wrong command.** Mac 0x1a5962 stores `fast_restart`. Native stored `map_restart`.

**`tell` and `gc` are gone.** Neither Mac `ClientCommand` nor Linux has them. A client sending either now gets the
unknown command text. The Config Studio catalog was regenerated to match.

**`G_setfog`.** Mac reads the seventh value as an integer, skips NaN density and stores (far - near) + near.

## Retained differences

`Cmd_SetViewpos_f` keeps the optional pitch from commit 9bb9166, which the rendering sweep uses. Mac takes exactly
five arguments. Its two messages now use the unreliable send at Mac 0x1a61a8.

`ClientNumberFromString`, `SanitizeString` and `Cmd_Fog_f` are Linux helpers with no Mac code and no native caller.

## Verification and limits

This module was reconciled in one batch with `g_client_fields_mp`, `g_weapon_load_obj`, `g_weapon_mp`,
`g_cmds_mp`, `g_utils_mp` and `g_client_script_cmd_mp`. No new per-fix tests were written, so each fix here was
compiled and reviewed but not run on its own. The local CI steps 02 to 13 passed on this batch, and both builds
compile. `check_legacy_guards.py` found source changes only in the configuration with the port on.

Both 2,000-frame replays match the trace from the previous commit, and stock matches CoD2x, so that recording reaches none of the corrected paths. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, with the first player difference at frame 102 and the first state difference at frame 116.

Two fresh local reviews checked every source hunk against the binary and spot-checked about 30 functions marked equal. The first found a misapplied edit, which had put the unreliable send meant for `Cmd_SetViewpos_f` on `DeathmatchScoreboardMessage`, and a wrong warning string in `g_weapon_load_obj`. Both were corrected before this commit. The second found no further mismatch. Neither read every equal row, so a single row can still hide a slip.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-batch2-20261104.noindex/`.
