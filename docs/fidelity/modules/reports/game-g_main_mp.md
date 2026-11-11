# game/g_main_mp module reconciliation

Updated 2026-11-03; baseline `3075413`.

**34/34 reconciled: 20 equal, 12 fixed, two intentional differences.** Complete Mac bodies and the Linux helper union were compared. The corrections restore game initialization and shutdown order, objective visibility, one-shot intermission scoreboards, vote-clock arithmetic, callback typing and diagnostic metadata. Documented CoD2x extensions remain.

## Behavioral corrections

Cold starts now prepare scripts and 72 animation trees before constants and entity spawning. Restarts preserve those scripts and trees. Registration occurs only on cold starts or restarts without saved persistence; duplicate setup and registration calls are removed. Initialization also resets the winner configstring and script file handle, keeps the initialization flag set through script startup, then clears the active animation context before saving registries. Log serverinfo uses the original 1,024-byte bound. Game dates and the initialization separator match the Mac literals.

The existing split script-loader adapter remains: its caller begins and ends loading. PostCompile now runs inside GScr_LoadScripts before publishing class maps and fields. This narrow dependency correction is included in this batch; the rest of game/g_scr_main_mp remains unchecked.

Active global objectives and objectives belonging to the viewer's team are copied; hidden objectives clear only their state. Intermission scoreboard requests clear after the send loop, including when a client receives a message. Vote deadline tests use the sign bit of a wrapping 32-bit difference, and scheduled vote times wrap without signed overflow. Think callbacks use their actual void signature; callers discard the reconstructed wrapper return. Tree shutdown clears owner pointers while preserving the separate corpse client-info view.

## Complete-body decisions
### CalculateRanks (equal)

Complete Mac 1acb42, 0x1adb42 (171 bytes). Clears both counters, scans level.maxclients, appends non-disconnected indices, counts only connected non-spectators as voting, then qsort(base,count,4,SortRanks), then sets update flag 1. rank-counts verifies indices/counters/qsort args and zero-client behavior.

### CheckVote (fixed)

Complete Mac 1adb2a, 0x1aeb2a..0x1aec93 (362 bytes). Execute-time strict signed JL matches. Deadline CMP at 0x1aeb53 / JS at 0x1aeb59 tests wrapped subtraction sign, unlike native relational <. Passed schedule ADD 0xbb8 at 0x1aec52 wraps, while both native time+3000 expressions incur signed UB with actual Darwin flags. Both build modes baseline 3075413 fail four dedicated rollover cases; remaining vote tests pass. All majority/abstention thresholds, float ceilf pipeline, command/config IDs/literals and callback/store ordering match. Root applied all three COD2_X64 guarded corrections; complete patched body reread and votes-green-stock/votes-green-codx pass all 11 case groups each with ASan/UBSan, including four baseline-red rollover groups.

### DebugDumpAnims (fixed)

Linux helper mapped to complete Mac G_RunFrame 0x1af77f..0x1af7bc; original literal at image 0x2e0580 is server newline. Gate/index/call match, banner does not. Root native-only production patch inspected; frame-green-stock and frame-green-codx each pass all 7 cases for client and dedicated (28 outcomes total). Baseline objective/scoreboard/banner cases fail in both modes and targets; callback typing correction is static proof, not sanitizer-red evidence.

### ExitLevel (intentional)

Complete Mac 1accea, 0x1adcea (223 bytes), stock body equal: append map_rotate, clear axis/allies scores, zero connected-client scores up to g_maxclients, second pass sets connected -> connecting, then exact log literal. Native COD2_CODX hook intentionally delegates queueing and returns before mutations on rejection, documented in WS81-cod2x-stop-gametype.md and WS100-transition-rollback.md; actual GScr_Cod2xQueueExitLevel confirms success-conditioned queue and match-end. exit-level/exitrejected fixture covers boundary contract in both modes (real queue implementation covered by existing cod2x stop_gametype suite; not re-executed here).

### G_AddDebugString (intentional)

Complete 48-byte Mac client body at0x1aee86 forwards xyz,color,scale,text and fromServer1 to CL_AddDebugString, matching native client. Native Apple dedicated deliberately omits function and its GScr_print3d call; Linux dedicated helper is empty. Rendering-only omission is a platform deviation, not a client-body difference.

### G_CreateDObj (equal)

Whole0x1ada80–1ada8f:uint16numModels,tailforward fourarguments,ignoreci;nativevoidcallback adapter documented WS12.

### G_FreeAnimTreeInstances (fixed)

Native now frees every nonNULL client and corpse owner, clears only owners; exact complete Mac region. Null-owner/stale-ci view fixture validates both groups, 4 freeScripts cases green in each mode (baseline red).

### G_FreeEntities (equal)

Complete0x1aeee3–1aef54:dynamicnum_entities loop,inuse,world1022,clearcount/head/tail;typedstrides agree.

### G_GetClientArchiveTime (equal)

Complete 34-byte Mac body at 0x1ada16 uses client stride10404 and archiveTime offset0x26b4, confirmed STABS sess9896+12; native typed load matches.

### G_GetClientScore (equal)

Complete 34-byte Mac body at 0x1ad9f4 uses 10404-byte client stride and score offset0x26b8. Retail STABS confirms sess offset9896 plus score16; native typed indexing matches valid client indices.

### G_GetClientState (equal)

Complete 34-byte Mac body at 0x1ada5e returns client+0x2748, confirmed STABS sess9896+cs160; native address expression preserves LP64 adaptation.

### G_GetFogOpaqueDistSqrd (equal)

Complete 11-byte Mac body at 0x1ad9e8 returns the float field at level+0x1ddc; native typed float access matches.

### G_GetSavePersist (equal)

Complete 10-byte Mac body at 0x1ad9d0 loads level.savepersist; native typed access matches.

### G_InitGame (fixed)

Full Mac CFG compared again with new native body: required sequence restored, script PostCompile moved into dependency before classmap; 7 ASan/UBSan cases pass stock and CoD2x (init-green-stock2/codx2). Explicit scoped differences: Sys_DiffSeed opt-in harness, CoD2x banner suppression, typed native data/callbacks. No unmatched init behavior retained.

### G_LoadAnimTreeInstances (equal)

Mac0x1aea87–1aeae3 alloc64clienttrees then8corpseownertrees;native typedloops match; BG_LoadAnim assigns both native treealiases.

### G_LocationalTrace (equal)

Complete 83-byte Mac body at0x1aeda0 forwards results/start/zero/zero/end/pass/mask,locational1,caller priority,staticmodels1 to SV_Trace. Native zero-vector values and argument identity match.

### G_LocationalTracePassed (equal)

Complete 77-byte Mac body at0x1aedf4 forwards start/zero/zero/end/pass,owner1023,mask,locational1,staticmodels1 to SV_TracePassed and propagates result.

### G_LogPrintf (equal)

Complete 251-byte Mac body at 0x1adbee: logfile-zero early return, vsnprintf size1024, signed millisecond/second/minute decomposition, Com_sprintf size1024, strlen then FS_Write. Binary literal at0x2e00b4 is %3i:%i%i %s. Native matches; valid format/varargs and terminated library output assumed.

### G_RegisterDvars (fixed)

All 57 Mac call sites (56 base + conditional dedicated) compared; Aug30 2006 literal restored. 2 registration cases pass stock and CoD2x; previous revision fails date. Intentional CoD2x2 extra dvars documented WS65/66.

### G_RunFrame (fixed)

Complete Mac 1ae284 body reviewed; clocks, triggers, animations, dispatch, HUD and ordered tail match on native initialized domain. Objective predicate, one-shot intermission flag and dump string diverge; intentional gated CoD2x broadcast and debug-only instrumentation are retained. See frame.md. Root native-only production patch inspected; frame-green-stock and frame-green-codx each pass all 7 cases for client and dedicated (28 outcomes total). Baseline objective/scoreboard/banner cases fail in both modes and targets; callback typing correction is static proof, not sanitizer-red evidence.

### G_RunFrameForEntity (fixed)

Complete Mac 1ae0ba body reviewed; processedFrame, masks, expiry endpoints and dispatch precedence match. Inlined G_RunThink_core shares invalid callback function typing. Root native-only production patch inspected; frame-green-stock and frame-green-codx each pass all 7 cases for client and dedicated (28 outcomes total). Baseline objective/scoreboard/banner cases fail in both modes and targets; callback typing correction is static proof, not sanitizer-red evidence.

### G_RunThink (fixed)

Complete Mac 1adc94 body reviewed; exact due-time/error/callback contract matches. Registered callback bodies return void; native int callback cast is undefined C behavior and all call sites discard returns. Root native-only production patch inspected; frame-green-stock and frame-green-codx each pass all 7 cases for client and dedicated (28 outcomes total). Baseline objective/scoreboard/banner cases fail in both modes and targets; callback typing correction is static proof, not sanitizer-red evidence.

### G_SetClientArchiveTime (equal)

Complete 37-byte Mac body at 0x1ada38 uses same client stride and archiveTime field, stores supplied time unchanged; native typed store matches.

### G_SetSavePersist (equal)

Complete 13-byte Mac body at 0x1ad9da stores its integer argument to level.savepersist; no boolean normalization in either body.

### G_ShutdownGame (fixed)

Complete Mac outer CFG and tree-free dependency now match; 10 ASan/UBSan cases pass per mode including freeScripts/savepersist/active matrix, null owners, stale corpse ci view, world entity, logfile and scriptIO handle0/buffer. CoD2x banner suppression intentional.

### G_SightTrace (equal)

Complete 68-byte Mac body at0x1aee42 forwards hitNum/start/zero/zero/end/pass/owner1023/mask to SV_SightTrace. Native preserves callee return; Linux wrapper is void and callers only require hitNum side effect, so no invented Mac meaningful return contract.

### G_TraceCapsule (equal)

Complete 85-byte Mac body at0x1aecfc forwards caller results/start/mins/maxs/end/pass/mask to SV_Trace with locational0,priorityNULL,staticmodels0; native matches.

### G_TraceCapsuleComplete (equal)

Complete 78-byte Mac body at0x1aed52 forwards start/mins/maxs/end/pass plus owner1023,mask,locational0,staticmodels0 to SV_TracePassed and propagates result.

### G_UpdateHudElemsToClients (equal)

Linux helper mapped to complete Mac G_RunFrame 0x1af67a..0x1af6d9: inuse level.gentities, ent.client, ent.s.number, update=3. Native same for stable initialized maxclients; no helper writes maxclients.

### G_UpdateObjectiveToClients (fixed)

Linux helper mapped to full Mac G_RunFrame 0x1af578..0x1af67a. Active global or matching-team entries fully copy; empty/opposing entries clear only state. Native predicate inverted. Root native-only production patch inspected; frame-green-stock and frame-green-codx each pass all 7 cases for client and dedicated (28 outcomes total). Baseline objective/scoreboard/banner cases fail in both modes and targets; callback typing correction is static proof, not sanitizer-red evidence.

### G_XAnimUpdateEnt (equal)

Linux helper mapped to complete Mac G_RunFrame 0x1af48b..0x1af50e: inuse and 0x2000 gates, notify=1, continuation scripts, recheck predicates. Native same on complete inline region.

### Hunk_AllocXAnimServer (fixed)

Complete Mac9-byte forwarder calls Hunk_AllocLowInternal(size) and returns its pointer. Native void* return now matches Alloc_t callback signature; prior int* return was called through incompatible void* callback casts. Production compile-time type assertion fails baseline, both modes pass actual callback size/pointer/null-return checks. Legacy declaration/definition retained under OFF.

### SendScoreboardMessageToAllIntermissionClients (fixed)

Linux-only row inlined in Mac G_RunFrame 1ae284: flag/maxclient gate 0x1af733..0x1af74a; loop 0x1af82b..0x1af870 checks connected==2 and pm_type==5, calls scoreboard for matching g_entities slot. Loop completion at 0x1af84b jumps unconditionally to flag clear 0x1af74a. Native foundAny condition wrongly retains flag when any message is sent. Parent/main_frame independently confirmed and owns fixture. Root applied COD2_X64 unconditional clear after loop; reread exact patched block and confirmed authority match. Full-frame case execution owned by main_frame; parent must attach that result. Frame worker reports baseline scoreboard failure and current success in stock/CODX client and dedicated: frame-red-stock2/frame-red-codx and frame-green-stock/frame-green-codx. Covers one-shot, next-frame, zero-client and no-eligible-client behavior.

### SortRanks (equal)

Complete Mac 1aca9a, 0x1ada9a..0x1adb40 (167 bytes), all branches compared. Connecting clients precede all team/score decisions (including both connecting -> 1). Two spectators sort by client-address order; one spectator follows a player. Descending signed score then ascending signed deaths, exact ties 0. Native typed stride is the LP64 adaptation. rank-comparator fixture exercises all branches and direct comparator self comparison.

## Verification and limits

Focused tests pass 106 outcomes under ASan/UBSan with assertions enabled: frame 28, votes 22, initialization 14, shutdown 20, registration four, allocator two, and script-load boundary 16. Frame and script-load cases cover both client and dedicated in stock and CoD2x; the other cases cover both modes. Baseline runs reproduce objective, scoreboard, banner, vote deadline/overflow, lifecycle, corpse-view clearing and date failures. The void callback correction is supported by declarations and complete call-site inspection; this Apple sanitizer did not reproduce the mismatched-return-type call.

Frame, init, shutdown, registration and script-boundary fixtures extract complete production bodies after actual-target preprocessing. Votes use the complete production translation unit. These service-seam tests establish bounded contracts; they are not live multiplayer or visual acceptance. The registration fixture asserts selected metadata and call counts; the complete table was also manually compared against all 57 Mac call sites.

Both stock and CoD2x client/dedicated builds pass. Scoped ABI checks pass for both changed translation units in both modes. Both source legacy checks pass all ten inactive configurations; the native allocator declaration now uses the compatible void-pointer return type. No i386 binary identity is claimed.

Both 2,000-frame stock and CoD2x replays exactly match the preceding item-module native trace and each other, including entity counts and all recorded player fields. These runs preceded the final allocator signature correction; final builds, allocator and initialization fixtures validate that type-only change. The final CoD2x binary also passes all 15 existing live transition cases, including veto/approval, a real persistent restart, script-error teardown, fresh-engine recovery and shutdown.

The unchanged original-reference differences begin at velocity frame 102, origin 104, quantized state 116, pm_flags 652, viewangles 776 and pm_time 1803. Entity counts, pm_type, health, weapon and weaponstate match. The replay does not establish objective rendering or exercised voting/intermission branches; focused fixtures cover those contracts.

One fresh local review identified the allocator callback return-type mismatch, corrected and validated within this same review. The completed review has no remaining findings; receipt: `/tmp/compound-engineering-501/ce-code-review/20261103-main-f0388e05/review.json`. Private evidence is in `/Users/stix/Library/Application Support/CoD2-native-decomp/g-main-20261103.noindex/`. Original parity remains open; no whole-project completion claim is made.
