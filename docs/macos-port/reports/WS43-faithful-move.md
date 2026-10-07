# WS43: faithful player movement

Branch `port/faithful-move`; base `port/main` at `664f1a12da05a9abada6b8a6ab4831f2377f502d`.
Evidence: `~/Library/Application Support/CoD2-native-ws43/` (**E** below).

Native walking, bob, footsteps, lean, ladders, landing, slide/step and mantle control flow now follows the Mac 1.3 reference. An independent arm64 movement-unit comparison has zero playerState byte differences over 20,000 randomized frames. Accepted live weapon speeds match the reference scales, and steady local movement intervals show no prediction misses. Third-person models remain incomplete, so visible walking/running legs are unproven. The online reference-server and Linux i386 object gates remain external work.

## Authority and boundaries

I read the full PLAN and faithful-wave common rules first. Authority was the owned Mac 1.3 STABS executable at `~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, its disassembly and scalar constants. The readable 1.3 guide available here is `~/Projects/cod2-native-refs/research-clones/cod2engine/archive/lnxded-1.3/src/bgame/bg_pmove.cpp`; other reference directories contain 1.0 material and were not used as 1.3 authority. Guide source and disassembly stayed outside the repository. No packages, remotes, pushes, PRs or sibling worktrees were changed.

All production changes use `defined(COD2_X64)` and preserve original legacy statements. CoD2x's controller movement-direction change and stance-animation gates remain. Established playerState layout, event offsets, REAL_PS_SIZE, acceleration/friction core and bob-factor functions were retained. No CMake or shared header changes were needed.

## Inventory

The table uses `nm -a` and per-symbol disassembly, including direct, indirect and external tail calls. Private details are in `E/reference/callee-inventory.json`. “Faithful on reviewed path; retained” describes the movement/view callee and constant checks; it does not claim exhaustive weapon, renderer or audio validation. Inline paths are listed separately. Native math wrappers and opt-in diagnostics are implementation aids.

| Function | Mac 1.3 address | Classification, before → after | Reference calls / checked behavior |
|---|---|---|---|
| `BG_CheckProneValid` | `0x6a534` | faithful on reviewed path; retained | AngleVectors, indirect, Vec3Normalize, vectopitch, AngleSubtract, AngleNormalize180, Vec3NormalizeTo |
| `BG_CheckProne` | `0x6b252` | faithful on reviewed path; retained | BG_CheckProneValid |
| `PM_trace` | `0x6b608` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_AddTouchEnt` | `0x6b65e` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_AddEvent` | `0x6b6ac` | faithful on reviewed path; retained | BG_AddPredictableEventToPlayerstate |
| `PM_GetEffectiveStance` | `0x6b6ce` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_GroundSurfaceType` | `0x6b6ee` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_GetViewHeightLerpTime` | `0x6b70a` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_SetProneMovementOverride` | `0x6b730` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `BG_GetSpeed` | `0x6b746` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `BG_CheckProneTurned` | `0x6b790` | faithful on reviewed path; retained | AngleDelta, AngleNormalize360Accurate, BG_CheckProne |
| `PM_ShouldMakeFootsteps` | `0x6b8a2` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_ClipVelocity` | `0x6b8ec` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_Accelerate` | `0x6b970` | faithful on reviewed path; retained | Same normalize, inertia and directional acceleration branches; no physics rewrite. |
| `PM_Friction` | `0x6bc62` | faithful on reviewed path; retained | Same Jump_ReduceFriction call; stopspeed 100, ground friction 5.5, flight friction 1.5. |
| `PM_UpdateLean` | `0x6bdce` | stub → restored | Enter 350 ms, leave 280 ms; prone cap .25; other stances .5; capsule trace and inverse lean fraction. |
| `PM_UpdateViewAngles` | `0x6c082` | faithful on reviewed path; retained | Lean, mantle cap and prone yaw/pitch checks retained; real native lean now supplies the view. |
| `PM_UpdatePronePitch` | `0x6cafe` | simplified → restored | BG_CheckProne and ground-pitch targets; approach 70 degrees/s. |
| `PM_playerTrace` | `0x6cd96` | faithful on reviewed path; retained | indirect |
| `PM_SetMovementDir` | `0x6ceb8` | simplified → restored; CoD2x retained | Displacement → angle in degrees; original vector/angle helpers; existing CoD2x controller path retained. |
| `PM_AirMove` | `0x6d060` | faithful on reviewed path; retained | PM_Friction, Vec3Normalize, PM_Accelerate, PM_StepSlideMove, PM_SetMovementDir |
| `PM_LadderMove` | `0x6d2b4` | simplified → restored | Jump/AirMove, plane projection, pitch bias .25 and ×2.5; vertical .5, lateral .2, accel 9, adhesion -50. |
| `PM_ViewHeightAdjust` | `0x6d7aa` | faithful on reviewed path; retained | Vec3Normalize, PM_StepSlideMove |
| `PM_CheckDuck` | `0x6e08e` | faithful on reviewed path; retained | Prone trace, animation events, jump slowdown and view-height interpolation retained. |
| `PM_FootstepEvent` | `0x6eab2` | stub → restored | (old+64) XOR (new+64), byte bit 128; surface event bases 1/24/47; ladder trace -31. |
| `PM_Footsteps` | `0x6ed5c` | simplified → restored | Forward run/walk .335/.305; backward .36/.325; crouch .34/.315; prone .25/.24. Animation selection precedes bob; fallback follows it. |
| `PM_GroundTrace` | `0x6fbe8` | simplified landing → restored | Ground branches retained; negative-gravity landing quadratic, 128/300 damage bounds and proportional stun restored. |
| `Pmove` | `0x70c7e` | simplified → restored | See path details below; BG_AnimUpdatePlayerStateConditions, PM_InteruptWeaponWithProneMove, PM_AdjustAimSpreadScale, PM_UpdateViewAngles, AngleVectors, Mantle_ClearHint… |
| `PM_IsBinocularsADS` | `0x7a04a` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_ExitAimDownSight` | `0x7a064` | faithful on reviewed path; retained | PM_AddEvent |
| `BG_GetBobCycle` | `0x7a088` | faithful on reviewed path; retained | Verified existing bobCycle → radians; unchanged as requested. |
| `BG_GetVerticalBobFactor` | `0x7a0c8` | faithful on reviewed path; retained | Verified existing standing/crouched/prone float amplitudes and max clamp; unchanged. |
| `BG_GetHorizontalBobFactor` | `0x7a1b2` | faithful on reviewed path; retained | Verified existing float amplitude and clamp; unchanged. |
| `PM_WeaponAmmoAvailable` | `0x7a8d2` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_ReloadClip` | `0x7aba4` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_Weapon_AllowReload` | `0x7ac60` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_AdjustAimSpreadScale` | `0x7ad44` | faithful on reviewed path; retained | AngleSubtract |
| `PM_Weapon_ReloadDelayedAction` | `0x7b114` | faithful on reviewed path; retained | PM_ReloadClip, PM_AddEvent |
| `BG_GetViewmodelWeaponIndex` | `0x7b226` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `BG_CalculateWeaponPosition_Sway` | `0x7b5fe` | axis mismatch → fixed | DiffTrack/Angle; yaw × horizontal, pitch × vertical; 60 Hz normalization and ADS interpolation. |
| `PM_UpdateAimDownSightFlag` | `0x7bc60` | faithful on reviewed path; retained | BG_UpdateConditionValue |
| `PM_BeginWeaponChange` | `0x7c110` | faithful on reviewed path; retained | PM_SetProneMovementOverride, PM_AddEvent, BG_AnimScriptEvent |
| `PM_SetReloadingState` | `0x7c3d6` | faithful on reviewed path; retained | PM_AddEvent |
| `PM_BeginWeaponReload` | `0x7c5a2` | faithful on reviewed path; retained | PM_AddEvent, PM_SetReloadingState, BG_AnimScriptEvent |
| `PM_UpdateAimDownSightLerp` | `0x7c762` | faithful on reviewed path; retained | PM_AddEvent |
| `PM_ResetWeaponState` | `0x7ca54` | faithful on reviewed path; retained | PM_AddEvent |
| `PM_InteruptWeaponWithProneMove` | `0x7cade` | faithful on reviewed path; retained | PM_AddEvent |
| `PM_Weapon` | `0x7cbb2` | faithful on reviewed path; retained | See path details below; PM_UpdateAimDownSightLerp, DiffTrack, PM_AddEvent, Mantle_IsWeaponInactive, PM_BeginWeaponChange, BG_AddPredictableEventToPlayerstate… |
| `BG_CalculateWeaponAngles` | `0x7f226` | faithful on reviewed path; retained | GetLeanFraction, BG_CalculateWeaponPosition_GunRecoil_SingleAngle, AngleSubtract |
| `BG_CalculateViewAngles` | `0x8033a` | faithful on reviewed path; retained | GetLeanFraction |
| `Jump_RegisterDvars` | `0x9ccf0` | faithful on reviewed path; retained | Dvar_RegisterFloat, Dvar_RegisterBool |
| `Jump_ClearState` | `0x9cdd2` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Jump_GetStepHeight` | `0x9cde8` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Jump_IsPlayerAboveMax` | `0x9ce4a` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Jump_ActivateSlowdown` | `0x9ce6e` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Jump_ApplySlowdown` | `0x9ce8c` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Jump_ReduceFriction` | `0x9cf32` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Jump_Check` | `0x9cfb2` | simplified ladder reflection → restored | Ground/stance checks retained; ladder reflection uses dot product after flattening and normalization. |
| `Jump_ClampVelocity` | `0x9d2b2` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `PM_SlideMove` | `0x9d318` | simplified edge cases → restored | Five clip planes; duplicate-plane handling; clipping ties; time-limited primal Z restoration. |
| `PM_StepSlideMove` | `0x9dc9c` | simplified edge cases → restored | 18-unit step; threshold <1; client entity <64; actual drop delta; 7.0 bob factor; .19999999 slowdown term. |
| `Mantle_RegisterDvars` | `0x9e618` | faithful on reviewed path; retained | Dvar_RegisterBool, Dvar_RegisterFloat |
| `Mantle_ShutdownAnims` | `0x9e738` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Mantle_ClearHint` | `0x9e748` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Mantle_CapView` | `0x9e758` | faithful on reviewed path; retained | AngleDelta, AngleNormalize360Accurate |
| `Mantle_IsWeaponInactive` | `0x9e814` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `Mantle_CreateAnims` | `0x9e84c` | faithful on reviewed path; retained | XAnimCreateAnims, XAnimBlend, XAnimPrecache, XAnimCreate, XAnimGetAbsDelta, Com_Error |
| `Mantle_GetAnimDelta` | `0x9eb68` | faithful on reviewed path; retained | XAnimGetLengthMsec, XAnimGetAbsDelta, VectorAngleMultiply |
| `Mantle_Move` | `0x9ecdc` | faithful on reviewed path; retained | XAnimGetLengthMsec, Mantle_GetAnimDelta, BG_AnimScriptAnimation, BG_AddPredictableEventToPlayerstate, BG_AnimScriptEvent |
| `Mantle_CheckLedge` | `0x9ef56` | simplified over trace → restored | 60/40/20 ledge search; 16-unit reach; 31-unit over shift then vertical -18 trace. |
| `Mantle_Check` | `0x9f6c6` | inverted wall test → restored | Both startsolid/allsolid reject; otherwise wall mask 0x1000000, radius .1, range 20, angle 60. |
| `CG_DObjGetViewModelTagPos` | `0x1638ea` | faithful on reviewed path; retained | DObjGetBoneIndex, CL_DObjCreateSkelForBone, DObjGetRotTransArray, AxisCopy, MatrixTransformVector43, DObjGetHierarchyBits, DObjCalcAnim, DObjCalcSkel |
| `CG_Viewpos_f` | `0x1d2d50` | faithful on reviewed path; retained | Com_Printf |
| `CG_GetViewFov` | `0x1d33ca` | faithful on reviewed path; retained | BG_GetViewmodelWeaponIndex, BG_GetWeaponDef, BG_IsAimDownSightWeapon |
| `CG_OffsetThirdPersonView` | `0x1d3722` | faithful on reviewed path; retained | AngleVectors, CG_TraceCapsule |
| `CG_CalcViewValues` | `0x1d3b28` | float/int misread → fixed | Float bg_bobMax=8; landing 150/450 ms, step 100 ms; native view-height/lean/bob fields. |
| `CG_InitView` | `0x1d49ba` | faithful on reviewed path; retained | CG_PredictPlayerState, CL_ResetSkeletonCache, CG_UpdateViewWeaponAnim, CG_CalcViewValues, CL_FX_AdjustCamera, FX_AdjustTime |
| `CG_UpdateHandViewmodels` | `0x1d96e8` | faithful on reviewed path; retained | BG_GetNumWeapons, BG_GetWeaponDef, DObjGetTree, Com_SafeClientDObjFree, CL_RegisterModel, XModelBad, CG_Weapons_SetToDefault, Com_ClientDObjCreate, Com_GetClientDObj, I_strncpyz, DObjUpdateClientInfo |
| `CG_AddViewWeapon` | `0x1da2f8` | faithful on reviewed path; retained | BG_GetViewmodelWeaponIndex, BG_GetWeaponDef, BG_CalculateWeaponPosition_Sway, BG_IsAimDownSightWeapon, AngleVectors, AnglesToAxis, BG_CalculateWeaponAngles, MatrixMultiply, CG_AddPlayerWeapon, CG_GetWeapReticleZoom, AxisToAngles, AngleNormalize360, GetLeanFraction |
| `CG_UpdateViewWeaponAnim` | `0x1dd3fe` | faithful on reviewed path; retained | BG_GetNumWeapons, DObjGetTree, XAnimClearTreeGoalWeights, XAnimSetGoalWeight, BG_ClipForWeapon, BG_GetViewmodelWeaponIndex, CG_RegisterWeapon, CG_WeaponRunXModelAnims, DObjUpdateClientInfo, CL_DObjCreateSkelForBones, DObjGetClientNotifyList, CG_PlayClientSoundAlias, stricmp, DObjCalcAnim, DObjCalcSkel |
| `CG_PredictPlayerState` | `0x1ddafa` | faithful on reviewed path; retained | See path details below; _ZN10LargeLocalC1Ei, _ZN10LargeLocal6GetBufEv, CG_InterpolatePlayerState, _ZN10LargeLocalD1Ev, BG_EvaluateTrajectory, BG_GetWeaponDef… |
| `CG_TransitionPlayerState` | `0x1e4088` | event boundary mismatch → fixed | CG_EntityEvent / damage feedback; skip i <= oldSequence-4. |
| `CG_EntityEvent` | `0x1e4650` | faithful on reviewed path; retained | See path details below; CG_PlayEntitySoundAlias, Com_Printf, BG_GetWeaponDef, Com_Error, ByteToDir, CG_PlaySoundAlias… |
| `CG_CheckEvents` | `0x1e614a` | faithful on reviewed path; retained | CG_CalcEntityLerpPositions, CG_EntityEvent, Com_GetClientDObj, BG_UpdatePlayerDObj |
| `BG_CalculateWeaponPosition_GunRecoil_SingleAngle` | `0x7af8c` | faithful on reviewed path; retained | Inline arithmetic / no external call |
| `G_InitGame` | `0x1addca` | missing mantle initialization → restored | Mantle_CreateAnims at 0x1adf5d before bgs/animation setup; listen client skips allocation. |
| `BG_PlayAnim` | `0x181ff0` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `DiffTrackAngle` | `0x2ce40` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `BG_GetWeaponDef` | `0x79f28` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `GetLeanFraction` | `0x28ec4` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `AddLeanToPosition` | `0x2923c` | faithful on reviewed movement path; retained | AngleVectors |
| `CG_PlaySoundAlias` | `0x14482a` | faithful on reviewed movement path; retained | Com_PickSoundAliasFromList, SND_PlaySoundAlias, CL_SubtitlePrint |
| `UnGetLeanFraction` | `0x28ef4` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `BG_AnimScriptEvent` | `0x18729a` | faithful on reviewed movement path; retained | BG_ExecuteCommand |
| `BG_PlayerAnimation` | `0x183ef2` | faithful on reviewed movement path; retained | GetLeanFraction, AngleMod, BG_SwingAngles, BG_GetWeaponDef, BG_RunLerpFrameRate, XAnimGetWeight; runtime clips advance, full model rendering unproven. |
| `BG_UpdatePlayerDObj` | `0x182312` | faithful on reviewed movement path; retained | XAnimClearTree, indirect |
| `BG_AnimScriptAnimation` | `0x1873d2` | faithful on reviewed movement path; retained | BG_ExecuteCommand |
| `BG_UpdateConditionValue` | `0x1825d4` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `CG_PlayEntitySoundAlias` | `0x144c14` | faithful on reviewed movement path; retained | Com_PickSoundAliasFromList, SND_PlaySoundAlias, CL_SubtitlePrint |
| `BG_PlayerStateToEntityState` | `0x69db8` | faithful on reviewed movement path; retained | PM_GetEffectiveStance, indirect, AngleNormalize180, PM_GetViewHeightLerpTime |
| `BG_AnimUpdatePlayerStateConditions` | `0x182666` | faithful on reviewed movement path; retained | BG_GetViewmodelWeaponIndex, BG_GetWeaponDef |
| `BG_AddPredictableEventToPlayerstate` | `0x69d7c` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `DiffTrack` | `0x2c220` | faithful on reviewed movement path; retained | Inline arithmetic / no external call |
| `Sys_SnapVector` | `0xaed28` | faithful production arithmetic; retained | Mac cvttss2si truncates toward zero; production casts match. Differential fixture now executes the production body. |

The Mac has no standalone STABS/text symbol for PmoveSingle, WalkMove, CmdScale, the flight modes, ladder detection or small timer/foliage helpers. Their flow is inside Pmove (`0x70c7e`, single-frame body at `0x70cf3`). A final symbol-name check found 62 PM/Jump/Mantle/Pmove names, including two EH-only names with no separate text address; those inline helpers are included below.

| Inlined path / native helper | Before | Restored behavior |
|---|---|---|
| PmoveSingle | Simplified | Normal, linked/turret, frozen, intermission, spectator, flight and mantle dispatch; animation conditions, ADS, duck/ground, ladder, weapon, foliage, collision-derived velocity cap, oldVelocity and snap. |
| PM_CmdScale | Simplified | .4 on ADS-walk flag 0x100, not prone; 3-axis normalization and flight/spectator factors; air path has no stance factor. |
| PM_CmdScale_Walk | Missing | Weighted back .7 / strafe .8; crouch .65 / prone .15 with transition interpolation; ADS/lean .4; positive weapon factor (lookup at 0x73110); button 0x800 ×.4. |
| PM_DropTimers | Simplified | Movement/legs/torso timers and timed-bit clearing. Weapon/grenade/damage timers remain in their owning paths. |
| PM_DamageScale_Walk (EH-only name) | Missing arithmetic → restored inline | Nonzero damage timer and nonzero max time: 1 + timer × (-minScale / maxTime), using typed float dvars. |
| PM_StartWeaponAnim (EH-only name) | Faithful inline arithmetic; retained | pm_type <=5; toggle bit 0x200 and OR animation. Mac BeginWeaponChange at 0x7c2dd/0x7c338 confirms the guards and assignments. |
| PM_WalkMove | Simplified | Jump slowdown at 0x72ef6, Jump_Check, friction, damage scale/timer, slope projection, acceleration 9/12/19, slick/knockback acceleration 1, clipped speed preservation and stepping. |
| Noclip/UFO | Simplified / conflated | Distinct no-input UFO stop; 3-axis movement, friction 1.5, acceleration 9, factors 3/6, direct integration. |
| PM_FlyMove | Missing | Spectator duck/flight view height, friction, normalized wish velocity, acceleration 8, StepSlideMove. |
| PM_ClearLadderFlag / PM_CheckLadderMove | Missing | Cleanup and ladderEndPos; surface bit 8; narrow bounds; ground 8 / air 30 reach; entry verification; 300 ms jump exclusion. |
| PM_UpdatePlayerWalkingFlag | Missing | Command 0x1000 plus ADS 0x40, prone/weapon-state exclusions, flag 0x100. |
| PM_FoliageSounds | Missing | 40/180 speed bounds, .75/.9 traces, 500/1500/500 intervals, predictable event 142. |
| Crash landing block | Simplified | Negative-gravity quadratic, original double sqrt/division conversions, 128/300 damage bounds, surface events and proportional 500–1500 ms stun. |

Callee comparisons exposed concrete omissions: original Footsteps calls animation, condition update and FootstepEvent; our version did not select animations and its event function was empty. Walking omitted Jump_ApplySlowdown and weapon lookup. Dispatch omitted Mantle_Check/Move and ladder detection. Native paths now reach those services in reference order. Existing PM_Weapon, ADS, reload and prone-interruption paths were retained; reconstructed AimDownSight/Interupt spellings sometimes differ from Mac symbols.

Exact bob scalar reads are preserved in `E/reference/bob-constants.txt`: 0x32633c is .335 forward running, 0x326338 is .305 forward walking, 0x326334/0x326330 are .36/.325 backward, 0x32632c/0x326328 are .34/.315 crouched, and 0x326320/0x326324 are .25/.24 prone. All arithmetic uses their float representations.

Cgame's reviewed chain is InitView → prediction/viewmodel animation → CalcViewValues; AddViewWeapon → sway/weapon angles; prediction → TransitionPlayerState → EntityEvent → alias playback. Landing (150/450 ms), step (100 ms), duck and lean stay on these paths. Concrete fixes are float bg_bobMax, the event-history boundary and positional sway axes.

## Ported function groups

- Walking/bob/lean: scales, stance/directional animation selection, predicted surface footsteps, lean timing/collision, degrees-based stock movement direction, damage and jump slowdown.
- Dispatch/ladders/mantle: frame dispatch, ladder detection/movement, distinct flight modes, prone pitch, foliage and oldVelocity. Mantle traces use typed structs; clear walls are no longer rejected by an inverted condition. The over trace starts 31 units forward, then traces vertically down 18.
- Slide/step/jump/landing/events: timed primal Z restoration, duplicate-plane handling and clip ties, client entity <64, actual step-drop view/bob deltas and exact 0.19999999f slowdown, flattened ladder reflection, correct landing quadratic/stun. The event ring skips i <= oldSequence-4, matching 0x1e40cc.
- Bob/viewmodel details: bob advances before failed-animation idle fallback (0x6f63b). Yaw position sway uses horizontal scale (0x7b851), pitch uses vertical (0x7b822). bg_bobMax reads current.value (8), rather than integer interpretation of the same bits (1,090,519,040).
- Mantle lifecycle: live ledge testing crashed with a null animation tree in XAnimGetLengthMsec. CG_Init intentionally skips allocation on a listen server; G_InitGame omitted the server call at 0x1adf5d. The native server now creates it before bgs animation setup. The new fixture executes that production setup fragment and checks allocation/idempotence. The initial live crash and failing lifecycle test are preserved privately.

COD2_NATIVE_TRACE=1 enables [move], [move-view] and [move-mantle] records after WS45 integration. It is off by default. Bob records include msec, xyspeed, old/new bob, rate, origin, lean, flags, weapon factor, stance, command and ground; view records include applied bob/max; mantle records include timer/transition/origin. Default menu sweeps contain no move records.

## Unit and differential proof

```sh
taskpolicy -b nice -n 19 python3 tests/movement/run.py --build build-macos
taskpolicy -b nice -n 19 python3 tests/movement/run.py --build build-macos-codx
```

Both pass with assertions enabled and ASan/UBSan (UBSan halts on the first error). Coverage includes 98,304 bob combinations: every old byte, msec 1–16, stance, walk/run and speed. Walking has 2,880 cases with five weapon factors, stance, direction, ADS/buttons and msec; command scaling, friction/acceleration, lean/collision, timers, movement direction, ladder pitch, prone pitch and dispatch have separate checks. Collision cases cover step/drop, 1,802 jump slowdown timer values, 640 landing heights/surfaces, mantle traces/root motion/lifecycle and 20,000 client/server frames. Other tests cover local event-history boundaries, bobMax and three sway states.

Original bugs fail baseline fixtures (E/red-*.log). Bob fallback, sway and mantle initialization each also have a failing regression log and a passing production result. Final oracle review found that the fixture's velocity snap rounded, whereas production and Mac 0xaed28 truncate toward zero. A new boundary test failed that stub (`E/snap-red.log`); the fixture now extracts and executes the unchanged production Sys_SnapVector body. Both full suites and the guide comparison were rerun with that correction. The supplied bobtest.c was compiled privately. At speed 190:

| msec | nominal fps | old port bob units/s | Mac reference | fixed production |
|---:|---:|---:|---:|---:|
| 3 | 333 | 0 | 333 | 333 |
| 4 | 250 | 0 | 250 | 250 |
| 8 | 125 | 125 | 250 | 250 |
| 16 | 62 | 124 | 310 | 310 |

The production 3 ms case predicts three footsteps over 333 frames. The reference itself truncates sufficiently low rates: the supplied speed-171/3-ms case is zero. Frames whose reference increment is at least one advance; no fractional accumulator was invented.

The independent guide comparison is reproducible after run.py:

```sh
taskpolicy -b nice -n 19 python3 tests/movement/run_reference.py \
  --build build-macos \
  --reference "$HOME/Projects/cod2-native-refs/research-clones/cod2engine/archive/lnxded-1.3/src/bgame/bg_pmove.cpp" \
  --output "$HOME/Library/Application Support/CoD2-native-ws43/reference"
```

It builds the external readable 1.3 PmoveSingle on arm64, adapts C++ service names at link time, and compares all 9,896 playerState bytes over 20,000 randomized commands/states on a synthetic floor: zero differences. Native ps/pm/pml/cmd layouts matched 9896/256/128/28. The native client/server handler comparison also has zero differences over 20,000 frames in each build.

This is an independent movement-unit comparison, not retail i386 execution. Collision, jump/mantle/slide, animation, weapon and math services are shared with the fixture; their important expectations have separate disassembly-derived tests. Complex collision worlds and original i386 floating-point execution are outside the harness's proof. Guide disagreements were resolved against Mac constants/control flow.

## Live proof

Runs used stdin, windowed 1280×720 devmap mp_toujane, independent homepaths/ports 30043–30060, the read-only shader cache and timeout -k 5 180. Drivers checked pgrep and held the shared bench lock; no other agent's game was terminated. Drivers/analysis are in E/drivers.

Accepted speed runs are metrics-kar98k_mp, metrics-thompson_mp and metrics-pistol. IDs/factors are verified in traces: Kar98k 16/1.0, Thompson 11/1.05, British Webley pistol 5/1.1. g_speed is 190. Bots were excluded. Back/strafe were oriented into clear street space; prone needed room behind the player. Rejected attempts emit the reference force-stance event.

Measured rates below use player-origin deltas over the first continuous >=100 ms plateau at the expected speed and stance. Each command-time endpoint is in measurements.json. Whole-stage viewpos pairs remain in console.log; they are integer-rounded, include initial acceleration/view bob and are not precise steady-speed readings.

| Weapon | Forward | Back ×.7 | Strafe ×.8 | ADS ×.4 | Crouch ×.65 | Prone ×.15 |
|---|---:|---:|---:|---:|---:|---:|
| Kar98k expected | 190 | 133 | 152 | 76 | 123.5 | 28.5 |
| Kar98k measured | 190.004 | 132.999 | 152.004 | 76.000 | 123.498 | 28.500 |
| Thompson expected | 199.5 | 139.65 | 159.6 | 79.8 | 129.675 | 29.925 |
| Thompson measured | 199.501 | 139.655 | 159.600 | 79.800 | 129.675 | 29.913* |
| Pistol expected | 209 | 146.3 | 167.2 | 83.6 | 135.85 | 31.35 |
| Pistol measured | 209.000 | 146.300 | 167.196 | 83.599 | 135.848 | 31.342 |

*The final Thompson prone attempt was geometrically blocked. The accepted stance-11 interval is in live-thompson-250, with xyspeed 29.924967 and delta rate 29.913267. All accepted discrepancies are under .02 units/s. Early give/weapnext trials had the wrong weapon; another teleport was inside solid geometry. Those runs are rejected, not relabelled as proof.

Bob traces were checked with float32 multiply/add and reference truncation/masking, including prediction replays:

| Run / cap | Updates | Formula mismatches | Frozen despite increment >=1 |
|---|---:|---:|---:|
| live-stock-333 / 333 | 237,814 | 0 | 0 |
| live-stock-250 / 250 | 6,800 | 0 | 0 |
| live-thompson-250 / 250 | 122,083 | 0 | 0 |
| live-axis-125 / 125 | 57,858 | 0 | 0 |
| metrics-kar98k_mp / 125 | 14,907 | 0 | 0 |
| metrics-thompson_mp / 125 | 14,692 | 0 | 0 |
| metrics-pistol / 125 | 15,618 | 0 | 0 |

These are fps caps, not a claim of continuously sustained render fps. Tracing, prediction replay and shared-machine load produce other msec values too. There are 14,018 advancing 3-ms prediction updates in the initial 333 run; the isolated 125 Kar98k run has 9,742 advancing 8-ms updates. Applied bob is nonzero, with max exactly 8 in every view record (observed range across runs about -1.31 to .873).

cg_debugevents 1 shows local entity 0 EV_FOOTSTEP_RUN_DIRT, with two forward-run events at command times 11434/11926 in metrics-kar98k_mp. The cadence follows the recorded msec/bob threshold crossings; the pure 333-frame test checks the complete event count. Local running/wood/plaster and walking events occur in the longer runs. The sound-alias consumer calls remain; no separate acoustic recording was made.

Lean reached -.5 while standing in live-axis-125 (and -.25 prone in earlier runs), with position/view screenshots. The ladder at brush X 818–824, Y 776–802 engaged from its east side: flag 0x20, groundEntityNum 1023, origin Z 60 through 132.991 to the top at 151.125. Ladder snapshots are in live-axis-125/home/main/screenshots/ladder-east.jpg.

Mantle's first real ledge crashed at XAnimGetLengthMsec because the server tree was null. After the initialization fix, mantle-targeted records 4,733 replay-inclusive movement frames, transition 3, timer 8 through 666, and flag 4 clearing at completion; no crash. The targeted driver walks into the box before pressing jump. Its screenshots/logs show the original root-motion path operating on the licensed map. Live-mantle-fixed also survives ledge checks and joins both bots.

The shipped dm.gsc comments out addTestClients, so scr_testclients alone creates no bots. For the requested bot check, E/live-mantle-fixed/home/ws43qa contains a private, reversible QA override enabling that call and selecting Sten for the British bot. Original installed data is untouched. Both bot0 and bot1 reach CS_ACTIVE; status and screenshots are preserved. Runtime cg_dumpAnims output shows pb_combatrun_forward_loop and pb_stand_shoot_walk_forward at weight 1 with time advancing. However, third-person screenshots show an incomplete model (helmet/weapon near the ground), so visible animated legs are **not proven**. This remains for engine-wide model/skeleton/render integration; animation selection alone is not accepted as visual proof.

cg_showmiss: every accepted isolated movement interval reports zero misses. Each metrics run reports nine corrections outside those intervals, during scripted setviewpos resets/teleports (roughly 17–2059 units). Earlier wall/collision/invalid runs also contain misses and are not claimed clean. The misleadingly named dvar_developer import in cg_predict_mp.c aliases imp_cg_showmiss; these messages really are controlled by cg_showmiss. Local zero-miss steady intervals and the bit-identical handler fixture support prediction parity, not universal online parity. docs/macos-port/test-server.md remains the CoD2x online proof; it requires an x86-64 Linux VM this Mac cannot run.

## Gate results

Both Release arm64 targets build with `cmake --build build-macos --target cod2_macos --parallel 3` and the corresponding `build-macos-codx` command, under `taskpolicy -b nice -n 19`. Engine ABI checks use saved, unmodified `compile_commands.engine.json` databases; fixture databases enable assertions. Final build logs are `E/build-stock-mantle-trace.log` and `E/build-codx-final5.log`.

The complete native CONTRIBUTING merge-gate command set was run. `E/gate-independent/results.json`, `E/gate-native/results.json` and `E/gate-final/results.json` record the commands, exit codes and logs; `E/gate-completed.json` records both successful final ABI completions. The focused final reruns listed below supersede the identified earlier failures. The unavailable external Linux comparison is listed separately.

| Gate | Final result / evidence |
|---|---|
| Stock and CoD2x movement ASan/UBSan fixtures | PASS; `tests-final6-stock.log`, `tests-final6-codx.log`. |
| Independent guide PmoveSingle comparison | PASS; zero differing bytes over 20,000 frames; `reference/final4-harness.log`. |
| Tools verification tests | PASS, 19 tests; `gate-tools-retry.log`. |
| Datagen tests and private round trip | PASS; `gate-independent` logs. |
| ABI checker unit tests | PASS; audit, imports and raw-offset synthetic tests. |
| Shader manifest and Wine draw-trace parser | PASS. |
| CoD2x full sanitizer and native suites; SDK identity | PASS. |
| Online production fixtures, stock and CoD2x | PASS. |
| Fixes13 fixtures, stock and CoD2x; private reference facts | PASS. |
| Performance source fixtures | PASS; these are not speed benchmarks. |
| LP64 game/layout, script and both UI fixtures | PASS. |
| Renderer sanitizer, real-context shader raster, texture mip and volume suites | PASS. |
| HUD matrices, impact marks, FX events/primitives/cloud and dedicated FX | PASS. |
| Platform CTest, sanitizers on and off | PASS. |
| Full stock ABI | PASS, check.sh exit 0; 622 TUs and 218 renderer bindings, zero findings. Retail imports: 514 symbols / 1,777 native sites, zero extra/missing dereferences and zero errors. `abi-stock-resume.log`, `abi-final-stock/`. |
| Full CoD2x ABI | PASS, check.sh exit 0; 639 TUs and 218 renderer bindings, zero findings. Retail imports: 514 symbols / 1,779 native sites, zero extra/missing dereferences and zero errors. `abi-codx-complete.log`, `abi-complete-codx/`. |
| Raw offsets, stock / CoD2x | PASS: 390 sources / 1,199 sites and 403 sources / 1,205 sites; zero unreviewed/unsafe. |
| `tests/fixes13/legacy.py --base 664f1a1` | PASS: eight changed sources ×10 legacy configurations, zero mismatches; `legacy-final-fixes13.log`. |
| `tools/abi/legacy.py --base 664f1a1` | PASS: eight files ×5 COD2_X64-off configurations, zero mismatches; `legacy-final-abi.log`. |
| Existing production text, `unifdef` then `cmp` | PASS for all eight changed files; `legacy/unifdef-cmp.log`. |
| `tests/cod2x/check_inactive_gates.py --base 664f1a1` | FAIL: 56 mismatches across eight files, exclusively synthetic COD2_X64=0 or native stock configurations; `inactive-final.log`. This checker also requires native stock code to remain unchanged, so these authorized stock movement fixes fail that requirement. The binding common rules explicitly make this nonblocking; actual COD2_X64-off checks above pass. |
| Stock and CoD2x menu sweeps | PASS, completed=true: each checks 59 front-end menus, 73 in-game menus and 46 safe scripts; `menu-stock/results.json`, `menu-codx/results.json`. Six destructive/external actions are skipped by the sweep. Default move-trace records: zero. |
| Linux multilib object/binary equality | NOT RUN: no compatible Linux execution environment on this Mac. |

Earlier failures are preserved: the tools test run inherited nice 19 and its synthetic benchmark could not restore higher priority (EPERM); the exact command without inherited nice passed all 19 tests. Early movement fixtures had an unresolved fixture `va` symbol, subsequently fixed and rerun. The initial stock raw-offset audit found four scalar float expressions absent from its reviewed manifest; all four were reviewed and narrowly added, and both final audits have zero findings. Full ABI wrapper runs hit their private 1,800-second timeout on this shared Mac. The unrestricted stock resume and complete CoD2x rerun both finished with exit 0, reusing completed audit caches. Only my orphaned CoD2x import process was stopped after its wrapper timed out. Checker baselines are unchanged.

The final ABI commands were:

```sh
taskpolicy -b nice -n 19 sh tools/abi/check.sh \
  build-macos/compile_commands.engine.json \
  "$HOME/Library/Application Support/CoD2-native-ws43/abi-final-stock"
taskpolicy -b nice -n 19 sh tools/abi/check.sh \
  build-macos-codx/compile_commands.engine.json \
  "$HOME/Library/Application Support/CoD2-native-ws43/abi-complete-codx"
```

## Merge and remaining proof

The function-group commits are 53bb01b (walking/bob/lean), 2230854 (dispatch/ladders/mantle collision), 8434b75 (jump/landing/event replay), c703786 (bob fallback/sway), and 53eb4cf (server mantle lifecycle and trace). 60e6c01 strengthens the fixture with production velocity snapping. All have the required DCO email. A final report commit follows. Merge the complete series: restoring mantle dispatch without the server initialization fix can expose the null-tree crash.

The only new shared game initialization change is the small native Mantle_CreateAnims call/declaration in g_main_mp.c. No CMake or src/headers files changed. CONTRIBUTING.md adds the two movement commands; tools/abi/raw-offsets.json appends four reviewed typed float arithmetic expressions, including the existing prone yaw-cap subtraction and directional bob normalization. Those are scalar dvar math, not i386 addresses. Keep these small additions when resolving parallel-branch conflicts.

Remaining unproven work: complete third-person model rendering and visible bot legs; acoustic capture of footsteps; complex-world retail-i386 differential execution; online CoD2x reference-server prediction; and Linux multilib object/binary equality. Native/legacy source guards are proven here; that is not a substitute for the Linux object comparison. No tool was installed to bypass an unavailable platform.

Private cleanup removed the four computed audit-cache directories and external reference/bob/layout build helpers. Large console logs, function-audit JSON and nm output are retained as `.gz`; shorter results and measurement JSON remain plain. `E/cleanup.json` records removals and compression sizes. Screenshots, disassembly checks, rejected-run evidence and reproducible drivers remain in E. No app bundle is retained; the existing CoD2x gate scripts unregister and delete their URL-probe fixtures.
