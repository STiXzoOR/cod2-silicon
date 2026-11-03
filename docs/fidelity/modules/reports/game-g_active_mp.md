# game/g_active_mp module reconciliation

Reconciliation updated 2026-11-03; implementation baseline `a604676`.

Function reconciliation: **24/24 complete — 18 equal, 6 fixed, 0 unchecked**.
Stock and CoD2x client/dedicated builds and 2,000-frame replay runs pass.
Whole-project parity remains open as described below.

18 direct-source Mac exports plus StuckInClient export associated by STABS to com_math.h;19 exports map24 union names. All five Linux-only named helpers are mapped to their complete Mac inline bodies.

Manual complete exported-instruction/control-flow and native/Linux function-body comparison. Source assertions absent in Mac release are not behavioral mismatches. Equal assumes valid engine inputs and normal field/object lifetime; not exhaustive formal verification. Byte equality validates export provenance only, not native semantic equality. The October 26 checkpoint was read-only; November 3 production fixtures and batch review are recorded below.

All19 linked Mac exports instruction bytes were independently validated against original Mach-O: zero mismatches. This validates export provenance only.

## Function decisions

| Function | Status | Mac address | Native counterpart |
|---|---|---|---|
| ClientEndFrame | fixed | 0x001a47a8 | ClientEndFrame |
| ClientEvents | fixed | 0x001a30ec | ClientEvents |
| ClientImpacts | equal | 0x001a2556 | ClientImpacts |
| ClientInactivityTimer | equal | 0x001a26cc | ClientInactivityTimer |
| ClientIntermission | equal | No separate export | G_ClientEndFrameIntermission helper called by ClientEndFrame |
| ClientIntermissionThink | equal | No separate export | ClientThink_real inline button latch |
| ClientThink | equal | 0x001a46c2 | ClientThink |
| ClientThink_real | equal | 0x001a3d00 | ClientThink_real |
| G_AddPlayerMantleBlockage | equal | No separate export | ClientThink_real inline mantle block entity construction |
| G_ClientCanSpectateTeam | equal | 0x001a26b2 | G_ClientCanSpectateTeam |
| G_PlayerController | equal | 0x001a293e | G_PlayerController |
| G_PlayerEvent | equal | 0x001a29aa | G_PlayerEvent |
| G_PlayerStateToEntityStateExtrapolate | equal | 0x001a38b4 | G_PlayerStateToEntityStateExtrapolate |
| G_RunClient | equal | 0x001a45b6 | G_RunClient |
| G_SetClientContents | equal | 0x001a288c | G_SetClientContents |
| G_SetClientSound | fixed | No separate export | ClientEndFrame inline loopSound clear |
| G_SetLastServerTime | equal | 0x001a2832 | G_SetLastServerTime |
| G_TouchTriggers | fixed | 0x001a2e3e | G_TouchTriggers |
| G_UpdateClientInfo | equal | No separate export | G_ClientInfoForEntity and G_UpdateClientInfoModel helpers called by ClientEndFrame |
| GetFollowPlayerState | equal | 0x001a28d2 | GetFollowPlayerState |
| P_DamageFeedback | fixed | 0x001a3366 | P_DamageFeedback |
| SpectatorClientEndFrame | equal | 0x001a2bc6 | SpectatorClientEndFrame |
| SpectatorThink | equal | 0x001a2a08 | SpectatorThink |
| StuckInClient | fixed | 0x001a35d2 | StuckInClient |

### ClientEndFrame (fixed)

All 2,032 exported bytes (0x1a47a8-0x1a4f97) compared. Native fixes now select ps.viewmodelIndex from the session, clear s.loopSound without clearing eventSequence, and copy s.angles2[1] into animation lerpMoveDir. Existing damage/dead-state fixes remain correct. Complete comparison covers model updates before the connected gate, intermission/spectator/respawn returns, contents and movement types, compass expiry, gravity/speed/aim spread, look-at/cursor/damage ordering, connection/health stores, state conversion, collision ejection, compass information, handlers, DObj animation and turret behavior. All three new end-frame regressions failed on the baseline and pass after the fixes in stock and CoD2x, with all nine prior movement cases passing. Stable client-slot lifetime is required across callbacks, including respawn; dedicated debug-drawing omission is an existing diagnostic platform exclusion.

### ClientEvents (fixed)

Whole 136-instruction634-byte Mac function read, all40 jump-table entries decoded directly from binary0x33be00; all23 fall event IDs checked. Commit8cec375 repairs dispatch IDs and max-health field. Native matches ring clipping/dynamic event count, eType fall early exit, binary32 constants loaded from binary0x3262f0/0x3263c0 and truncation, antilag time choice, five script notices, forced-death guards/state/9arguments, default noops. Linux full body also compared; FireWeapon name maps native FireWeaponAntiLag. Existing both-mode fixture and2k replay receipts in F1-server-events report remain applicable; this agent did not rerun tests.

### ClientImpacts (equal)

All Mac blocks checked: capture ent touch before iteration, no touches early return, skip duplicate prior entity indices, script-active reciprocal touch notifications before resolving otherTouch, invoke otherTouch then captured entTouch with third argument1, reload numtouch each iteration. Native and Linux preserve full loop/call behavior.

### ClientInactivityTimer (equal)

All Mac blocks: disabled resets time+60000 and warning; packed forward/right or buttons0x401 reset inactivity seconds*1000; localClient bypass; strict greater expiry drops returns0; strict greater expiry-10000 emits one warning type0 command99 then true. Native and Linux match state/calls/conditions, assuming valid client array member and normal nonoverflow game clock.

### ClientIntermission (equal)

The complete inline block at 0x1a4e1d-0x1a4ec3 matches G_ClientEndFrameIntermission: server flags, damage disable, contents, exact movement/entity flag masks, movement type 5, viewmodel reset, invisible entity type, and ordered configstrings 5/6 from team scores.

### ClientIntermissionThink (equal)

The complete inline block at 0x1a437c-0x1a43b1 matches old/current button updates and rising buttons accumulated into buttonsSinceLastFrame; latched_buttons is untouched. Mac reads sess.cmd.buttons and native reads ucmd->buttons. All three native callers (ClientThink, G_RunClient and ClientSpawn in g_client_mp.c) pass the session command itself, establishing that alias precondition.

### ClientThink (equal)

All Mac instructions: bgs assignment, 28-byte cmd-to-oldcmd copy, GetUsercmd, lastCmdTime level.time, conditional Think_real when nonsynchronous, bgs NULL. Native and Linux same operations/order with sizeof(usercmd_t)=28 and stable entity client across usercmd service.

### ClientThink_real (equal)

All 2,229 exported bytes (0x1a3d00-0x1a45b4) compared against native and Linux. Covers connected gate; command clamps and elapsed/follow exception; 200 ms cap; frozen flag; intermission/spectator/inactivity exits; complete pmove construction; origin/button state; view/weapon structures; scalar msec scaling; sway with constant 1.0; ADS/overlay conditions; weapon writeback; Pmove/mantle; crouch/event times; state conversion; snapped origin/bounds; events/link/triggers; precise origin/yaw; impacts and activation. The normal button latch shares the verified session-command alias precondition. Mac reloads ent->client after Pmove and trigger touches; native uses its saved pointer under the established stable connected-client-slot lifetime. Linux unused shellshock intermediates are absent in Mac and correctly absent natively.

### G_AddPlayerMantleBlockage (equal)

The complete inline block at 0x1a4491-0x1a45a9 matches owner lookup, spawn, parent/owner, contents and clipmask 0x10000, svFlags 0x21, invisible type 5, handler 0x13, owner bounds, origin, linking and level.time + duration + buffer scheduling. Native handle conversion retains its existing LP64 representation.

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

### G_SetClientSound (fixed)

Mac inline store 0x1a4b71 clears entity+0x84 (s.loopSound). ClientEndFrame now clears that field and preserves eventSequence. The complete one-store helper is reconciled. A nonzero sound/event-sequence regression failed before the fix and passes in stock and CoD2x.

### G_SetLastServerTime (equal)

All Mac blocks: clientNum entity lookup, clamp level.time-lastServerTime>=1001 to time-1000, update if >= previous or >=level.time. Native and Linux conditions equivalent; field+0x289c asserted native.

### G_TouchTriggers (fixed)

The complete 686-byte Mac function is reconciled. Successful item pickup at 0x1a3069 bypasses contact testing; only nonitems take SV_EntityContact. Native now preserves that branch. Area and shrunken contact bounds, movement-type exit, handler eligibility, script notices and callback order match. Whole-production-TU ASan/UBSan fixture reproduces item pickup despite rejected contact and covers nonitems, rejected items, missing handlers, inactive scripts and empty queries in stock and CoD2x. Trace-only logging remains an intentional diagnostic.

### G_UpdateClientInfo (equal)

The complete inline block at 0x1a47d1-0x1a4978, including copy branches at 0x1a499f-0x1a49bd and 0x1a4a2d-0x1a4a48, matches the native client-info/model helpers: clientNum indexing, model/session indexes, bounded 64-byte copies, all six attachments, string/tag conversions and changed return. As in Mac, clearing absent attachments does not itself mark the model changed.

### GetFollowPlayerState (equal)

All Mac blocks: PMF_PLAYER0x00800000 gate, false without copying; memcpy0x26a8 then memset0x7a8 length0xf80 and true. Native uses sizeof(playerState_t), established ABI0x26a8; Linux copies same ps and hud.current.

### P_DamageFeedback (fixed)

The complete 620-byte Mac body is reconciled. Native preserves DIVSS angle/360 then MULSS 256 at 0x1a357d-0x1a35a0 before truncation. Real production vectoangles reaches angle bits 0x40e0ffff from direction (1, float bits 0x3dfc98c1, 0), producing Mac yaw 4 where the old folded ratio produced 5; the corresponding negative Z input demonstrates pitch too. Both new regressions fail on a604676 and pass after the fix. Whole-production gameplay/math TU tests also cover dead/no-damage/invalid-health returns, world feedback, minimum kick, damage/spread clamps and event/time/blood writeback in stock and CoD2x.

### SpectatorClientEndFrame (equal)

Whole Mac body read: initial spectator flags/damage/contents/model/gun reset; forced archive search clamp0 and50ms decrement with subtraction offset; fallback cycle and normal archive addition offset; teammask gates; archived memcpy0x26a8 with voted eFlag preserved across HUD update; all three forced/free/follow flag paths; StopFollowing fallback three team permissions. Native and full Linux source match. Normal finite/valid team input preconditions.

### SpectatorThink (equal)

All Mac blocks checked: old/new/button latch, negative forced spectator plus allowed/team/current-follow/melee-change StopFollowing gate, reread buttons after StopFollowing, rising attack takes precedence over rising ADS, PMF_FOLLOW return, pm_type4 speed0/400, zero pmove with copied cmd/tracemask0x800811/handler1, Pmove then origin copy and unlink. Native and Linux match.

### StuckInClient (fixed)

The complete 738-byte Mac export is reconciled. Native now bounds candidates by level.maxclients (0x1a3617), checks r.inuse (0x1a3631), and sets pm_flags 0x200 for both players (0x1a3851 and 0x1a3892), preserving eFlags. Other eligibility, all bounds, radius, two random samples, normalization, speed selection, horizontal push and 300 ms timers match for valid connected-client inputs. Production-TU tests reproduce all three old defects and cover moving/stationary/zero-eject fallback, contents variants, rejection gates, radius equality and unchanged vertical velocity in stock and CoD2x. Real production math is linked; only external rand is controlled.

## Verification and review

The November 3 implementation batch changes only native branches of
`src/PC/game_mp/g_active_mp.c`. Tests compile complete production translation
units with engine flags, assertions enabled, ASan and UBSan. The three new
end-frame regressions and six active-module regression groups fail against the
pre-fix source for their intended assertions; all 12 end-frame groups and all
10 active-module groups pass in stock and CoD2x. Existing player-event tests
also pass in both modes, for 32 case groups per build. Both full function ABI
audits passed (634 stock and 651 CoD2x translation units, no errors or
mismatches). After regenerating the SDK configuration, the changed translation
unit was checked again with the exact final flags; no interfaces or layouts
changed. The repeated full SDK audit was stopped in favor of this scoped check,
not counted as a second full pass.

One fresh local batch review found no actionable defects. Receipt:
`/tmp/compound-engineering-501/ce-code-review/20261103-175802-3d01167a/review.json`
(machine-local, temporary; also retained as `batch-review.json` in the
private evidence directory). It covers the production/test diff, not overall
project acceptance. No external review or five-lens cleanup was performed.

Both source-specific legacy checks pass all 10 inactive configurations against
`a604676`. The broad `tools/abi/legacy.py` also inspects changed test fixtures
and reports their intentional edits as five mismatches; it reports no production
source mismatch. These are preprocessing checks, not i386 object identity.

Private evidence is under `CoD2-native-decomp/g-active-20261103.noindex` in the
user's Application Support directory. It contains before/after fixture logs,
ABI/build logs and the subsequent replay comparison. The initial integrated
build encountered stale CMake SDK/header-search paths; local build configuration
was regenerated with an explicit Xcode SDK. No project source or oracle was
changed to bypass that failure. Both fresh configurations build their client
and dedicated targets. The saved original replay was reused without modifying
or rerunning the oracle; its command SHA, seed and all 28 IWD hashes agree with
the new native runs.

The new stock replay is identical to the previous native replay in all recorded
state hashes and raw player fields. The new CoD2x replay is identical to stock
on this corpus. Neither result makes the original-server comparison pass:
first raw velocity divergence is still frame 102, client 0, component 1
(`c390e697` original versus `c390e696` native); first quantized-state divergence
is frame 116. Entity counts, movement type, health, weapon and weapon state have
zero differences over all 2,000 frames. Movement flags, view angles and movement
timer retain their earlier differences. These remain whole-project acceptance
work; no replay parity is claimed. The new field, item and collision regressions
are established by the focused fixtures, not by coverage of those cases in this
replay corpus.

## Estimate boundary

This is the first module with no unchecked functions, but its original
comparison work was not fully timed. The October 26 sample closed five mostly
small functions in 380 seconds using earlier evidence; November 3 completes the
remaining ten with repairs and validation. Neither sample establishes a full
project throughput rate. Reused historical evidence and local toolchain repair
must be distinguished from newly reconciled function work.

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
