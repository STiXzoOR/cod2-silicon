# game/g_client_mp module reconciliation

Updated 2026-11-03; implementation baseline `a94ecc4`.

Function comparison and focused validation: **11/11 complete — 5 equal, 6 fixed**.
One independent local batch review found no actionable defects. Integrated replay acceptance is complete within the limits below.
Ten Mac exports cover the eleven union entries, including both inline name-cleaning bodies.

Mac 1.3 is authoritative; Linux source is a read-only behavioral reference.
All function bodies were compared, not merely their names or exported bytes.
Equality assumes valid engine inputs, normal finite arithmetic/nonoverflow clocks,
and the documented initialized globals and caller domains. No reference source
or binary payload is added to Git.

| Function | Status | Mac address |
|---|---|---|
| ClientBegin | equal | 0x001928ae |
| ClientCleanName | equal | inline in ClientUserinfoChanged |
| ClientConnect | fixed | 0x00193a5e |
| ClientDisconnect | fixed | 0x0019291c |
| ClientSpawn | equal | 0x001930a6 |
| ClientUserinfoChanged | fixed | 0x00193654 |
| G_BroadcastVoice | fixed | 0x00192d0c |
| G_GetNonPVSFriendlyInfo | fixed | 0x00192a7e |
| G_GetPlayerViewDirection | equal | 0x00192894 |
| G_GetPlayerViewOrigin | fixed | 0x00193462 |
| SetClientViewAngle | equal | 0x00192e52 |

## ClientBegin (equal)

Complete 110-byte body compared: connected state 2, movement type 4, CalculateRanks then begin notification with zero arguments. Optional native DBGSPAM output is diagnostic instrumentation.

## ClientCleanName (equal)

Both complete Mac inline loops at 0x193738-0x1938d2 and 0x1938fa-0x193a58 match the native reachable loops: leading-space removal, atomic color escapes, black suppression, trailing caret, three-space limit, 31-byte output limit and UnnamedPlayer fallback. Both callers use 32-byte names. The unused native static CleanName is not claimed as a general arbitrary-outSize implementation.

## ClientConnect (fixed)

Whole 0x193a5e-0x193c47 body compared. Restores case-insensitive none password sentinel from Mac literal 0x243764 and branch 0x193c15; ordinary passwords still compare case-sensitively. Client/clientInfo clearing with preserved XAnimTree, validity, narrowed script ID, spectator state, entity setup, userinfo/local bypass, rejection cleanup and success callback/ranks ordering match. Native null-dvar fallback is outside initialized-server inputs. Both-mode fixtures reject wrong ordinary passwords and accept exact passwords/none/NoNe.

## ClientDisconnect (fixed)

Complete body matches after restoring second menuresponse argument -1 from Mac string 0x2dbd9c at 0x192a4c. Script-active gates, spectator follower loop, HUD/script cleanup, entity free, connected reset, entire client-state clear and rank update match. Production-TU fixture checks both notice strings and resulting cleanup state; old string assertion fails on baseline.

## ClientSpawn (equal)

Whole 0x1930a6-0x193461 body compared: turret/player predicates, unlink sequence, classname/clipmask/server/damage/contents/handler/flags, entity bounds, complete session save/restore around client clearing, spawn count/max health, eFlags mask/toggle, client indices/usercmd, movement bounds, view height/lerp, origin/respawn flag/view angles, inactivity/buttons/times, spawning flag around end-frame/think and final entity-state conversion. Mac level.gentities and native g_entities alias by game initialization. Typed sizes preserve native layouts.

## ClientUserinfoChanged (fixed)

Whole 0x193654-0x193a5c body compared. Restores cg_predictItems key (Mac literal 0x2d3934, lookup at 0x1936c9) instead of handicap. Common scoreboard copy now explicitly reads sess.cs.name, including the manual-name branch that jumped past a different local initializer. Userinfo validation/fallback, local-client flag, boolean prediction, connected/manual branch, both name-cleaning loops, staged/current names, client number and team updates match. Conflicting prediction keys and actual current/staged/scoreboard-name assertions fail before the fix and pass in both modes.

## G_BroadcastVoice (fixed)

Complete function compared. Mac 0x192d71 checks INTERMISSION (3), not SPECTATOR (2), for the recipient team bypass. At 0x192d3a-0x192d4c unequal states proceed only when one is DEAD and deadChat permits it; unequal nondead states skip. Native now matches both. Timestamp, 64-client scan, validity, global/team/free-team rule, local echo, mute, subscription and queue arguments match. Expanded production relay covers all 16 state pairs with deadChat off/on, team bypass/denial, codec playback and existing gates in stock/CoD2x. This corrects the earlier WS119 spectator interpretation; it is not an intentional CoD2x policy.

## G_GetNonPVSFriendlyInfo (fixed)

Whole function compared. Mac 0x192c9a scales Y only for scaleX<scaleY, and 0x192cf2 scales X only for scaleY<scaleX; native now leaves equal scales untouched. Candidate1 at (-1042.5,-1042.5), viewer zero and yaw zero must encode 0x00000001, not old 0x00008001. Yaw packing now uses unsigned 32-bit shifting, matching SHL24 at 0x192c3f and avoiding UBSan failure at ordinary 180-degree yaw. Remaining team/validity/circular scan/snapshot gates, rounding/clamps, bit fields and binary float constants match. Previous-entity inputs are 0..63 or1023 as established by ClientEndFrame; negative-modulo normalization is outside that domain.

## G_GetPlayerViewDirection (equal)

Complete 26-byte function loads the entity client and passes its viewangles to AngleVectors with all three output pointers unchanged.

## G_GetPlayerViewOrigin (fixed)

Complete function compared. Restores AddLeanToPosition view-roll16/lean-distance20 arguments from Mac 0x1935ea/0x1935f2, plus original turret tag failure text at 0x193637. Turret selection/tag lookup, eye-height sum, bob calls and arguments, right-vector displacement and eight-unit minimum height match. Regression links actual production AddLeanToPosition and AngleVectors and checks final lean displacement, with normal/minimum-height/turret/error controls.

## SetClientViewAngle (equal)

Entire 596-byte function compared. Existing native prone-bit and no-turret predicate matches; yaw +/-45 and pitch -15/+45 delta bounds, overflow normalization, short-angle conversion and command subtraction, and copies to entity/current player angles all match.

## Verification and limits

`client_module.py` compiles complete production client, shared utility and math
translation units. Five baseline cases fail: compass yaw (UBSan signed shift),
equal-scale compass encoding, lean displacement, turret diagnostic and disconnect
notice. Its two control groups pass before and after. All seven groups pass
afterward in stock and CoD2x.

`client_lifecycle.py` compiles the complete client translation unit. Both-mode
baseline runs fail none/NoNe password acceptance, both independent prediction-key
cases and manual-name handling; ordinary correct/incorrect password controls pass.
The manual-name fixture was strengthened to compare observable output rather than
source-pointer identity, and that output assertion also fails on the baseline.
All seven groups pass afterward in both modes.

The existing production voice relay fixture first passes codec/cadence controls
then fails the corrected spectator-filter expectation against `a94ecc4`. Both-mode
updated runs pass, including all 4x4 session-state pairs with deadChat off/on,
intermission and spectator team rules, free-team talkers, mute, subscription,
local echo, packet serialization and decoding/playback. This supersedes the game
routing claim in `docs/macos-port/reports/WS119-voice-relay.md`.

Stock and CoD2x client and dedicated builds pass. The changed translation unit
passes scoped ABI audits in both modes. Both source-specific legacy checks pass
all ten inactive configurations against `a94ecc4`; no i386 binary identity claim
is made. No public signatures or layouts changed.

Machine-local evidence:
`/Users/stix/Library/Application Support/CoD2-native-decomp/g-client-20261103.noindex/`.
Baseline and fixed fixture logs, build/ABI results, review receipt and native replay
comparisons are retained there. Original-server evidence remains unchanged.

Both stock and CoD2x replay runs completed all 2,000 records with two clients,
the same command SHA, seed and IWD set. Both state hashes and all recorded player
fields match the preceding native build exactly and match each other. Against
the saved validated original, the existing first differences remain: raw velocity
frame 102, quantized state frame 116, movement flags frame 652, view angles
frame 776 and movement time frame 1803. Entity counts, movement type, health,
weapon and weapon state match throughout. Whole-project parity is still open.

The single fresh local batch review checked all six corrections against the Mac
exports, the equal functions, callers, fixture evidence and legacy guards. It
reported no findings. Its receipt is retained as `batch-review.json` in the
evidence directory. No external or separate per-fix review was used.
