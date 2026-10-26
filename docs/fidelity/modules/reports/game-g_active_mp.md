# game/g_active_mp module reconciliation

Checkpoint 2026-10-26T12:15:16.101465+00:00; canonical baseline `36f4fd5`.

Status: **in progress**. {'total': 24, 'equal': 13, 'fixed': 1, 'intentional': 0, 'unchecked': 10}

18 direct-source Mac exports plus StuckInClient export associated by STABS to com_math.h;19 exports map24 union names. Five Linux helpers still need inline mapping.

Manual complete exported-instruction/control-flow and native/Linux function-body comparison. Source assertions absent in Mac release are not behavioral mismatches. Equal assumes valid engine inputs and normal field/object lifetime; not exhaustive formal verification. Byte equality validates export provenance only, not native semantic equality. No builds or execution done by this agent.

All19 linked Mac exports instruction bytes were independently validated against original Mach-O: zero mismatches. This validates export provenance only.

## Function decisions

| Function | Status | Mac address | Native counterpart |
|---|---|---|---|
| ClientEndFrame | unchecked | 0x001a47a8 | ClientEndFrame |
| ClientEvents | fixed | 0x001a30ec | ClientEvents |
| ClientImpacts | equal | 0x001a2556 | ClientImpacts |
| ClientInactivityTimer | equal | 0x001a26cc | ClientInactivityTimer |
| ClientIntermission | unchecked | No separate export | G_ClientEndFrameIntermission helper called by ClientEndFrame |
| ClientIntermissionThink | unchecked | No separate export | ClientThink_real inline button latch |
| ClientThink | equal | 0x001a46c2 | ClientThink |
| ClientThink_real | unchecked | 0x001a3d00 | ClientThink_real |
| G_AddPlayerMantleBlockage | unchecked | No separate export | ClientThink_real inline mantle block entity construction |
| G_ClientCanSpectateTeam | equal | 0x001a26b2 | G_ClientCanSpectateTeam |
| G_PlayerController | equal | 0x001a293e | G_PlayerController |
| G_PlayerEvent | equal | 0x001a29aa | G_PlayerEvent |
| G_PlayerStateToEntityStateExtrapolate | equal | 0x001a38b4 | G_PlayerStateToEntityStateExtrapolate |
| G_RunClient | equal | 0x001a45b6 | G_RunClient |
| G_SetClientContents | equal | 0x001a288c | G_SetClientContents |
| G_SetClientSound | unchecked | No separate export | No explicit native loopSound clear found; inspect Mac ClientEndFrame inline store before classifying. |
| G_SetLastServerTime | equal | 0x001a2832 | G_SetLastServerTime |
| G_TouchTriggers | unchecked | 0x001a2e3e | G_TouchTriggers |
| G_UpdateClientInfo | unchecked | No separate export | G_ClientInfoForEntity and G_UpdateClientInfoModel helpers called by ClientEndFrame |
| GetFollowPlayerState | equal | 0x001a28d2 | GetFollowPlayerState |
| P_DamageFeedback | unchecked | 0x001a3366 | P_DamageFeedback |
| SpectatorClientEndFrame | equal | 0x001a2bc6 | SpectatorClientEndFrame |
| SpectatorThink | equal | 0x001a2a08 | SpectatorThink |
| StuckInClient | unchecked | 0x001a35d2 | StuckInClient |

### ClientEndFrame (unchecked)

Known partial fixes include takedamage and dead tagInfo state. New mismatch: native ps.damageCount assignment from sess.viewmodelIndex vs Mac0x1a4a78-7e writes ps.viewmodelIndex at0xe4. Full function remains open.
Repair wrong viewmodel field, complete all remaining blocks including embedded helpers, verify/report as module batch.

### ClientEvents (fixed)

Whole 136-instruction634-byte Mac function read, all40 jump-table entries decoded directly from binary0x33be00; all23 fall event IDs checked. Commit8cec375 repairs dispatch IDs and max-health field. Native matches ring clipping/dynamic event count, eType fall early exit, binary32 constants loaded from binary0x3262f0/0x3263c0 and truncation, antilag time choice, five script notices, forced-death guards/state/9arguments, default noops. Linux full body also compared; FireWeapon name maps native FireWeaponAntiLag. Existing both-mode fixture and2k replay receipts in F1-server-events report remain applicable; this agent did not rerun tests.

### ClientImpacts (equal)

All Mac blocks checked: capture ent touch before iteration, no touches early return, skip duplicate prior entity indices, script-active reciprocal touch notifications before resolving otherTouch, invoke otherTouch then captured entTouch with third argument1, reload numtouch each iteration. Native and Linux preserve full loop/call behavior.

### ClientInactivityTimer (equal)

All Mac blocks: disabled resets time+60000 and warning; packed forward/right or buttons0x401 reset inactivity seconds*1000; localClient bypass; strict greater expiry drops returns0; strict greater expiry-10000 emits one warning type0 command99 then true. Native and Linux match state/calls/conditions, assuming valid client array member and normal nonoverflow game clock.

### ClientIntermission (unchecked)

Linux-only named definition; prove complete inline Mac body and native counterpart mapping before closure.

### ClientIntermissionThink (unchecked)

Mac inline body0x1a437c–0x1a43b1 reads sess.cmd.buttons atclient+0x26cc; Linux same. Native ClientThink_real intermission block reads ucmd->buttons. Normal callers alias ucmd with sess.cmd, but semantic precondition must be established before equal.
Linux-only named definition; prove complete inline Mac body and native counterpart mapping before closure.

### ClientThink (equal)

All Mac instructions: bgs assignment, 28-byte cmd-to-oldcmd copy, GetUsercmd, lastCmdTime level.time, conditional Think_real when nonsynchronous, bgs NULL. Native and Linux same operations/order with sizeof(usercmd_t)=28 and stable entity client across usercmd service.

### ClientThink_real (unchecked)

Whole-function semantic comparison pending.

### G_AddPlayerMantleBlockage (unchecked)

Linux-only named definition; prove complete inline Mac body and native counterpart mapping before closure.

### G_ClientCanSpectateTeam (equal)

All 10 Mac instructions: load noSpectate+0x2740, arithmetic shift by team, XOR1 AND1; native boolean inverse bit test and Linux mask test equivalent for valid team enum values. No calls or mutations.

### G_PlayerController (equal)

All Mac instructions: clientInfo index from s.clientNum, set bgs=level_bgs, capture level.frametime before serverDObj lookup, call controllers(obj,entityState at entity offset0,partBits,ci,frametime). Native preserves order/arguments; Linux self pointer equivalent offset0. Native typed array stride preserves LP64 layout.

### G_PlayerEvent (equal)

All Mac blocks: subtract0x9e unsigned bound0x11, select mask0x20007, call recoil with ps, vGunSpeed+0x288c, temporary3floats; other IDs no-op. Native exact arithmetic and selector; Linux four symbolic fire events correspond IDs0x9e,0x9f,0xa0,0xaf.

### G_PlayerStateToEntityStateExtrapolate (equal)

Whole Mac body1100bytes read: trajectory copies/type/time50ms; movementDir and flags; bounded entity event ring and old-event replay with byte truncation, recoil mask and single-event filtering; weapon/ground widths; player/invisible choice; six CVTTSS2SI snaps; animations/client/turret/dead/ADS flags; lean; prone lerp clamp/inversion and normalized torso/waist output. Native matches. Linux source SnapVector macro com_math.h60 also truncates; actual Linux oracle implementation behavior is a separate comparison. Valid finite vectors and nonoverflow event sequence preconditions.

### G_RunClient (equal)

All Mac blocks: synchronous command time and Think first; noclip early return; linked chooses session dead7 else1, fixedlink mode2, origin/angle, both trajectories1, link and copy origin; unlinked decrements types1/7. Native and Linux same valid-state behavior and call order. Referenced entity client remains stable through linking services.

### G_SetClientContents (equal)

All Mac blocks: noclip+0x27ac then ufo+0x27b0 clear contents; sessionState+0x26a8==1 clears; otherwise contents+0x11c=0x02000000. Native and Linux match all branches.

### G_SetClientSound (unchecked)

Linux one-statement function maps Mac ClientEndFrame inline0x1a4b71 store entity+0x84=0 (s.loopSound). Native wrongly clears s.eventSequence at corresponding point. Need correct store before closing inline helper and containing function.
Repair/verify described discrepancy within active module batch.

### G_SetLastServerTime (equal)

All Mac blocks: clientNum entity lookup, clamp level.time-lastServerTime>=1001 to time-1000, update if >= previous or >=level.time. Native and Linux conditions equivalent; field+0x289c asserted native.

### G_TouchTriggers (unchecked)

Whole686-byte Mac function read. Native adds SV_EntityContact after successful item pickup test. Mac eType3 successful BG_PlayerTouchesItem bypasses contact query at0x1a3069→306d; only nonitem path0x1a303f→2fae tests SV_EntityContact. Linux else branch corroborates. Other branch/callback order matches; gated native debug trace intentional diagnostic only. Needs item-path fix and proof.
Repair/verify described discrepancy within active module batch.

### G_UpdateClientInfo (unchecked)

Linux-only named definition; prove complete inline Mac body and native counterpart mapping before closure.

### GetFollowPlayerState (equal)

All Mac blocks: PMF_PLAYER0x00800000 gate, false without copying; memcpy0x26a8 then memset0x7a8 length0xf80 and true. Native uses sizeof(playerState_t), established ABI0x26a8; Linux copies same ps and hud.current.

### P_DamageFeedback (unchecked)

Whole exported620-byte function and native/Linux bodies read. All branches/state/calls align except native angle*(256/360) folds Mac DIVSS angle/360 followed by MULSS256 (0x1a357d–3589 and0x1a359c–35a0). Binary32 model angle0x40e0ffff=7.031249523162842 gives Mac4/native5 after truncation;96 near-boundary samples found. Need preserve operation order and fixture, and establish vectoangles output reachability. No whole-function closure.
Repair/verify described discrepancy within active module batch.

### SpectatorClientEndFrame (equal)

Whole Mac body read: initial spectator flags/damage/contents/model/gun reset; forced archive search clamp0 and50ms decrement with subtraction offset; fallback cycle and normal archive addition offset; teammask gates; archived memcpy0x26a8 with voted eFlag preserved across HUD update; all three forced/free/follow flag paths; StopFollowing fallback three team permissions. Native and full Linux source match. Normal finite/valid team input preconditions.

### SpectatorThink (equal)

All Mac blocks checked: old/new/button latch, negative forced spectator plus allowed/team/current-follow/melee-change StopFollowing gate, reread buttons after StopFollowing, rising attack takes precedence over rising ADS, PMF_FOLLOW return, pm_type4 speed0/400, zero pmove with copied cmd/tracemask0x800811/handler1, Pmove then origin copy and unlink. Native and Linux match.

### StuckInClient (unchecked)

Separate Mac export0x1a35d2 discovered under STABS PC/universal/com_math.h, not inline. Whole738-byte function read. Native loop uses level.num_entities vs Mac level+0x1e4 maxclients; native ent->takedamage vs Mac entity+0xfc r.inuse; native sets both ps.eFlags0x200 vs Mac client+0xc pm_flags0x200. Linux corroborates all three. Remaining bounds/radius/RNG/normalization/speed/push blocks compared, subject to binary32 operations and valid engine inputs. Needs fixes plus regression fixture.
Repair/verify described discrepancy within active module batch.

## Estimate boundary

Initial work start clock was not captured. Timed checkpoint began12:09:13UTC with9 closures; current checkpoint has14. These include previously tested ClientEvents and several simpler bodies. Do not extrapolate that rate to unchecked large functions or the full inventory. This module remains open, with concrete defects in4 functions and additional large/inlined functions awaiting full comparison.

## Verification and review

Read-only reconciliation: no builds, VM operations, production edits, cleanup passes or external reviews. Parent owns one batch review. The initial candidate findings are not fixed statuses.

## Frozen handoff checkpoint

Stopped on parent instruction to preserve the user usage reserve. Frozen at 2026-10-26T12:15:33.981034+00:00. Timed interval since12:09:13.867404UTC: 380.1 seconds;5 additional functions closed, now14/24 closed and10 unchecked. No production fixes were made. Do not interpret source diagnoses as module completion.

## Fresh original-server replay after the dead-state fix

After `36f4fd5`, the restored SSH agent was used to run the unchanged original
server instrumentation on the VM with the frozen command stream. The original
anchor checksum passed; the new 2,000-frame original trace matches the saved
original trace under the unchanged quantized-state and raw-player comparator.
Seed, all 28 IWD hashes and command-stream SHA agree with the native run.
Against the corrected native replay, entity counts, `pm_type`, health, weapon
and weapon state match throughout. The first raw velocity difference remains
frame 102; complete replay parity still fails. Private results are under
`F1-dead-state-integration.noindex/original-fresh/` and the adjacent comparison
logs. No keys or authentication configuration were changed.
