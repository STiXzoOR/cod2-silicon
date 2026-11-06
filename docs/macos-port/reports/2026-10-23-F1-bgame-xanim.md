# F1a — bgame and xanim reconciliation, 2026-10-23

Branch: `decomp/f1-bgame-xanim`. Starting commit: `4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7`.

## Scope and dispositions

Compared every function in `src/PC/bgame/` and `src/PC/xanim/` with cod2engine Linux 1.3, pairing renamed ABI bridges and folded helpers explicitly. Mac 1.3 assembly settled differences and scalar arithmetic order. No reference implementation or pseudocode was copied. The weapon field table also matches all 366 Mac keys, ILP32 offsets and field types; its inc files needed no changes.

| Group | Our function rows | Linux names | Equal | Fixed | Intentional | Mac differs from Linux |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Movement, jump, mantle, slide | 77 | 92 | 55 | 6 | 15 | 1 |
| Weapons, misc, weapon loader | 151 | 163 | 97 | 39 | 10 | 5 |
| Player animation and CoD2x helpers | 48 | 48 | 25 | 14 | 8 | 1 |
| xanim, DObj, xmodel and loaders | 204 | 252 | 151 | 32 | 21 | 0 |
| Total | 480 | 555 | 328 | 91 | 54 | 7 |

Counts include our representation/ABI helpers and retained legacy helpers; they are not a count of original Mac symbols. The active compile-set AST has 471 C definitions, all represented in the tables; nine CoD2x header helpers bring the total to 480. xanim has 253 Linux definitions because `DObjUpdateServerInfo` is overloaded. Linux-only rows identify helpers folded into our functions, dedicated/source-set differences, or APIs with no emitted Mac symbol/native caller. They are explained separately in the tables rather than silently omitted. No function remains unreviewed. `equal` means source-level behavior compared under valid player/loaded-asset contracts; it does not claim exhaustive executed binary equivalence.

Private evidence root: `~/Library/Application Support/CoD2-native-decomp/F1-bgame-xanim.noindex/`. Tables: `movement-table.json`, `weapons-misc/table.{json,tsv}`, `animation/functions.json`, `xanim/function-table.{json,tsv}`. The report intentionally does not reproduce the full tables.

## Restored behavior and Mac evidence

### Movement

- `PM_ViewHeightAdjust`: initialize a zero current height immediately (`0x6d7cb–0x6d7da`); restore all three saved velocity words after position adjustment (`0x6da0f–0x6da21`). Stock stance tables have zero offsets, so the latter regression uses a synthetic nonzero offset.
- `PM_CheckDuck` and the missed-ground branch of `PM_GroundTrace`: use raw handler traces, without the player-trace BODY retry, touch insertion or tracemask mutation. Mac calls include `0x6e233`, `0x6e2cf`, `0x6e395`, `0x700ca`, `0x70899`.
- `Pmove`/our `PmoveSingle`: linked types 1 and 7 select their weapon update before the default turret path. The jump table at `0x329210` sends both to `0x712ef`; weapon call `0x713db` differs from turret reset `0x710be`.
- `Mantle_Check`/`Mantle_CheckLedge`: restore original diagnostic strings (`0x9f6c6`, `0x9ef56`).

WS43 movement, friction precedence `5a907af`, signed-zero acceleration `9bd5d79`, and angle interpolation `742bf1f` remain. Mac `PM_Accelerate` MAXSS at `0x6b9f8` selects wishspeed on equal operands; Linux/x87 is not the oracle for its signed-zero result.

### Weapons and shared player logic

- `BG_LoadWeaponDefInternal` (`0xbd248`): ADS rate reciprocals use transition times at ILP32 `0x2c0/0x2c4`, not fire/rechamber times. Default damage ranges update `0x598/0x59c`, not projectile destabilization fields (`0xbd4b6–0xbd529`).
- Weapon registration (`BG_GetWeaponIndexForName`, `0x7ede0`): restore load/fallback/requested-name/item/ammo/clip/alternate/callback sequence and item field writes; remove native reconstruction-only guards, logging and extra item-count/classname writes.
- `BG_TakePlayerWeapon` (`0x7bec4`, search `0x7c037–0x7c0d9`): search for a stackable replacement while the old slot is occupied, so the removed weapon cannot select itself.
- `PM_Weapon` (`0x7cbb2`) and its factored helpers: restore binocular/offhand/change/reload/melee/rechamber ordering and timer gate; continuous automatic cadence; held semi-auto latch using semiAuto rather than boltAction; weapon removal/zero/ladder/disabled/frozen changes; segmented reload and melee interruption; rechamber ejection/delay; grenade pullback/hold-edge/throw/quick-raise timing; dry-fire cooldown; binocular timing, use/release and cancellation. Preserve the intentional CoD2x empty-offhand event policy and deliberate mantle lowering.
- Hold breath and spread: restore the fire-delay contribution and original rounding points in gasp scaling (`0x7cd17–0x7cd57`); add the two airborne spread increments sequentially (`PM_AdjustAimSpreadScale`, `0x7ad44`).
- Weapon/view angles (`BG_CalculateWeaponAngles`, `0x7f226`; `BG_CalculateViewAngles`, `0x8033a`): use rotational movement vectors and attenuate before filtering; freeze scoped idle posture while aiming; restore bob phase and single/double boundaries; compute damage timing before scoped amplitude attenuation; apply gun offsets only for ADS-capable weapons; preserve double subtraction in recoil subdivision and breath multiplication order. The zero-speed static recoil decay branch also runs (`0x7af8c`, `0x7b09c`).
- `BG_CheckProneValid` (`0x6a534`): raised entry trace tests allsolid (`0x6aa6a`); rear deflection runs only after the first body trace hits (`0x6a959`); preserve exact-one and angle-failure predicates.

Mac wins over Linux for out-of-line versus inlined bob precision and sin/sinf selection; view-damage decay applies its factor once; Mac anim-type storage is 64 pointers (`0x8aab20–0x8aac20`) despite Linux declaring 32. Existing faithful Mac behavior is retained in these rows.

### Player animation

- `BG_PlayAnim` (`0x181ff0`): torso-only playback updates state while returning -1; only the legs path sets the return-status slot (`0x1820c0/0x1820f9`).
- `BG_AnimUpdatePlayerStateConditions` (`0x182666`): underhand uses view pitch, read at playerState+`0xe8` (`0x182922`).
- `BG_AnimScriptAnimation` (`0x1873d2`): locomotion command is clientNum modulo numCommands (`0x187576/0x187580`), preserving the random stream; event animations retain random selection.
- `BG_UpdatePlayerDObj` (`0x182312`): first creation skips the free callback for NULL (`0x18238c–0x1823bb`).
- `BG_Player_DoControllers` (`0x182b82`): back-low compensation subtracts waist from torso (`0x1832e0/0x183390`); pelvis intentionally uses the opposite order.
- `BG_RunLerpFrameRate` (`0x183492`): initialize missing animation pointers; choose the maximum of initial/minimum/remaining stance blend; preserve moving-loop phase and snapshot-held speed; restore death/torso notification and weight behavior and original diagnostic text. The original stock stance blend is 400 ms in the existing synthetic matrix, not 37 ms (`0x18357a–0x183598`).
- `BG_PlayerAnimation` (`0x183ef2`) and `BG_SwingAngles` (`0x182170`): restore mantle flag, idle swing preservation/tolerance, firing-before-ADS clamp and mounted-before-ladder precedence, and ordered swing branches.
- Parser: preserve command token unget/line handling, optional sound fallthrough, condition scalar fallback and allowed indentation state. `BG_ParseCommands` (`0x18548e`) clears only the packed body-part word (`0x18550e`); Linux clears the whole command. Mac wins.
- CoD2x 1.4.6.8 controller helpers retain separate 30/10 degree rounding, additive backward offsets, remaining-time fractions and double cosine curves. Revision gates remain in place.

### Animation trees, skeletons and models

- `XAnimCalcRelDeltaParts`: select the second quaternion for reference index 1, avoiding an out-of-array read (`0x39a96/0x39aac/0x39b53`).
- Leaf sampling (`XAnimCalc`, `0x3bf76`): sample the last key at time 1/zero frame count; null simple quaternion contributes full identity weight; remove native fabricated 4096-frame/pointer-range skips; preserve duplicate-name mapping stop behavior.
- Goal blending: choose descendants/ancestors by goalWeight (`0x38f5e`, `0x3b935`); multiply before dividing blend duration (`0x3b678–0x3b683`); use current sibling weight; zero weight rather than rate for immediate clearing; clear siblings at each knob-all ancestor; reset sync descendants only when the sync node changes.
- Time and notetracks: restore rate*frequency*dtime, current cycle and 16-bit wrap (`0x41a1d–0x41c0f`); reject children with either zero current or zero goal weight (`0x42873/0x4287b`); emit wrapped server prefixes (`0x3eca0–0x3ecfb`); retain Mac's odd two-record client prefix stride (`0x4115d`); ordered notify minimum reductions retain current results on NaN (`0x42208`, `0x428a9`, `0x4290a`).
- `DObjCalcSkel` (`0x7561a`): restore ignored/control/duplicate masks, duplicate creation, controlled quaternion order and grouped parent transforms. The original does not implicitly call `DObjCalcAnim` here. All production viewmodel/tag/pose/bone paths in `cg_ents_mp.c`, `cg_weapons.c` and the server's `G_DObjCalcPose`/`G_DObjCalcBone` already calculate animation for the requested mask before skeleton composition; no caller repair was needed outside the synthetic fixture.
- `DObjGetSurfaces`: logical mask shifts (`0x74520/0x74624`). `DObjTraceline`: minimum planes before maximum planes, product comparison before division, original tie-normal/sign persistence and transform grouping (`0x76fb6–0x773f5`). `XModelTraceLine` uses the corresponding grouped normal transform (`0x46209–0x46224`).
- `DObjSetLocalTagInternal`: double half-degree conversion before rounding to float (`0x74cf5–0x74d06`, literal8 `0x340c48`). A 0.3f yaw supplies radians bits `0x3b2b92a7`, not `0x3b2b92a6`.
- `XModelBoneNames`: return the complete native pointer; update its shared declaration and `GScr_GetPartName` consumer. This is the only necessary engine edit outside the two module directories.
- `XModelLoad`: free the successful temporary parts buffer (`0xbc35a–0xbc360`) and restore the surface-conflict diagnostic. `Com_ValidXModelName` accepts slash only (`0x45b06`). `XAnimDisplay` prints current then goal weight (`0x3fbb4–0x3fbc4`).

`XAnim_GetTimeIndex` and `XAnim_GetTimeIndexCompressed` already exist. Their valid sorted-index interval/lerp behavior was checked; they were not missing stubs.

## Verification

Both final Release builds pass. All new weapon, prone, animation and xanim fixtures pass in stock and CoD2x; the modified existing movement/weapon fixtures, weapon/animation revision matrices and skeleton/CPU-skinning fixtures also pass in both variants after the final compatibility guards. Assertion-enabled fixture databases were checked after rebuilding (642 stock entries, 659 CoD2x entries); the separate actual engine databases retain Release `-DNDEBUG`.

Both full native ABI scripts pass. Stock checks 634 translation units and 1,779 active import uses; CoD2x checks 651 translation units and 1,782 active import uses. Each reports zero compiler errors/signature mismatches, all 218 renderer bindings clean, zero table/cast/unprototyped-floating-call findings, zero strict bit-conversion violations, and zero proven extra/missing import dereferences. All 14 final rebuild/recheck commands pass. The stock literal weapon fixture also passes a fresh run after the final build (`f1-weapons-stock-final.log`).

The remaining CONTRIBUTING checks pass: tool/datagen tests, trace guards, shader/Wine parsers, CoD2x sanitized/native and SDK identity fixtures, audio, fidelity, collision, UI/online, camera/HUD, gameplay matrices, host/network/dedicated lifecycle, script/compiler/VM, renderer/performance, layout/reference/data round-trip, and all 46 platform CTests with sanitizers on and all 46 off. Wave-relative fixes13 and ABI legacy guards pass; the latter reports 17 files, five `COD2_X64`-off configurations, zero mismatches.

The main run contains 113 commands and six initial nonzero exits. Two were obsolete stock weapon-fixture setup/expectations, now corrected from Mac evidence and passing; one was the interrupted stock ABI run's two unreviewed source patterns, now precisely classified in its existing ledger. The persistent reds are the two Metal stub checks below and the inactive-CoD2x source comparison. The final rebuild/recheck run records the superseding results; the initial failure logs remain available.

The inactive-CoD2x check against this wave's starting commit reports 114 differences: 80 in configurations defining `COD2_X64=0` and 34 in the two native `COD2_CODX=0` configurations. The former use a configuration CONTRIBUTING explicitly disallows as the OFF build. The latter include intentional stock native repairs and fixture assertions; the checker also processes the CoD2x-only helper header outside its include context. All configurations leaving `COD2_X64` undefined match. This source comparison remains red; it is distinct from the supplied cumulative 436 count and was not rebaselined.

Both full stub gates are red: 466 existing candidates and the same nine candidates absent from their baseline, all in untouched Metal files. These are `MacMetalBackend_BeginScene`, `MacMetalDraws_Update`, `ObjectsReleased`, `CacheNamespace`, `GLInvoke`, `MacMetalTargets_StopGPU`, `MacMetalTargets_CancelClears`, `MacMetalTargets_HasClears`, and `VolumeD1`. No bgame/xanim candidate was added. `git diff --exit-code 4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7 -- src/Mac/Metal` exits 0; the stub checker and baseline were not changed. This is an inherited red relative to the stub baseline, not a passing merge gate.

Exact commands and exit codes for the complete gate run are in private `checks-results.json`; the final incremental rebuild/rechecks are in `final-checks-results.json`. Core reproduction commands, run from this worktree with the actual engine compile databases and assertion-enabled fixture databases prepared as in CONTRIBUTING:

```sh
E="$HOME/Library/Application Support/CoD2-native-decomp/F1-bgame-xanim.noindex"
for B in build-macos build-macos-codx; do
    taskpolicy -b nice -n 19 cmake --build "$B" --target cod2_macos --parallel 2
    for T in run weapons f1_weapons f1_misc; do
        taskpolicy -b nice -n 19 python3 "tests/movement/$T.py" --build "$B" --output "$E/reproduce/$B/$T"
    done
    taskpolicy -b nice -n 19 python3 tests/movement/f1_animation.py --compile-db "$B/compile_commands.engine.json" --output "$E/reproduce/$B/animation"
    taskpolicy -b nice -n 19 python3 tests/xanim/run.py --compile-db "$B/compile_commands.engine.json" --output "$E/reproduce/$B/xanim"
    taskpolicy -b nice -n 19 python3 tests/movement/weapons.py --revision-matrix --compile-db "$B/compile_commands.engine.json" --output "$E/reproduce/$B/weapon-matrix"
done
python3 "$E/legacy_views.py"
git diff --check
```

The private legacy script obtains each starting file with `git show 4f0ba1000cd6f8032d9ebd9b44e6e043181db1c7:PATH`, runs `unifdef -UCOD2_X64` on both versions and executes `cmp` on the resulting files; it checks every changed engine C/header file.

Heavy commands ran under `taskpolicy -b nice -n 19`, sequentially, with build/audit worker counts capped at two. The private runner's `limitjobs/sitecustomize.py` caps the existing Python executor defaults without changing audit results or repository tools. `TMPDIR` and fixed fixture-output paths point into this unit's evidence directory; the temporary native-app probe unregisters and deletes its bundle on exit.

Detailed ABI reports are retained as `abi-stock/functions.json.gz` and `abi-codx/functions.json.gz`; reproducible compiler caches and redundant reference-body extracts were removed. Function disposition tables and all check logs/results remain available.

All 13 touched engine C/header files have identical `unifdef -UCOD2_X64` output against the starting commit, checked with actual `cmp`; `legacy-views.log` records each file. This is source-view identity, not the unavailable Linux x86 object/binary comparison.

New fixtures use production functions, synthetic assets/services and literal Mac-derived expected values. They cover weapon cadence/timers/loading/angles/breath, prone trace order, animation selection/blending/parser/controllers, root motion/keyframe ends/notetracks/masks/traces and exact controller radians. The xanim fixture uses ASan/UBSan with shift sanitization disabled because inherited signed `1 << 31` mask idioms remain; no claim is made that this removes their C-language UB.

Existing fixtures were corrected to the original contracts: 400 ms stock stance blend; explicit CalcAnim for each requested skeleton mask; distinct empty/mapping string handles; real duplicate masks; real model bone lookup. Weapon-selection setups now put their owned rifle into a primary slot, as required by Mac `BG_IsWeaponValid` (`0x7be4c`); the original 30 ms raising assertion remains. The stock unavailable-offhand event total is 11, because Mac `PM_Weapon` tests offhand before finishing the firing state (`0x7cf79–0x7cfab`, empty event `0x7d062`); CoD2x suppression still expects 1. Expectations were derived from Mac instructions and contracts.

Changes to the four existing C/header fixtures are native-gated as well. Their `COD2_X64`-off views remain identical to the starting fixtures; their selected native bodies are unchanged by those compatibility guards. Final fixture runs cover both build variants after adding the guards.

The existing raw-offset review ledger gained two precise entries: scalar breath arithmetic (a pattern-match false positive) and the duplicate-parts string's four 32-bit mask words before byte index pairs. Neither the checker nor its thresholds changed.

## Replay and remaining verification

F0 must confirm command/state replay, particularly held automatic/semi-auto fire; segmented reload/melee and prone interruption; frag/smoke hold/release and final-grenade quick raise; binocular cancellation; ladder/linked/turret weapon paths; zero view-height initialization; prone ledges/slopes; locomotion random-state stability; 400 ms stance blends; loop/root-motion ends; crossing goal weights and split notetracks; controlled/attached skeleton masks; and equal-entry bone-box hit normals.

Asset-backed scenarios should include fallback/alternate weapon registration, unequal ADS/fire timings and default damage ranges, stackable weapon removal, movement while aiming, scoped stance/damage kick, sub-5-ms recoil steps, and repeated uncached model loads. These cover behavior and temporary-buffer lifetime that the pure synthetic cases cannot establish for real assets.

No GUI game run or original i386 process differential was performed. Licensed game data stayed read-only, and no files were written under `~/Library/Application Support/CoD2 Silicon/`. Synthetic fixtures used private output/temp directories. Cross-platform sinf/cosf last-bit agreement remains unverified; exact input rounding and scalar arithmetic are covered. The inherited red gates remain visible: legacy-abi has the supplied 10 known differences, inactive CoD2x comparison the supplied 436, and x86 Linux binary byte comparison is unavailable. These are not green claims or newly measured baseline counts.

## Time and merge notes

Rough stages: rules/setup/enumeration about 3 minutes; paired source/Mac inspection and fixes about 20 minutes with three parallel sub-agents; fixture authoring, table cross-checks and integration about 10 minutes, overlapping the builds and gate run. End-to-end wall time was about 2 hours 50 minutes. Existing whole-tree checks accounted for most of that time.

Measured command time: the 113-command main run took 98.1 minutes; the 14-command final rebuild/recheck run took 52.3 minutes, 150.4 minutes combined. The two full stub audits consumed 20.8 minutes; the full native ABI scripts consumed 50.0 minutes for CoD2x and 49.9 minutes for stock. Stages overlap, so these figures should not be summed with the source-work estimates. Reporting and commit preparation ran during the long audits; no inventory sealing, output-hash manifests or duplicate parent runs were performed.

One coherent batch, DCO signed. No pushes, tags, remote changes, global installs or edits to another worktree. All evidence is private; the committed artifacts are code, synthetic fixtures, two scoped raw-offset review entries and this report. The orchestrator owns integration and replay acceptance. Merge the pointer-return declaration/consumer and fixture contract corrections together with the module changes; retain both new raw-offset ledger entries if parallel work conflicts there.

Worktree-local builds and the `build-macos`/`build-macos-codx` alias symlinks remain untracked for reproduction. No scratch build directory was created outside this worktree.
