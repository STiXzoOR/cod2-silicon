# WS44: engine-wide fidelity audit

Date: 2026-10-06. Branch: `port/fidelity-audit`. Engine input: `port/main`
`664f1a12da05a9abada6b8a6ab4831f2377f502d`. No engine, header, CMake, Metal,
packaging or 32-bit source changes were made. This branch supplies audit tools,
tests, a new-stub gate and evidence-based fix briefs. It does **not** establish
that the whole game is faithful, and does not implement the fixes below.

The authority is the Mac 1.3 STABS binary at
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
CoD2x differences were checked against the private 1.4.6.8 source where noted.
All addresses below are original i386 virtual addresses, not native offsets.
Evidence is in `~/Library/Application Support/CoD2-native-ws44/`. It contains
summary JSON and logs, no retained disassembly, decompiler output or AST dumps.
The full final audit summaries are compressed as `stock.json.gz` and
`codx-final.json.gz`; regenerate them with the commands below or decompress
them before running table/calibration tools. Reproducible caches, superseded
summaries and generated game profiles were removed to keep evidence small.
The retained evidence occupies approximately 8.3 MB; `manifest.json` records
each retained file's size and SHA-256.
Nothing was installed, pushed, tagged, published or written to sibling worktrees.

## Reproduce the audit

See [the tooling README](../../../tools/fidelity/README.md) for interpretation,
input restrictions and the stub baseline policy. Use the Release engine database
saved before test fixtures change `-DNDEBUG` to `-UNDEBUG`.

```sh
export FIDELITY_EVIDENCE="$HOME/Library/Application Support/CoD2-native-ws44"
python3 -m unittest discover -s tools/fidelity -v
python3 tools/fidelity/audit.py \
  --compile-commands build-macos/compile_commands.engine.json \
  --output "$FIDELITY_EVIDENCE/stock.json"
python3 tools/fidelity/audit.py \
  --compile-commands build-macos-codx/compile_commands.engine.json \
  --output "$FIDELITY_EVIDENCE/codx-final.json"
python3 tools/fidelity/tables.py "$FIDELITY_EVIDENCE/stock.json" \
  --output "$FIDELITY_EVIDENCE/tables-stock.json"
python3 tools/fidelity/calibrate.py "$FIDELITY_EVIDENCE/stock.json" \
  --reviews docs/macos-port/reports/WS44-fidelity-sample.json \
  --output "$FIDELITY_EVIDENCE/calibration-stock.json"
python3 tools/fidelity/calibrate.py "$FIDELITY_EVIDENCE/codx-final.json" \
  --output "$FIDELITY_EVIDENCE/calibration-codx.json"
python3 tools/fidelity/audit.py \
  --compile-commands build-macos/compile_commands.engine.json \
  --output "$FIDELITY_EVIDENCE/stub-stock.json" \
  --stub-gate tools/fidelity/stub-baseline.json
# Repeat tables, the stub gate and all three harnesses with the CoD2x build.
python3 tools/fidelity/spotcheck_msg.py --build build-macos \
  --output "$FIDELITY_EVIDENCE/message-stock.json" --expect-difference
python3 tools/fidelity/spotcheck_reverb.py --build build-macos \
  --output "$FIDELITY_EVIDENCE/reverb-stock.json" --expect-difference
python3 tools/fidelity/spotcheck_collision.py --build build-macos \
  --output "$FIDELITY_EVIDENCE/collision-stock.json" --expect-difference
```

`--expect-difference` is a characterization of a known failure. Omit it to
require reference behavior; all three harnesses currently fail that acceptance check.
The reference is read at runtime. Generated sources/executables are temporary
and deleted. The harnesses need no game data and do not execute the i386 binary.
Their expected results come from reference table bytes or manually checked
reference branches, independently of the production function being exercised.

## Inventory and structural coverage

The final inventory and counts are recorded in
[WS44-fidelity-inventory.jsonl](WS44-fidelity-inventory.jsonl). Each row gives
the reference name/address, native match/status, both scores, missing callee
names and counts of weaker signals. These are candidates, not verified bugs.
The full local JSON also retains summarized literal sites and storage uses.

| Result | Stock | CoD2x |
| --- | ---: | ---: |
| Native definitions extracted | 7,552 | 7,670 |
| Reference functions uniquely paired | 5,428 | 5,428 |
| Present without a stub pattern | 4,981 | 4,981 |
| Paired stub candidates | 447 | 447 |
| Unmatched identities | 1,386 | 1,386 |
| Ambiguous identities | 10 | 10 |
| Structural flags (score ≥ 8) | 601 | 603 |
| Paired functions below threshold | 4,827 | 4,825 |

There are 1,674 paired functions with potential blob/import storage annotations.
The parser correction recovered 35 previously unmatched reference definitions.
The committed catalog was generated using tooling commit `02f7605`; subsequent
snapshot/documentation changes do not change structural scoring.

`nm -ap` agrees with the parser on **6,824 named `N_FUN` records**, covering both
global and local functions and both `__text` and `__textcoal_nt`. Native analysis
uses only the actual `cod2_macos` target: 390 stock and 403 CoD2x translation
units. All inputs parsed successfully. A cross-check against linked native
symbols caught clang's elided-file locations after header macro expansion;
the parser now has a regression test for that case.

“Missing” means no uniquely matched definition under the current name matching,
not proof that behavior is absent. This group includes libwww, original Miles
objects, obsolete Carbon/OpenGL paths, C++ constructors/templates, renamed
helpers and intentionally unsupported PunkBuster. For example,
`Dvar_RegisterVariant_impl`, `FX_AddFxToScene_impl`, `GetToken_impl`,
`Scroll_Slider_SetThumbPos_impl` and `HudElem_SetEnumString_impl` implement
functions reported unmatched under their original names. Ten ambiguous entries
are static initialization functions. Do not schedule 1,386 missing-function
restorations from a raw count. Resolve identity and active callers first.

Blob annotations identify native global storage references, including imports;
they do not prove execution reachability. `src/blobs` supplies data/BSS/literals
and import pointers, rather than a hidden executable i386 engine. The arm64
target uses generated typed C storage. `src/imports` contains SDK declarations.
Platform replacements need subsystem comparisons, not a demand to recreate
each old platform method.

The strongest score signal is an original engine callee absent from four-edge
native reachability. PLT stubs, `_impl` callees, recognized `Reg` ABI bridges and
encoded template callees normalize to their identities. Same-function extracted
helpers contribute literals and branch counts. Floating and string differences
are weaker, and branch/size differences are weak. A score of eight flags triage.
Added callees and reverse literal/string differences are retained for review.
No required calibration name participates in scoring.

## Calibration and precision

All seven required cases flag in both builds:

| Function | Reference address | Stock score | Evidence |
| --- | --- | ---: | --- |
| `PM_Footsteps` | `0x6ed5c` | 38 | Omitted event/animation branches and callees |
| `PM_FootstepEvent` | `0x6eab2` | 30 | Reduced event selection and literals |
| `PM_UpdateLean` | `0x6bdce` | 26 | Output-zeroing replacement of traced lean behavior |
| `Pmove` | `0x70c7e` | 52 | Missing mantle checking/hints/move paths |
| `PM_WalkMove` | No standalone `N_FUN` | 52, caller context | Mismatching `Pmove` ancestry |
| `PM_CmdScale` | No standalone `N_FUN` | 52, caller context | Mismatching `Pmove` ancestry |
| `Item_List_Paint` | `0x16ad16` | 16 | Missing `Item_Text_Paint` |

The two caller-context results are weaker than independent function comparisons:
the original compiler inlined those helpers. Original ladder behavior is also
partly inlined; there is no claim that `Pmove` originally made a standalone
`PM_LadderMove` call. Native `Pmove` does not call its ladder helper and lacks
the original ladder handling. WS43 owns movement restoration. WS42 owns the
known listbox/label/input work. This branch does not duplicate those fixes.

The final seed-44 stock sample has 30 flagged and 30 unflagged functions,
validated by `calibrate.py`. All 60 have individual hand-review notes in
[WS44-fidelity-sample.json](WS44-fidelity-sample.json). The inventory correction
changed 15 selections; those replacements were reviewed before reporting the
final results. The CoD2x known-case calibration also passes, but this precision
sample is stock, not a second independent CoD2x sample.

| Sample | Material differences | Signals explained / no difference found | Unresolved | Result |
| --- | ---: | ---: | ---: | --- |
| Flagged | 5 | 19 | 6 | Resolved precision **5/24 = 20.8%**; unresolved-case bounds **16.7–36.7%** |
| Unflagged | 1 | 25 | 4 | Observed difference rate **1/26 = 3.8%**; unresolved-case bounds **3.3–16.7%** |

Bounds count unresolved reviews as all negative or all positive; they are not
statistical confidence intervals. One flagged difference is the intentionally
absent PunkBuster facility. Excluding it from newly actionable regressions gives
**4/24 = 16.7%** resolved precision and **13.3–33.3%** bounds. The unflagged
text-contract difference is the bind-item placeholder/prompt described below;
it may be a deliberate improvement. Numerical scores are therefore unsuitable
as a pass/fail gate or as permission to restore every reference detail blindly.

“Equivalent” in the hand review means the reported signals had an explanation
and no material difference was found in that inspection; it is not an all-input
proof. Large functions with unresolved paths remain unresolved. Notes identify
callee-boundary noise, debug/error text, platform substitutions and constant
folding rather than silently accepting every flag. The randomized sample covers
owned `src/PC` functions, excluding bundled Speex/zlib/JPEG and functions outside
`src/PC`; PC sound and PunkBuster adapters remain eligible. Do not extrapolate
its precision to the excluded libraries.

The reverb inversion below scores **zero** for both functions. The key-mask
problem's caller scores **four**. These independently confirmed false negatives
show why low scores cannot be a fidelity acceptance gate. The unflagged sample
also cannot measure global recall. Compare functions structurally to prioritize
review, then require an independent behavioral oracle for each fix.

## Ranked findings and fix waves

“Confirmed” means the omission or behavior difference was established from the
reference and native code/table, with a harness where stated. A predicted player
symptom can still be conditional. “Plausible” means the signal needs a dedicated
behavioral check before changing the engine. Rankings reflect player impact,
not the numerical structural score. Proposed ownership is exclusive per wave;
the orchestrator must sequence shared-file dependencies.

### Gameplay, network, weapons, prediction and sound

1. **FA01 — keyed user-command decoding: confirmed.**
   `MSG_ReadDeltaUsercmdKey` (`0x1757bc`) uses `kbitmask` at `0x34b7e0`;
   its mask load at `0x175e9c` accesses the 18-bit entry at `0x34b828`.
   The source initializer omits 8191, shifting entries 13–31 and leaving entry
   32 zero. The **actual linked build input**, `x64_gen/data_native.c.o`, has
   20 wrong fields out of 33. An older assembly table's correction does not
   fix this generated C input. Extracted production `MSG_ReadDeltaKeyBits`
   yields **20/32 decoding differences in each build**. At width 18, key
   `1 << 18` and wire value 1 decode to 262145 rather than 1; the caller's
   later shift introduces button bit 19. No active action bound to that bit
   was established, so a specific unintended player action is not claimed.
   Out-of-width decoding and protocol interoperability risk are confirmed.
   Own `tools/datagen/generate.py` and a native-only table correction, plus
   focused protocol fixtures. **Preserve the legacy initializer byte-for-byte**;
   do not repair `src/blobs/data.c` unconditionally. Re-run the table and message
   harnesses as acceptance checks without `--expect-difference`.

2. **FA15a/FA15b — specialized collision paths: confirmed omission and corner-contact difference.**
   `CM_Trace` (`0x64426`, 4,254 bytes) calls stationary brush testing
   (`CM_TestInLeafBrushNode_r`, `0x6488e`/`0x64abf`), `CM_MeshTestInLeaf`
   (`0x64afa`/`0x651a3`) and `CM_TraceSphereThroughSphere`
   (`0x6514c`/`0x651ee`/`0x6521f`/`0x65292`). Native `cm_trace.c:486` uses
   generic swept brush/terrain paths and treats temporary model `0x3ff` as a
   brush. The original temporary-model branch (`0x6498c` to `0x64e2a`) derives
   rounded capsule geometry from the model bounds (`0x64f8c`–`0x64feb`).
   A production-source harness traces five point rays through a synthetic
   model with bounds `[-10,-10,-20]` to `[10,10,20]`. Rays at `(9,9)` and
   `(-9,9)` in the middle cylinder are clear under the reference radius-10
   geometry, but native reports start-solid/all-solid: **2/5 differences in
   both builds**. This analytic oracle is deliberately far from epsilon
   boundaries; it does not execute i386 or test terrain. Player contacts,
   prediction and near-corner entity rays are affected conditionally on these
   model shapes. Give `src/PC/qcommon/cm_trace.c` to FA15a for trace/position/
   capsule paths, coordinating contract tests with WS43 but not its files.
   FA15b must follow serially in the same file: `CM_BoxSightTrace`
   (`0x6551e`, 2,350 bytes) omits `CM_SightTraceThroughLeaf` (`0x65836`),
   `CM_SightTraceThroughTree` (`0x65899`) and rounded sphere tests
   (`0x65bf5`/`0x65dc8`/`0x65e27`), ignores the old-hit hint, and substitutes
   ordinary tracing/leaf enumeration. Native visibility callers exist in
   `sv_world_mp.c`, `rb_light.c` and `rb_sky.c`. Sight-trace behavior beyond
   the proven contact cases is **plausible** and needs dedicated fixtures.

3. **FA02 — weapon inactivity during mantling: confirmed omission.**
   `PM_Weapon` (`0x7cbb2`, 8,749 bytes) calls `Mantle_IsWeaponInactive` at
   `0x7d0ce` and `0x7d842`. Native `bg_weapons.c:2345` never calls it, although
   the predicate is implemented in `bg_mantle.c:191`. It checks mantle enable,
   the mantle player flag and the transition animation. Conditional impact:
   weapon activation/firing during mantle states once WS43 makes those states
   reachable. Own `src/PC/bgame/bg_weapons.c`; coordinate mantle state contracts
   with WS43, without editing its movement files. Add mantle/fire state traces.

4. **FA03 — skeletal/locked camera paths: confirmed omission; player effect
   plausible.** `CG_CalcViewValues` (`0x1d3b28`, 3,729 bytes) calls
   `DObjGetBoneIndex` (`0x1d4344`), `DObjGetRotTransArray` (`0x1d4362`),
   `MatrixMultiply43` (`0x1d453f`), `AxisToAngles` (`0x1d45cf`),
   `Com_GetClientDObj` (`0x1d46a5`) and `CG_DObjGetWorldTagPos` (`0x1d46d1`).
   Native `cg_view_mp.c:374` implements normal bob/lean/third-person/shake/FOV,
   but none of these weapon-tag or view-locked attachment paths. Missing tag
   errors also disappear (`Com_Error`, `0x1d48eb`). Animated weapon and mounted
   camera orientation/aim are the likely visible effects. Own
   `src/PC/cgame_mp/cg_view_mp.c`; test controlled weapon/tag/view-lock states,
   independent of the WS43 movement harness.

5. **FA04 — level initialization: confirmed omissions; conditional gameplay
   impact.** `G_InitGame` (`0x1addca`) calls `Mantle_CreateAnims` (`0x1adf5d`),
   `G_setfog` (`0x1ae0b8`) and `G_InitObjectives` (`0x1ae0bd`). Native
   `g_main_mp.c:625` omits them. The only mantle-animation creation is in client
   initialization, so a dedicated server has no corresponding setup. Zeroed
   objective entries retain entity number zero instead of the reset sentinel
   `0x3ff`; initial fog opaque-distance-squared remains zero instead of `FLT_MAX`.
   The server visibility code distinguishes the no-fog sentinel, making map
   script ordering relevant. Own `src/PC/game_mp/g_main_mp.c`; compare post-init
   objective/fog/mantle state for listen and dedicated servers. Coordinate the
   mantle initialization insertion with WS43. Original `SaveRegisteredItems`
   and `SaveRegisteredWeapons` calls at `0x1ae137`/`0x1ae13c` moved to native
   `G_RunFrame`; their registration timing is a separate plausible issue.

6. **FA05 — packet and client dvar policy: confirmed registration differences.**
   `cl_maxpackets` and `loc_language` ranges and 20 client registration flags
   differ; see the dvar table below. Packet rate affects network timing, while
   registration flags can alter configuration promotion. Own
   `src/PC/client_mp/cl_main_mp.c`. Coordinate this file with WS42's version and
   input work; schedule after its merge if necessary. Record registration
   state with and without a preexisting config and competitive mode.

7. **FA06 — environmental reverb: confirmed behavioral inversion.**
   `SND_Update2DChannelReverb` (`0x56878`, branches `0x56897`–`0x568d8`) and
   `SND_Update3DChannelReverb` (`0x568fa`, branches `0x56919`–`0x56952`) choose
   environment wet level when the alias is null, or when effects are enabled
   and alias flag `0x10` is clear; otherwise they choose zero. Native
   `snd_driver.c` chooses the opposite result. Capturing AIL wet-level calls
   from the extracted production bodies gives **10/10 mismatches in each
   build**, covering both channel modes, enabled/disabled and flagged/unflagged
   aliases. The native AIL setters honor wet level, so audible environment
   response is affected. Own `src/PC/win32/snd_driver.c`; the existing harness
   is the independent acceptance check. No audio backend rewrite is indicated.

8. **FA07 — environment effects command: confirmed active stub.**
   `SND_SetEnvironmentEffects_f` (`0x4cf46`, 592 bytes) parses command arguments,
   validates 0–1 values and `none`/`level`/`shellshock` priorities, and calls
   `SND_SetEnvironmentEffects`. Native `src/stubs/link_stubs.c:406` is empty.
   `SND_Init` actually registers this function (`snd.c:1107`), so the console
   command does nothing. Own only this portion of `src/stubs/link_stubs.c`, with
   argument/output-state tests. Assign a single owner to this shared stub file;
   other restoration waves must not edit it concurrently.

9. **FA08 — voice codec compatibility: plausible, needs fixed packet vectors.**
   `nb_decode` (`0x1f52f6`) differs in `compute_rms`, `lsp_interpolate`,
   `signal_mul`, `speex_bits_remaining` and `speex_rand` paths. The two excitation
   tables at `0x33f140`/`0x33f160` differ bitwise, but the original uses linear
   gain while native uses log gain followed by `exp`; original loads at
   `0x1f6527`/`0x1f655b` confirm that distinction. Raw table mismatch is **not**
   proof of wrong gain. Version/packet-loss concealment and rounding could
   affect voice quality or interoperability. Own `src/PC/speex/`; first compare
   fixed encoded packets, loss sequences and decoder PCM against a reference
   oracle. Preserve intentional platform capture behavior.

No separately verified damage or hit-registration regression was established.
High-scoring `turret_think_client` is not evidence of missing turret shots:
native extracted helpers already contain the gun-position/blend/trace/fire
paths. Those subsystems still need deterministic state and trace-result fixtures.

### UI, rendering, effects and scripts

10. **FA09 — HUD branches: confirmed simplifications, plus one plausible case.**
   `CG_DrawTurretCrossHair` (`0x1cf59c`, 346 bytes) uses `BG_GetWeaponDef`
   (`0x1cf5f7`), split-screen scaling (`0x1cf66b`) and a weapon reticle draw
   (`0x1cf6d7`). Native `cg_draw_mp.c:1030` always draws an 8×8 white square at
   `(316,236)`, alpha 0.75, without the original material/state logic.
   `CG_DrawSpectatorMessage` (`0x1cfeba`, 637 bytes) uses `CL_GetKeyCatchers`
   (`0x1cfede`), `Controls_GetConfig` (`0x1cfefa`), localized bindings
   (`0x1d003b`) and `UI_ReplaceConversionString` (`0x1cffa3`); native prints
   only the spectator heading. Player impact: wrong turret aiming display and
   missing spectator control instructions. Own `src/PC/cgame_mp/cg_draw_mp.c`.
   Separately, `CG_OwnerDraw` (`0x18cc30`) lacks compass pointer radius scaling,
   translated location strings and material-registration branches in its
   dispatch. This is **plausible**, pending owner-draw case/menu reachability
   checks; give `src/PC/cgame_mp/cg_newDraw_mp.c` a separate FA09b owner if its
   compass scope grows. Compare captured draw commands and screenshots at
   multiple resolutions/control configurations, not just menu-script success.

11. **FA10 — server-browser map sorting: confirmed.**
    `LAN_CompareServers` (`0x181c36`) calls `UI_GetMapDisplayName` at
    `0x181e47`/`0x181e54` before comparing map columns. Native `cl_ui_mp.c:493`
    compares raw `mapName` strings. Localized/display map names can sort
    differently. Own `src/PC/client_mp/cl_ui_mp.c`; test the comparator with
    differing raw/display-name orders. Coordinate browser expectations with
    WS42's listbox work, without duplicating its implementation.

12. **WS42 follow-up — localized numeric text fields: confirmed omitted branch.**
    `Item_TextField_Paint` (`0x16994a`) uses `Dvar_FindVar` (`0x169c8d`) and
    `Com_LocalizedFloatToString` (`0x169cc4`). Native `ui_shared_mp.c:2776`
    always copies `Dvar_GetVariantString`, losing the numeric-localization
    branch. Focus/color/blink signals need review too. WS42 already owns
    `src/PC/ui_mp/ui_shared_mp.c`; attach this finding to that owner or sequence
    a follow-up after its merge. Test locale-sensitive numeric drawing.

**WS42 policy review — bind-item draw text: confirmed reference difference,
not an automatic restoration request.** `Item_Bind_Paint` (`0x16a346`, score 2)
has a no-label branch passing the literal `FIXME` at `0x16a6c9` to
`UI_DrawText` (`0x16a6d0`). Native draws binding names, or `EXE_KEYWAIT` in the
waiting state. This is the unflagged sample's text-contract difference. Keep
ownership with WS42's `ui_shared_mp.c`; verify the labeled/unlabeled and waiting
states, and explicitly decide which native improvements to retain instead of
blindly reintroducing a placeholder.

**FA16 — locale-specific font selection: confirmed omitted branch; visible
impact plausible.** `UI_ParseMenuInternal` (`0x1acac4`, score 14) calls
`RegisterFontHack` at `0x1acc6c`, `0x1acd2a` and subsequent font branches.
`RegisterFontHack` (`0x1a8acc`, 393 bytes) switches on current languages 6–14
and selects `fonts/russian/`, `czech/`, `thai/`, `chinese/`, `japanese/`,
`taiwanese/`, `korean/` or `polish/` paths before `CL_RegisterFont`.
Native `UI_ParseFontAsset` registers the supplied path directly;
`CL_RegisterFont`, `R_RegisterFont` and `R_LoadFont` contain no equivalent
locale selection. Expected player impact: wrong font/glyph coverage in affected
locales. Own `src/PC/ui/ui_shared_obj.c`; capture registered asset paths and
locale glyph draws, coordinating FA05's language range and WS42's labels.
This is one of the final flagged sample's verified differences.

13. **FA11 — console dvar hints: confirmed omitted display information.**
    `ConDrawInput_DetailedDvarMatch` (`0x15f646`) calls
    `Dvar_DomainToString_GetLines` at `0x15f960`, displaying default/latched labels
    and domain/type/range information. Native `cl_console_mp.c:1482` has a fixed
    3/4-line box and values only. Own `src/PC/client_mp/cl_console_mp.c`; compare
    draw text/box sizes for int, float, enum and latched variables. Coordinate
    any WS42 console debug-output edits first.

14. **FA12 — animated water/generated coordinates: confirmed absent calls,
    visible effect plausible.** `RB_DrawSingleTechnique` (`0xf7e66`, 5,666
    bytes) calls `RB_ChangeGenTexCoords` at `0xf8e6c` and
    `RB_UploadWaterTexture` at `0xf8eea`/`0xf8f52`. Native `rb_shade.c:1219`
    never calls either. `RB_UploadWaterTexture` has a definition/header but no
    active engine caller. Water animation and generated texture coordinates
    need material-specific observation; an independent Metal implementation
    was not ruled out by this code audit. Own `src/PC/gfx_d3d/rb_shade.c` and
    the corresponding state/water routines. Check a water material across
    timestamps and sampler/coordinate draw state before proposing backend work.

**FA12 follow-up — vertex-buffer lock/lifetime contract: plausible.**
`RB_SetVertexData` (`0xf6e1e`, score 0) has the same supported stride cases
(0x14/18/20/24/40/44), with color conversion extracted into a helper. Its original
lock flags at `0xf6e4d`–`0xf6e57` are 0x2000 for offset zero and 0x1000 otherwise;
native uses 0x2000 for offset zero or `gpuSync == 0`, and zero otherwise.
Native `CDirect3DVertexBuffer_Lock` ignores flags, while original Lock
(`0x1eb66`) has discard/recreate/fence behavior. Unsupported-stride fallback also
differs. No specific Metal rendering failure is proven, so the sample keeps
this unresolved. Share FA12's exclusive `rb_shade.c` owner or sequence a small
follow-up; first trace buffer lifetime and draw payloads before changing flags
or the platform implementation. An integer/indirect-call contract comparison
would see signals missed by the current float/callee score.

15. **FA13 — script constant-condition local analysis: plausible.**
    `Scr_CalcLocalVarsStatement` (`0x9290a`, 8,198 bytes) uses
    `EvalPrimitiveExpression` (`0x93fe8`, `0x94802`),
    `EvalBinaryOperatorExpression` (`0x94171`, `0x94875`), `Scr_CastBool`
    (`0x948c4`, `0x948ed`) and value-reference cleanup (`0x94015`, `0x9482f`).
    Native while/for local-variable analysis omits these constant-evaluation
    paths. Potential impact: changed local merges, uninitialized-variable
    diagnostics or break handling for constant loops. Own
    `src/PC/script/scr_compiler.c`; compile small constant/variable loop programs
    against independent expected errors and resulting behavior before fixing.

16. **FA14 — script-loading failure checks: confirmed omission.**
    `GScr_LoadScripts` (`0x1a034c`, 664 bytes) checks script-load/handle results
    following the delete/struct loads (`0x1a0364`, `0x1a039c`) and later handle
    lookups. Native `g_scr_main_mp.c:4618` does not enforce those failures,
    although its gametype helper checks its own loads. Player impact is
    conditional on invalid/missing mod scripts: later failure instead of the
    original early fatal error. Own `src/PC/game_mp/g_scr_main_mp.c`, separate
    from FA04's initialization file. The tool's lifecycle-callee flag alone is
    a false positive: begin/post/end compilation moved into `G_InitGame`.
    Use missing-script/entrypoint fixtures to establish the real error contract.

These are the hand-established fix briefs. The inventory preserves all other
flags and unmatched identities as an unreviewed queue, rather than claiming
that this list exhausts defects. Resolve the 30/30 sample's unresolved cases
and active unmatched engine identities before treating that queue as complete.

## Dvars and data tables

Both builds recover 534 reference dvar names and compare 532 names. Stock
recovers 536 native names, with 25 mismatch candidates; CoD2x recovers 578,
with 29 candidates.
Twenty-one are missing flag bits (20 client settings plus `sys_SSE`), two are
stock ranges, one is metadata and one is a first-registration/order artifact.
CoD2x adds four names with differing registration values.

The dvar extractor compares type/default/min/max/flags only when values are
resolved. It follows straight-line i386 cdecl stack/register values; branches,
calls and unmodeled arithmetic invalidate state. Unknown/computed arguments
stay null. It does not reconstruct every indirect renderer registration or
enum domain, and uses the first recovered registration per name. Thus these
counts are recovery coverage, not the whole runtime registry.

| Dvar(s) | Original registration | Native difference / interpretation | Owner |
| --- | --- | --- | --- |
| `cl_maxpackets` | `CL_InitOnceForAllClients`, `0x1485e2`: int 30, range 10–100, flags `0x1001` | Stock min 15; CoD2x 125, range 15–125. Min 15 disagrees with both reference policies. CoD2x source raises max to 125; its competitive branch sets 125 and restores min 10 outside that mode. Unconditional default 125 needs a policy correction/check. | FA05 |
| `loc_language` | `SEH_InitLanguage`, `0x316e2`: int 0, range 0–14, flags `0x1021` | Native max 13. Registration range difference confirmed; usability of index 14 needs the language table/runtime check. | FA05 |
| 20 client variables listed below | `CL_InitOnceForAllClients`, `0x1485e2` | Lose bit `0x1000`; bool/string Mac bridges simply forward flags. Dvar registration explicitly tests this bit when updating names/domains. It is not the cheat bit (`0x80`) or external bit (`0x4000`). Behavioral impact with preexisting variables is plausible. | FA05 |
| `sys_SSE` | `WinMain`, `0xc3a7a`: bool 0, flags `0x1000` | Native flags zero. Legacy compatibility variable on arm64; lower priority than active client settings. | Follow-up, `src/Mac/Main/mac_main.c` |
| `gamedate` | `G_InitGame`, `0x1addca`: `Aug 30 2006`, flags `0x1040` | Native `Apr 13 2006`. Metadata mismatch; coordinate WS42 version text, do not confuse it with simulation. | WS42 / FA04 |
| `sv_cheats` | First recovered `SV_Init`, `0x141d62`: flags `0x1018` | First native game registration is `0x1000`, but `Dvar_Init` also registers `0x1018`. First-site/order false positive; no claim that cheats are enabled incorrectly. | Registry trace, no immediate fix |

The 20 client variables are `activeAction`, `cl_anglespeedkey`, `cl_avidemo`,
`cl_bypassMouseInput`, `cl_connectTimeout`, `cl_forceavidemo`, `cl_freezeDemo`,
`cl_noprint`, `cl_serverStatusResendTime`, `cl_showSend`, `cl_showServerCommands`,
`cl_showTimeDelta`, `cl_showmouserate`, `cl_shownet`, `cl_shownuments`,
`cl_talking`, `cl_timeout`, `fx_cull`, `fx_sort` and `nextdemo`.

CoD2x additional differences: `com_hunkMegs` 512/min 512 is explicitly patched
in private `src/shared/common.cpp:386`, so preserve it; `shortversion` 1.4.6.8
is intentional. `snaps` max 40 and `cl_maxpackets` max 125 are explicitly
patched in `src/mss32/cgame.cpp:241–245`. However, unconditional `snaps` default
40 and `com_maxfps` default 250 are not justified by those registration patches:
`src/mss32/competitive.cpp:54–76` applies them conditionally and restores limits
outside competitive mode. Audit native competitive transitions against that
source before labeling all elevated defaults intentional. Give the
`src/PC/qcommon/common.c` default change to a small FA05b wave after the policy
trace; it need not share FA05's client file.

Reference-only recovered names are `cl_profileTextHeight` and `motd`; this is
not proof they are absent. Stock native-only recovered names are
`cg_shock_viewKickFadeTime`, `in_rawmouse`, `r_borderless` and `r_presentMode`.
CoD2x has these plus 42 extension names, including radar, competitive, raw-input,
demo-upload and match settings. They remain in the local registration summary;
this pass does not certify all their defaults against CoD2x policy. Platform
and extension settings are not automatically fidelity regressions.

The table pass produced **363 records** in each build: 95 equal scalar tables,
five different, 32 unresolved layouts, 86 unmatched/ambiguous native storage and
145 pointer-bearing tables. It reads actual object storage, preserves float
bits and accounts for widened `long` and struct padding. The five differences:

| Table / original address | Mismatched fields | Interpretation |
| --- | ---: | --- |
| `kbitmask`, `0x34b7e0` | 20 | Confirmed native bug, FA01; behavior harness above |
| `s_XModelSurfaceSize`, `0x34a200` | 3 | Expected LP64 command strides: native skinned/cached 0x20 rather than 0x10, rigid 0x40 rather than 0x38 |
| `extendedVirtualKeyConvert`, `0x34a460` | 19 in original 42-byte extent | Legacy Carbon path differs; no active direct native caller found, SDL supplies live input. Cross-check WS42 before classifying as player bug. |
| `exc_gain_quant_scal1`, `0x33f140` | 2 | Different gain domain; not a direct gain failure, FA08 requires packet vectors |
| `exc_gain_quant_scal3`, `0x33f160` | 8 | Same qualification |

The pointer-bearing set is **not compared semantically** by this pass. The
existing typed-data relocation/roundtrip gate passes but checks generator inputs,
which can already be wrong, as `kbitmask` demonstrates. Extend the table method
to relocatable pointer graphs, strings and function targets before closing
this coverage gap. Runtime BSS tables require state snapshots after initialization.

## Gate results and run hygiene

Both Release arm64 builds succeeded with `COD2_X64=ON`; CoD2x used
`COD2_FEATURE_CFLAGS=-DCOD2_CODX=1`. The build command was
`cmake --build <build> --target cod2_macos --parallel 3` for each build.
`python3 -m unittest discover -s tools/fidelity -v` passes all **32 tests**.
The CONTRIBUTING suites were run, covering tools/datagen/ABI unit tests,
CoD2x sanitized and native fixtures, online identities and fixtures, fixes13,
LP64 game/script/UI/renderer, texture/raster/volume, HUD/impact/FX and both
sanitized/plain platform CTest configurations. Layout, private-reference and
typed-data roundtrip checks also pass. `tests/perf/run.py` is a fixture result,
not a performance measurement.

The tools suite initially failed `test_benchmark_completion_and_shutdown`
with `[Errno 1] Operation not permitted`; the unchanged full suite passed all
19 tests on retry. The cause is unresolved, so preserve this as a transient
run-hygiene/tooling failure rather than suppressing the first result.
The initial ABI wrapper timeouts (900 seconds) were superseded by full runs
with a longer bound. Logs and original/retry exit records remain local.

Full `sh tools/abi/check.sh <build>/compile_commands.engine.json <evidence>`
exits zero in both builds, including the private-reference import pass:

| Check | Stock | CoD2x |
| --- | ---: | ---: |
| ABI translation units | 622 | 639 |
| Parse errors / mismatches / new mismatches | 0 / 0 / 0 | 0 / 0 / 0 |
| Renderer bindings | 218 | 218 |
| Callback/cast/unprototyped-float mismatches | 0 | 0 |
| Raw-offset sources / sites | 390 / 1,196 | 403 / 1,202 |
| Unsafe or unreviewed offsets | 0 | 0 |
| Reference import symbols / active native sites | 514 / 1,777 | 514 / 1,779 |
| Proven extra or missing import dereferences | 0 | 0 |

Both `tests/fixes13/legacy.py --base port/main` and
`tools/abi/legacy.py --base port/main` pass with zero mismatches.
`tests/cod2x/check_inactive_gates.py --base port/main` reports zero changed
files, 17 configurations and zero mismatches. `git diff --check` passes.

Both short windowed menu sweeps passed: 59 main menus, 73 in-game menus and
46 script actions each. A CoD2x stdin-driven `mp_toujane` smoke recorded 100
frames successfully. These prove startup/menu/script execution and smoke
stability, not control fidelity or parity with the original. No performance
or online paired-reference claim is made. Runs checked for an existing game
first, used unique homes/ports (29944–29946), a bounded `timeout -k` window and
the existing shader cache read-only. No game bundle was packaged. The required
CoD2x suites create synthetic URL-probe
bundles and unregister/remove them. Their first run used the scripts' existing
temporary paths without a `.noindex` ancestor; that hygiene mistake was corrected
by rerunning both suites beneath the private `qa.noindex` directory. No fixture
bundle or scratch build remains after cleanup.

The new-stub gate works without the private binary and scans active compiled
engine/platform function bodies. It fails on additional empty bodies,
unconditional zero returns, zeroed outputs or comments containing `stub`,
`simplified`, `TODO`, `approximate` or `placeholder`. The baseline freezes
existing candidates, including legitimate tiny functions; it is not a fidelity
approval. A negative run against an empty baseline and nonexistent reference
path fails with all existing candidates as new, proving it does not depend on
the reference. A parser repair updated the initial snapshot for previously
missed definitions; no engine change was exempted. Never regenerate this
baseline merely to make a future engine change pass.

Both build gates accepted the 668-entry initial baseline with zero new
candidates. The negative run reported 668 new candidates and exited one.

All engine paths are unchanged from the base (`git diff --exit-code 664f1a1
-- src CMakeLists.txt cmake`). Both legacy gates report zero mismatches. There
are no touched engine files needing a per-file `unifdef -UCOD2_X64`/`cmp`
comparison. A 32-bit executable build was not performed on this arm64 Mac;
the byte-for-byte source/build-input preservation follows from the empty diff.

## What remains invisible and how to close it

- Identical callee/literal sets can hide inverted predicates, reordered writes,
  wrong indices, missing state resets and arithmetic errors. Reverb and masks
  are concrete examples. Build extracted-function truth/state harnesses for
  weapons, damage, spread, hit locations, prediction, objective/fog and audio.
- Clang AST calls are not optimized arm64 object calls. Four-edge helper
  expansion reduces extraction noise but can hide paths and does not validate
  control-flow ordering. Callback arguments deliberately are not direct callees:
  for example, `VM_Execute` passes arithmetic callbacks to an evaluator; its
  high score is not evidence that arithmetic was omitted. Add indirect-target
  analysis and context-aware call edges, with assembly/object cross-checks.
- Exact definition names miss ABI-renamed implementations and constructor or
  template identities. Resolve active unmatched functions through linked
  symbols and source signatures; retain ambiguity rather than guess.
- Floating comparisons include heuristic `__const` interpretation, SSE masks,
  folding and AST literal signs. They do not establish numeric equivalence,
  precision or evaluation order. Add signed-expression normalization and
  per-operation output vectors; do not fix floats from a set difference alone.
- Static reference strings can be shared across functions; short localized keys
  and aliases are useful clues, while long/format/code strings are hashed.
  Missing strings alone do not prove missing behavior. Runtime registration and
  dispatch traces are needed for sound/FX/dvar/menu names.
- Original empty functions exist. `FxHelper_CameraShake` (`0xab0b0`) and
  `R_ClearFlares` (`0xf5b54`) are examples. Native `MSS_SpatializeStream` and
  `CM_PointContentsLeafBrushNode_r` also have unused small shims while active
  implementations exist. The gate detects new patterns, not semantic reachability.
- Whole-game differential execution requires an i386-compatible host/reference
  runner. Feed identical recorded user commands and RNG seeds, compare per-frame
  player/weapon/entity/snapshot state, trace contacts/damage and sound events,
  then compare draw commands and selected images. Menus need real mouse/trackpad
  events; stdin alone missed the failures motivating this wave. No such
  original-vs-native replay was available in this workstream.

## Merge instructions

Merge this branch's focused signed-off commits; no engine fix needs cherry-pick
ordering. `CONTRIBUTING.md` is the only shared top-level file changed, adding
the suite/gate commands and documentation link. No CMake or `src/headers/*`
conflict is introduced. Keep full generated audits local; the committed catalog
contains only names, addresses, counts, callee identities and summary notes.

Schedule FA01, FA02, FA03, FA06, FA09 and FA15a in parallel where resources permit.
FA04 needs a mantle initialization agreement with WS43. FA05, the numeric-field
follow-up, browser and console waves must coordinate WS42's file ownership.
Give each proposed wave a reference-backed acceptance fixture before an engine
change; retain CoD2x's verified conditional changes and preserve legacy builds.
Review plausible findings before committing to a restoration implementation.
