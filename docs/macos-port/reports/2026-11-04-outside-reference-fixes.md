# Fixes found through outside references (2026-11-04)

Leads came from studying janost/vcod's protocol documentation and
tonga54/cod2-wasm's defect history. Both were read, not copied. Each fix was
confirmed against the Mac 1.3 binary or the Linux 1.3 cod2engine source,
implemented independently under `COD2_X64` with the legacy source unchanged,
and covered by a test that fails on the previous code.

| Area | Defect | Authority | Test |
|---|---|---|---|
| `Netchan_TransmitNextFragment`, `Netchan_Process` | `fragmentStart` was a short from the client and read as a short on the server; protocol 118 always sends a long, so fragments from a retail client misparsed | Mac 0x158e21, 0x15963a; Linux `net_chan_mp.cpp` | `tests/lp64/network/f1_transport.c` |
| `SV_GetArchivedClientInfo`, `SV_FindArchivedSnapshotForClientLocal` | archive time converted with `sv_maxRate` (default 0) instead of `sv_fps`, so anti-lag rewind and archived clients found no history | Mac 0x195a06 | `tests/lp64/game/snapshot.c` |
| `SV_ArchiveSnapshot` | `nextArchivedSnapshotBuffer` never advanced, so each archived snapshot overwrote the last | Mac 0x195107 | same |
| `SV_ArchiveSnapshot` | delta base used frames *older* than `sv_maxRate` frames; the original uses the newest frame no older than `sv_fps` frames. The two bugs hid each other while `sv_maxRate` was 0 | Mac 0x194e2b, 0x194e5e | same |
| `SV_WriteSnapshotToClientLocal` | delta gate compared against `gamestateMessageNum`; snapshot flag bit 0 came from `pureAuthentic` instead of `rateDelayed`; out-of-date diagnostic text | Mac 0x1960e8, 0x196144, STABS offsets 0x20e18/0x20e2c | same |
| `CL_InitCGame` | `CG_Init` arguments reversed, so `cg.clientNum` received a message sequence | Mac 0x17b4d2 and STABS offsets | `tests/online/cgame_init.c` |
| `SV_PointTraceToEntity`, `SV_PointSightTraceToEntity` | 3x3 `entAxis` passed to a 4x3 transform: the hitbox ray read a garbage origin past the array | Linux `sv_world_mp.cpp` (`entAxis[4]`, origin in row 3) | `tests/lp64/server/point_trace.c` |
| `Player_UpdateCursorHints` | `trigger_use` team test inverted and hint read from `animMovetype`/`dmgFlags` instead of `dmgFlags`/`scale` (the fields the script setters write) | Linux `player_use_mp.cpp`, `g_shared.h` unions | `tests/lp64/game/use_list.c` |
| `Cmd_CallVote_f` | `EF_VOTED` cleared only up to `numConnectedClients` | Mac 0x1a5a0c (`level+0x1e4`, maxclients) | `tests/online/callvote.c` |
| `rb_state.c` stage enums | alpha/color texture-stage enum tables were zero, so every DX7 stage change set state 0 | Mac `.data` file 0x329d5c/0x329d6c | `tests/lp64/renderer/texture_stage_enums.c` |

CI repairs in the same batch: the Config Studio catalog now records the Mac
`gamedate` default, and the `fixes13` level-script, script-load and
corpse-tree fixtures extract the native `G_InitGame`/`G_ShutdownGame` that
`af7d4c6` introduced (the corpse fixture now follows the Mac shutdown contract
already asserted by `tests/lp64/game/main_shutdown.c`).

Ledger entries keep `unchecked` status with evidence notes: these are partial
fixes, not complete function reconciliations.

## Leads not taken

- `XAnimCalcMakeTimeLocal` clamps a finished animation to the penultimate key;
  Linux `XAnim_SetTime` leaves `frameIndex == frameCount` with lerp 0. Our
  native bounds guards in the time-index helpers interact with this, so it
  belongs in the xanim module reconciliation against the Mac code.
- cod2-wasm items not yet verified: `cg_draw_mp.c` vote flag test (`0x10` vs
  `EF_VOTED`), `cg_newDraw_mp.c` `pulseMags`, `cg_hudelem_mp.c` colour fade
  direction, `RB_DeriveEntityLights` cap, `CG_mg42_DoControllers` self-lerp,
  `G_ParseEntityField` `F_MODEL`, scoreboard ping column, `FxScheduler` roll
  mask and `Emitter_UpdateEmitFx` arguments. Details:
  `_agents/research/2026-11-04-cod2wasm.md` (machine-local).
- Not observable in the current replay oracle: its recording has no shooter
  hitting a moving target and no killcam, so the anti-lag and archive fixes
  need a new recording to show at runtime.
