# WS45: integrated 0.2.1 verification

Date: 2026-10-07. Checkout: `port/int-0.2.1`, HEAD
`0e46885152b77b5bd380c1b356400fc7d3878f17`, plus the focused fixes described
below. Legacy comparison base: `port/main` at
`664f1a12da05a9abada6b8a6ab4831f2377f502d`.

**Completed on this baseline: both cold Release builds, the full native gate
commands, unrestricted stock and CoD2x ABI/fidelity pipelines, and available
autonomous runtime checks. Known red gates and unavailable prerequisites are
listed explicitly below; this is not a whole-game fidelity approval.**

This wave runs the merged WS42/WS43/WS44 checks. It does not certify whole-game
fidelity: all three WS44 behavioral spotchecks still fail without
`--expect-difference`. The Linux object comparison and OS screenshot capture
also have unavailable prerequisites. Source and harness integration failures
were reproduced, narrowly fixed, and checked again. No checker baseline was
changed.

Private evidence roots used below:

```sh
E="$HOME/Library/Application Support/CoD2-native-ws45/integration-021-20261007"
INPUT="$HOME/Library/Application Support/CoD2-native-ws42/WS45-integration-021-20261007.noindex"
APP="$E/qa.noindex/CoD2 Silicon 0.2.1 QA.app"
```

`$E/run_gates.py` records the exact command, checkout, start time, duration,
exit code and log path for every executed command in `*-results.json`.
`build`, `version`, `fixtures`, `fixes`, `platform`, `input-fixtures`, `live`,
`launcher-fixtures`, `package`, `app-live`, `app-live-state`, `spotchecks` and
the focused retry ledgers retain successes and failures. Game content, local
GSC overrides, generated game profiles, screenshots and reference-derived
evidence remain private outside git. QA app bundles and temporary app bundles
are under `qa.noindex`; fixture compilers inherit its temporary directory.
The installed game data and main checkout were not modified.

## Build and merge-gate evidence

Both Release clients were configured with `COD2_X64=ON`, arm64 and exported
compile databases; CoD2x additionally uses
`COD2_FEATURE_CFLAGS=-DCOD2_CODX=1`. Both clean client builds passed. Both
clients were then configured with `COD2_RELEASE_VERSION=0.2.1`, rebuilt, and
rebuilt successfully after the trace fix. The stock dedicated target also
built. Builds and fixture compilation used `taskpolicy -b nice -n 19` and
build parallelism three. No performance measurement or FPS claim was made.

The actual Release databases were saved as
`build-macos/compile_commands.engine.json` and
`build-macos-codx/compile_commands.engine.json`. Fixture databases replace
`-DNDEBUG` with `-UNDEBUG`, as required by CONTRIBUTING; they do not change the
engine's compiled Release flags.

All commands in CONTRIBUTING's native merge-gate block were actually invoked.
The additional legacy, input, menu, launcher and WS44 checks were also invoked.
The final successful focused reruns supersede the earlier failures below.

| Check | Actual result and evidence |
| --- | --- |
| Tools tests | PASS, 32 tests; `tools-tests.log`. |
| Fidelity tooling tests | PASS, 32 tests; `fidelity-tests.log`. |
| Datagen tests | PASS, 12 tests; `datagen-tests.log`. |
| ABI audit/import/raw-offset/bit-conversion unit tests | PASS, 4/6/3/4 tests; `abi-*-tests.log`. |
| Common trace scanner and focused scanner tests | PASS, zero unguarded diagnostics in 721 files and 13 tests; `trace-guards-green.log`, `trace-tests-green.log`. |
| Shader setup manifest and Wine draw-trace parser | PASS; `shader-setup.log`, `wine-trace.log`. Parser success is not an original-game Wine runtime check. |
| CoD2x sanitizer and native suites; SDK identity | PASS after fixture fix; `cod2x-sanitizers-green.log`, `cod2x-native-green.log`, `online-identity.log`. |
| Stock and CoD2x online production-function fixtures | PASS; `online-stock.log`, `online-codx.log`. These use synthetic inputs, not an external public server. |
| Stock and CoD2x fixes13 fixtures | PASS; `fixes13-stock.log`, `fixes13-codx.log`. |
| Stock and CoD2x movement ASan/UBSan suites | PASS; each includes 20,000 randomized synthetic-floor prediction/server states with zero differences; `movement-green-stock.log`, `movement-green-codx.log`. |
| Independent movement guide comparison | PASS in both builds: 20,000 states each, zero differing bytes; `movement-guide-green-stock.log`, `movement-guide-green-codx.log`. Requires the movement suite's generated fixture headers/objects first. |
| Performance source fixtures | PASS; `perf-fixtures.log`; these are not benchmarks. |
| LP64 game and script suites | PASS after diagnostic-module fixture link fix; `game-green.log`, `script-green.log`. |
| Stock and CoD2x LP64 UI suites | PASS; `ui-stock.log`, `ui-codx.log`. |
| Renderer sanitizers, real-context shader raster, mip transparency and volume upload | PASS; `renderer-fixtures.log`, `shader-raster.log`, `texture-mips.log`, `volume-upload.log`. |
| HUD matrices, impact marks, FX events/primitives/cloud and dedicated FX | PASS; corresponding logs, with `fx-events-green.log` superseding the unresolved-link failure. |
| Platform CTest, sanitizers enabled | PASS, 34/34; `platform-ctest.log`. |
| Platform CTest, sanitizers disabled | PASS, 36/36, including diagnostics/crash-report checks; `platform-ctest-plain.log`. |
| Private datagen round trip | PASS: i386 bytes/relocations/symbols, arm64 full/native and BSS probes, LP64 address points and determinism; 16 recovered LP64 objects and 14 regenerated text files; `private-roundtrip.log`. |
| Private layouts | PASS: 335 native fields, 292 retail network offsets/widths, 366 weapon field mappings; `private-layouts.log`. |
| Private fixes13 reference facts | PASS for covered cases; byte-array BSS audit checked 262 objects, with 32 unavailable/ambiguous cases still unproved; `private-fixes13.log`. |
| Legacy fixes13 preprocessing | PASS, 45 changed source files × 10 legacy configurations, zero mismatches; `legacy-fixes13-green.log`. |
| Legacy ABI preprocessing | PASS, 47 files × 5 actual COD2_X64-off configurations, zero mismatches; `legacy-abi-green.log`. |
| Existing production text with native branches removed | PASS, all 45 changed existing production files match base after `unifdef -UCOD2_X64`; `legacy-text.json`. This supplements the unavailable Linux object comparison. |
| Strict inactive-CoD2x checker | FAIL, 48 files × 17 configurations, 270 mismatches; `legacy-inactive.log`. All mismatches are synthetic `COD2_X64=0` or authorized native stock configurations; actual COD2_X64-omitted checks above pass. CMake OFF omits the macro; defined zero is not equivalent. The strict checker remains red and was not weakened. |
| Linux object/binary comparison | BLOCKED: command executed and exited 2, requires x86_64 Linux multilib; `x86-comparison.log`. No compatible Linux execution environment or packages were installed. |
| Full stock ABI, imports and strict bits | PASS, wrapper exit 0 in 1,936.443 seconds: 624 TUs, 218 renderer bindings, 514 import symbols/1,777 native sites; zero mismatches, new findings, extra/missing dereferences or bit-conversion violations. Raw offsets: 391 sources/1,196 sites, zero unsafe/unreviewed. `abi-stock.log`, `abi-stock/`. |
| Final stock and CoD2x new-stub gates | PASS, each has 665 existing candidates and zero new; `stub-final-stock.log`, `stub-codx.log`. The stock rerun uses the final patched source and cache. |
| Full CoD2x ABI, imports and strict bits | PASS, wrapper exit 0 in 2,048.023 seconds: 641 TUs, 218 renderer bindings, 514 import symbols/1,779 native sites; zero mismatches, new findings, extra/missing dereferences or bit-conversion violations. Raw offsets: 404 sources/1,202 sites, zero unsafe/unreviewed. `abi-codx.log`, `abi-codx/`. |

## Reproduced integration failures and focused fixes

1. The trace scanner rejected three WS43 diagnostic sites: `[move]`,
   `[move-mantle]` and `[move-view]`. They had a separate movement opt-in,
   rather than WS42's required common gate. `trace-guards.log` is the red
   result. The native sites now use `COD2_NATIVE_TRACE_ENABLED()`; duplicate
   cached environment helpers were removed and the WS43 report documents
   `COD2_NATIVE_TRACE=1` after this integration. The scanner and 13 existing
   negative/positive scanner tests pass unchanged. Legacy text remains equal.

2. Both CoD2x suites failed to link `_MacPlatform_GetRenderSize` after WS42
   added the real input-pump dependency. The synthetic mouse fixture now
   supplies its 640×480 display response inside the existing native guard.
   `cod2x-sanitizers.log` and `cod2x-native.log` retain red evidence; both
   corresponding `*-green.log` reruns pass. An initial placement outside the
   guard made the legacy ABI checker report five mismatches; the guarded final
   fixture passes all five COD2_X64-off configurations.

3. Game/netchannel, script and FX-event sanitizer fixtures failed to link
   `_cod2_native_trace_enabled`. On this toolchain ASan references require the
   real global despite the weak-import fallback. The affected fixtures now
   link the existing `port_debug.c`, matching the other integrated fixture
   patterns. The common movement gate exposed the same link dependency in
   the movement suite and independent guide; `movement-common-gate-stock.log`
   records that focused red result. The movement fixtures and guide also link
   the real module. Game, script, FX and both movement/guide reruns pass;
   actual Release clients rebuild successfully. No production symbol, scanner
   rule or baseline was replaced with a test-only approximation.

4. `tests/cod2x/run_full.sh` now honors `${TMPDIR:-/tmp}` so its synthetic
   probe bundles can remain under the private `.noindex` temporary root.

Runner/prerequisite failures are also retained. An initial private runner typo
pointed `TMPDIR` at a nonexistent directory, making one tools compile test
fail; the correct directory and exact normal-priority tools command pass all
32 tests. Initial input self-tests were rejected because their output was
outside the deliberately required WS42 private root; the correct `$INPUT`
paths pass without relaxing the validator. An input UDP unit-test timeout
under background contention was rerun at normal priority: 8/8 pass in
0.156 seconds. Initial independent movement-guide calls lacked the generated
`pmove_source.h`; the correctly ordered movement-then-guide runs pass.

Linux prerequisites were checked beyond the host OS: Docker, Lima, OrbStack
and standalone QEMU commands are absent. UTM is installed; its bundled
`utmctl list` exits 0 with no configured guests. Direct access to its private
preferences is denied by macOS, and no permission was changed. This does not
claim x86 Linux emulation is impossible; it records that the required ready
Linux multilib environment is unavailable for this run.

## Runtime, input, focus and launcher evidence

Stock and CoD2x `tests/input/run.py --scenario all` ran sequentially against
an owned stock dedicated server, using fresh private homes. Both pass all 87
checks, including real localhost UDP browser/join traffic. Each menu sweep
passes 59 front-end menus, 73 in-game menus and 46 safe script actions. The
sweep's six destructive/external actions remain deliberately skipped. A
100-frame parity recording passes; it is a smoke check, not original-engine
rendering equality or a performance measurement.
Input uses the test-only SDL event injector, not physical-device gestures.
Physical trackpad feel and HID hardware polling rates are not established.
The optional display-sleep/wake focus scenario was not selected; the executed
focus scenarios cover app switching and Mission Control.

The canonical bare-executable stock fullscreen browser command failed twice
with `Fullscreen menu did not retain SDL relative input`. Both snapshots
show relative input false and system cursor shown. A private minimal stock
app identity permitted explicit LaunchServices activation of the same built
binary: passive window-state evidence changes from foreground false to true,
and the **unchanged** fullscreen input suite then passes all eight checks,
including injected `[10,-6]` matching the UI delta. This characterizes the
missing focus prerequisite; no production focus/input code was changed and
the original two command failures remain recorded. The normal shipping
packager requires CoD2x, so the private stock identity is not a release app.
The first activation-wrapper attempt was rejected by that stock packager
preflight (exit 2); the subsequent minimal test identity avoids that unsupported
packaging path. Both wrapper results are retained.
The second failed bare launch also raised `PermissionError` while its driver
queried the terminated process group during cleanup; the following inventory
confirmed no surviving game. That secondary driver failure is retained too.

Quiet-default evidence is `$E/quiet-default.json`: both complete live input
suites have zero records matching the scanner's native diagnostic tags, zero
movement/view/mantle records, and no `x64_*_trace.txt` or `cod2_heapdiag.txt`
files. Intentional injector telemetry is separate. With only
`COD2_NATIVE_TRACE=1`, a local spawned-player/bot characterization records
1,629 `[move]`, 815 `[move-view]`, and five `EV_FOOTSTEP_RUN_DIRT` lines, with
both bot names present and exit 0. No mantle was attempted in this short
check. The first private probe remained in the server-info/team UI and its
movement-record assertion failed; after the normal menu-response sequence
selected a team/weapon, the corrected private probe passed. Its private GSC
override enables the shipped test-client call and stays outside git.

Run-animation telemetry is present, but complete visible third-person legs
are unproved. The private JPEG shows partial helmet/weapon geometry near the
ground at a position close to a wall; this single shot does not distinguish
missing-model work from camera occlusion. No acoustic recording was made;
event-consumer success does not prove heard footstep quality.

`sh tests/launcher/run.sh` passes core/network/artwork/form/font tests,
macOS 13 type checking, 26 dark/light views and two material-fallback views.
`tests/packaging/first_run.py` passes isolated setup, migration, fake-key/path
validation and signature checks. The built QA app passes
`tests/packaging/launcher_bundle.py`: four portable arm64/macOS 13 Mach-Os,
launcher/helper ownership, deep ad-hoc signature, icon/fonts/licenses and no
game data. The actual release-smoke passes a fresh-home menu, Toujane devmap,
two engine JPEGs, quit exit 0, launcher return and bundled SDL2/SDL3 loading.

Canonical `tests/launcher/real_handoff.py` and both
`tests/platform/focus_visibility.py` modes fail at OS screenshots:
`screencapture` exits 1, `could not create image from display`. No permissions
or global configuration was changed. Private documented copies remove only
the OS screenshot call and fix their repository-root lookup; all original
state assertions remain. These complete all six Play/Deploy/cold-URL cases
(two cycles each): active visible game, hidden system cursor, relative input,
game exit 0 and active visible launcher return. Both fullscreen modes pass
three Finder focus cycles, Mission Control handling, selected-mode recovery
and desktop-mode restoration after quit. JSON state evidence is retained;
these are independent state proofs, not successful canonical screenshot
commands. The first unregister call failed because its focus bundle had not
yet registered; the subsequent actual focus tests register it and final
unregistration passes. All owned game and launcher processes had ended when
the interactive slot was released to the orchestrator.

## Whole-game fidelity acceptance remains red

The actual integrated spotchecks ran without `--expect-difference`:

```sh
python3 tools/fidelity/spotcheck_msg.py --build build-macos --output "$E/msg-stock.json"
python3 tools/fidelity/spotcheck_reverb.py --build build-macos --output "$E/reverb-stock.json"
python3 tools/fidelity/spotcheck_collision.py --build build-macos --output "$E/collision-stock.json"
# The same three commands also ran with build-macos-codx and codx outputs.
```

| Behavioral harness | Stock | CoD2x |
| --- | --- | --- |
| Message byte/bit/bounds behavior | FAIL, 20/32 differences, exit 1 | FAIL, 20/32 differences, exit 1 |
| Reverb wet-level selection | FAIL, 10/10 differences, exit 1 | FAIL, 10/10 differences, exit 1 |
| Collision entry/startsolid behavior | FAIL, 2/5 differences, exit 1 | FAIL, 2/5 differences, exit 1 |

These reproduce known WS44 fidelity defects rather than new merge regressions.
They are not counted as acceptance passes. A zero-new-stub result only guards
against additional suspicious stubs; hundreds of existing candidates remain.
The full structural inventory and table comparison do not prove behavioral
equivalence. Complex-world retail-i386 differential execution, an external
CoD2x reference-server round, complete third-person legs and acoustic
footsteps remain unproved. Subsequent protocol/audio/model work belongs to a
new composed baseline, not this checked 0.2.1 tree.

The integrated structural inventories both completed. Stock has 6,824 reference
and 7,565 native functions; CoD2x has the same reference and 7,683 native
functions. Both have 1,386 missing, 4,984 present, 444 stub candidates and
10 ambiguous mappings. The heuristic flags 595 stock and 598 CoD2x functions.
Both dvar comparisons cover 532 registrations: stock has 536 native
registrations and 25 candidates; CoD2x has 578 and 29 candidates, against
534 reference registrations. These are review signals, not counts of
independently confirmed defects. Both scalar-table comparisons have
95 equal, five different, 32 unresolved, 86 missing/ambiguous native-storage
cases and 145 pointer-bearing cases delegated to the datagen relocation gate.
The five different tables are `s_XModelSurfaceSize` (three scalar fields),
`extendedVirtualKeyConvert` (19), `kbitmask` (20),
`exc_gain_quant_scal3` (eight) and `exc_gain_quant_scal1` (two). Existing WS44
analysis distinguishes intentional native/platform representations from
potential defects; byte differences alone are not new behavioral failures.

The recorded `calibrate.py --reviews WS44-fidelity-sample.json` commands fail
with exit 1 for both builds: the old stock sample no longer matches either
integrated seeded sample. Its previous 60 hand-review notes cannot be
reused to claim precision for this inventory. No sample or review note was
rewritten to manufacture a pass. Current known-function coverage is checked
separately without historical reviews; that cannot replace a fresh hand review.

Current sample applicability was checked explicitly: flagged retains five and
replaces 25; unflagged retains 11 and replaces 19. All 16 retained signatures
and function-body tokens match the original source, and their native semantic
inventories match after excluding checkout paths. Their prior verdicts are
carried only into `calibration-current-reviews-partial.json`: one difference,
12 equivalent and three unresolved. A complete current sample review still
requires **44 new Mac-versus-source reviews**. No verdict was assigned to these
new entries; `calibration-current-pending-reviews.json` records their exact
identities and native locations. Existing three unresolved inherited verdicts
also remain unresolved. The delta is too large to treat as a small sample
refresh within integration-gate verification.

| Sample group | Added current members requiring review | Removed historical members |
| --- | --- | --- |
| flagged | `AnglesSubtract`, `BG_GetWeaponIndexForName`, `CG_Player`, `CL_GetServerCommand`, `CL_Record_f`, `CM_LoadMapFromBsp`, `CStringEdPackage_Clear`, `FS_AddIwdFilesForGameDirectory`, `HECmd_ScaleOverTime`, `Key_WriteBindings`, `MSG_WriteDeltaHudElems`, `MSS_SpatializeStream`, `PC_DollarEvaluate`, `R_ImageList_f`, `SND_DisconnectListener`, `SV_ConnectionlessPacket`, `SV_GetCachedSnapshotInternal`, `SV_Netchan_PrintProfileStats`, `SV_SpawnServer`, `Scr_AddSourceBufferInternal`, `Scr_AllocGameVariable`, `Scr_EvalVariableObject`, `SpecifyThreadPosition`, `String_Alloc`, `TextPool_AllocText` | `BG_RunLerpFrameRate`, `CL_PacketEvent`, `CL_WritePacket`, `CM_PointContentsLeafBrushNode_r`, `Com_SetRecommended`, `ConDrawInput_DetailedDvarMatch`, `DL_DownloadLoop`, `GScr_LoadGameTypeScript`, `Item_TextField_Paint`, `Key_Bindlist_f`, `Key_KeynumToString`, `LAN_CompareServers`, `NET_OpenIP`, `PM_GroundTrace`, `PM_StepSlideMove`, `PbClientProcessEvents`, `R_LoadAabbTrees`, `RemoveRefToObject`, `Scr_AddFields`, `Scr_ExecEntThreadNum`, `Scr_GetConstString`, `Scr_GetInt`, `Scr_StopThread`, `XAnimFillInSyncNodes_r`, `XModelLoad` |
| unflagged | `CG_CheckDObjInfoMatches`, `CG_DrawPlayerSprites`, `CL_CompareAdrSigned`, `CM_PointTraceToEntities_r`, `Effect_SetTimeStartEnd`, `EmitValue`, `FxHelper_Init`, `GScr_SetWinningPlayer`, `GScr_ShellShock`, `IsFieldObject`, `ItemParse_columns`, `Jump_ClampVelocity`, `Key_Shutdown`, `Menu_Paint`, `RB_SetIndexData`, `RB_SetupLighting`, `R_AddCmdDrawTextWithCursor`, `RemoveRefToValue`, `Script_InGameClose` | `CG_CheckEvents`, `CG_StartShakeCamera`, `CL_SortGlobalServers`, `CM_LinkWorld`, `EmitBreakStatement`, `Field_CharEvent`, `FxHelper_SetIgnorePrecacheErrors`, `GScr_GetTeamPlayersAlive`, `GScr_SetBottomArc`, `ItemParse_textstyle`, `Item_Bind_Paint`, `Particle_SetRandomVelocity2Weights`, `RB_SetVertexData`, `R_AddCmdSetMaterialColor`, `SEH_Shutdown_StringEd`, `SV_MapRestart`, `Scr_MakeValuePrimitive`, `Scr_RemoveThreadNotifyName`, `XSurfaceGetNumVerts` |

The separately executed current no-review calibrations also exit 1 in both
builds: `PM_FootstepEvent` scores two,
`PM_UpdateLean` four and `Item_List_Paint` zero, below the old positive-control
threshold. These are WS42/43 restoration targets; the historical calibration
assumes all seven pre-restoration differences remain flagged. This result is
not reclassified as a pass, and no calibration target or threshold was changed.

## Reusable verification handoff

`$E/run_gates.py` and its `*-results.json` files preserve the original commands
and actual outcomes. The runner's process exit alone is not a gate verdict;
each ledger entry records the underlying command's exit code. The recorded
historical calibration commands are preserved even though the old review
sample is inapplicable.

`$E/run_gates_fresh.py` is a private copy parameterized by `COD2_GATE_ROOT`,
`COD2_GATE_EVIDENCE` and `COD2_GATE_INPUT`. It includes the additional
`readiness-fixtures` phase for the composed branch's weapon and audio gates.
`$E/FRESH-GATES.md` supplies the phase order, cold-build and private dependency
prerequisites, log locations, runtime ownership requirements and the exact
separate current-calibration commands. These are an orchestration handoff,
not test results for a later branch. Read the composed branch's CONTRIBUTING
before execution and add any subsequent requirements; no 0.3/Config/Metal
work was started here. The complete cold builds, gates and runtime checks
must be rerun after composing later fixes. The WS45 app and results identify
only the baseline and patch recorded at the top of this report.

## Prepared app and changed paths

The private QA app is `$APP`, with unique identity
`io.github.stixzoor.cod2silicon.ws45qa` and its isolated `.game` helper.
Launcher and helper plists both declare 0.2.1; the engine compile definition
and embedded release literal are 0.2.1 (`qa-version.json`). The tracked
`RELEASE_VERSION` remains 0.2.0 for the orchestrator's canonical version change.
`qa-manifest.json` records HEAD, patched source/harness hashes, the built client
hash and packaged executable/plist/library hashes to identify this tested app.
Existing pinned SDL dependencies were read-only copied from the main checkout's
release dependency cache into the private `.noindex` root before packaging.
No installed app was replaced, and no commit, stage, push, tag or publication
was performed.

Changes for review are the native diagnostic changes in
`src/PC/bgame/bg_pmove.c`, `src/PC/cgame_mp/cg_view_mp.c`; harness dependency
fixes in `tests/cod2x/test_mouse_input.c`, `tests/lp64/game/run.py`,
`tests/lp64/script/run.py`, `tests/platform/run_fx_events.py`,
`tests/movement/run.py`, `tests/movement/run_reference.py`; temporary-root
hygiene in `tests/cod2x/run_full.sh`; the WS43 diagnostic flag documentation;
and this report. Existing legacy production text is unchanged when native
branches are removed. `git diff --check` passes.
The final handoff check confirms the expected HEAD and zero changed source,
harness, built-engine or packaged-file hashes against `qa-manifest.json`;
`final-review-verification.json` records this check. All edits are now
relinquished to the orchestrator for canonical review and commit.
