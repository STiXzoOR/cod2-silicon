# Whole-game fidelity acceptance before 0.3

The user explicitly reaffirmed whole-game fidelity and regression prevention on 2026-10-07. The latest instruction authorizes autonomous checks and removes a required manual playtest. Engineering workers use Sol 6.1 xhigh; design workers use Astra high.

## Authority and limits

Read WS44-fidelity-audit, WS43-faithful-move, WS42-faithful-ui, WS38-lp64-audit, WS38-core, WS12-abi-audit and `docs/macos-port/parity.md` alongside current source. Reports describe their recorded commit, not necessarily the integrated source. The Mac 1.3 reference is authoritative for stock behavior; actual CoD2x 1.4.6.8 code is authoritative for intentional extension behavior. Do not copy incompatible licensed reference code.

Structural score, a successful build, the 668-entry existing stub baseline, and a menu smoke are not whole-game acceptance. WS44 found both false positives and confirmed defects with low/zero structural scores. Every restored behavior needs a production-path regression test with an independent expected result, and runtime proof where relevant. Do not clear a known difference by changing the test expectation or baseline.

## Current integrated evidence and ownership

| Area | Evidence already present | Remaining acceptance and next owner |
| --- | --- | --- |
| ABI and pointer widths | WS12 plus WS38 declarations/imports/raw-offset gates | WS45 reruns actual stock/CoD2x compile databases, private imports, typed-data round trip, legacy source guards; preserve separate unavailable Linux object gate |
| Menus/input/focus | WS42 real SDL input tests, 87 checks/build, fullscreen subset | WS45 reruns clicks, scroll, double-click joins, option labels/binds, Cmd-Tab, default quiet diagnostics and console version; subjective hardware feel is not claimed |
| Movement/prediction | WS43 production fixtures and 20,000-state independent guide comparison | Rerun on integrated engine; comparison shares collision/weapon/animation services, so those are separate rows below |
| Dvars/grammar/profiles | Source research has concrete reference differences | WS47 verifies/fixes cheat flags, registration ranges/owner bits, commands, tokenizer and writable active-profile paths; ISO mapping must follow evidence, not literal §→F1 collision |
| FA01 network key masks | 20/32 decoded widths differ against reference | Correct native generator/table input only; spotcheck_msg must pass without --expect-difference in both builds; preserve original i386 initializer |
| FA15 collision | 2/5 capsule corner rays differ | Restore trace/position/capsule paths in cm_trace.c with independent rounded-geometry oracle; then serially characterize/fix sight trace, stationary brush/terrain and old-hit semantics |
| FA02 mantle weapons | Current integrated bg_weapons.c declares Mantle_IsWeaponInactive but does not call it | Compare mantle/fire/reload/change weapon state traces and restore original inactivity dispatch; do not infer this was fixed from WS43 inventory wording |
| FA04 level state | WS43 added Mantle_CreateAnims | Fog and objective initialization remain absent in current integrated G_InitGame; assert listen/dedicated post-init sentinels and ordering; inspect registration timing separately |
| FA06 reverb | 10/10 production wet-level cases differ | Correct reference predicate in snd_driver.c; existing spotcheck_reverb must pass without --expect-difference in both builds; verify actual audio consumer path |
| FA07 sound command | Active SND_SetEnvironmentEffects_f stub | Restore parsed argument/priority/value behavior with output-state tests; assign one owner to link_stubs.c |
| FA03 view attachments | Omitted bone/tag/view-lock calls | Establish controlled tag/mounted-camera orientation oracle before restoring paths; cross-check turret aiming runtime |
| FA09 HUD | Turret reticle simplified; spectator instructions missing | Capture draw commands for weapon/state/scaling/bindings and compare runtime across resolutions; owner-draw compass is separate plausible finding |
| FA10 browser | Uses raw map names rather than display names | Comparator fixture with deliberately opposing raw/localized orders |
| Numeric fields and FA16 fonts | Localized numeric drawing and language 6–14 font selection absent | Reference number-string and registered asset-path fixtures, glyph runtime checks with installed licensed locales |
| FA11 console hints | Type/domain/default/latched information omitted | Assert draw text and box height for int/float/enum/latched cases; retain approved CoD2 Silicon/CoD2x branding |
| FA12 water/buffer lifetime | Missing generated-coordinate/water upload calls; lifetime differences plausible | Material/time-state capture and rendered water comparison; inspect buffer lifetime/draw payload before changing backend flags |
| FA13 script analysis | Constant-condition local analysis suspected | Compile independent small constant/variable loops; compare errors/behavior before fixing |
| FA14 script failures | Missing script/entrypoint result checks | Production loader tests for missing scripts/handles must produce original early failure |
| FA08 voice codec | Gain-domain difference does not prove a bug | Fixed encoded packet/loss vectors and PCM reference before changes; audit native capture separately |
| Third-person models/legs | WS43 selected animations advance, screenshots show incomplete models | Diagnose model/skeleton/render chain and prove visible complete players and walking/running animation; animation selection alone cannot close this |
| Footsteps | WS43 events and alias calls observed | Capture acoustic output/call-chain behavior across running/walking/surfaces; an emitted event alone does not prove audible sound |
| Weapons/damage/hit registration | WS44 did not establish a specific new defect | Deterministic recoil/spread/damage/hit-location/trace result cases and gameplay capture; retain seeds and compare full relevant state rather than coarse hash only |
| Metal | WS39 goldens/soak pass on older branch, freshness/receipt gaps remain | Integrate only after faithful baseline, re-run combat/dense effects/goldens; strict draw/pass/resource accounting, pacing and freshness on supported displays before default switch |

## Scheduling

1. Finish WS45 full 0.2.1 gate and autonomous runtime checks. Record known fidelity failures separately from integration regressions.
2. Finish WS47 documented registry/grammar/profile corrections; do not mix them into WS45 baseline while it runs.
3. Run independent confirmed-fix units in isolated worktrees: protocol tables; collision; weapon/level state; audio; HUD/view/UI/script. Shared source files have one owner, and live-game/benchmark use is serialized with the benchmark lock.
4. Resolve plausible findings with characterization before implementation. Reconcile older reports against current source, including intentionally replaced platform behavior and active unmatched identities. Never turn all 1,386 unmatched names into implementation tasks without resolving identity and callers.
5. Integrate tested units and Config Studio. Re-run behavior tests for changed subsystems and the complete merge gate on the composed commit. Review actual code separately from worker claims.
6. Merge Metal onto that baseline and close renderer-specific acceptance. A failure is fixed or reported with exact evidence; it is never relabeled a pass to meet a version number.

## Regression contract

- Keep each discovered failure reproducible in a committed synthetic fixture; private original binaries/assets/corpus and generated captures stay local.
- Preserve stock and CoD2x coverage and original COD2_X64-off source. Changes affecting storage get typed layout and bounds tests; changes affecting behavior get state/output oracle checks.
- Verify runtime events, data state and visible/audible output where these are distinct. Avoid assertions against values derived from the function under test.
- Record unavailable prerequisites explicitly. Full original-i386 differential execution and Linux multilib binary equality require compatible execution environments; access to this Mac alone does not establish them.
- Full audit runs preserve unresolved identities, pointer-bearing table gaps, indirect-call uncertainty and unreviewed sample cases. No claim of universal perfection follows from finite tests.
