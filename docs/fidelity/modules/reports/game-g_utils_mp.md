# game/g_utils_mp module reconciliation

Updated 2026-11-04; baseline `01c5fb6`.

**63/63 reconciled: 53 equal, 2 fixed, 8 intentional.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

The two text corrections below are the only changes. `MT_Free` is called with the i386 size 0x70 while `tagInfo_t`
is 120 bytes on arm64. That is harmless: `MT_GetSize` maps both sizes to the same class.

## Corrections

`G_LocalizedStringIndex` restores the label at 0x24872c, and `G_Spawn` the localized prefix at 0x2e0f0c.

## Retained differences

`G_Find`, `G_FindStr`, `G_KillBox`, `G_PickTarget`, `G_PlaySoundAliasAsMaster`, `G_PlaySoundAliasAtPoint`, `G_Unused`
and `infront` have no Mac code and no native caller.

## Verification and limits

This module was reconciled in one batch with `g_client_fields_mp`, `g_weapon_load_obj`, `g_weapon_mp`,
`g_cmds_mp`, `g_utils_mp` and `g_client_script_cmd_mp`. No new per-fix tests were written, so each fix here was
compiled and reviewed but not run on its own. The local CI steps 02 to 13 passed on this batch, and both builds
compile. `check_legacy_guards.py` found source changes only in the configuration with the port on.

Both 2,000-frame replays match the trace from the previous commit, and stock matches CoD2x, so that recording reaches none of the corrected paths. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, with the first player difference at frame 102 and the first state difference at frame 116.

Two fresh local reviews checked every source hunk against the binary and spot-checked about 30 functions marked equal. The first found a misapplied edit, which had put the unreliable send meant for `Cmd_SetViewpos_f` on `DeathmatchScoreboardMessage`, and a wrong warning string in `g_weapon_load_obj`. Both were corrected before this commit. The second found no further mismatch. Neither read every equal row, so a single row can still hide a slip.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-batch2-20261104.noindex/`.
