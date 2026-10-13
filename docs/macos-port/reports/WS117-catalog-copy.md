# WS117 — Config Studio catalog copy

2026-10-09. Worktree `config-copy`, branch `codex/config-copy`, based on `codex/config-studio-int` at `defeb5a`. This unit rewrites the plain-language copy in `tools/catalog/copy.json` and regenerates `launcher/Resources/ConfigCatalog.json` from it. It changes no Swift, no engine source and no test. The registration facts, coverage and gate are exactly those of `048ff5e`; only authored copy fields changed.

## Result

Every registered setting now carries authored copy. Before: 99 settings had it and 629 showed the generic "An engine reference setting…" text. After: 728 of 728 registered settings are authored and none shows the generic text (the other five of 733 are the existing supplemental references, left as they were).

| Purpose group | Written (was generic) | Revised (already authored) | Unchanged | Total |
| --- | ---: | ---: | ---: | ---: |
| Frame & movement | 74 | 3 | 0 | 77 |
| Aim & input | 12 | 11 | 1 | 24 |
| Network | 129 | 33 | 0 | 162 |
| Visibility | 28 | 6 | 0 | 34 |
| HUD & crosshair | 121 | 9 | 0 | 130 |
| Audio | 36 | 7 | 0 | 43 |
| Graphics | 144 | 18 | 0 | 162 |
| Other | 85 | 12 | 4 | 101 |
| **All** | 629 | 99 | 5 | 733 |

Of the 629 written entries, 338 are settings a player can edit and 291 are protected or read-only (cheat-protected, ROM or write-protected). Those 291 never become Expert rows; their copy exists for Jump, History, text-edit notes and the "N protected settings" count. Each of the 303 protected or read-only entries (the 291 plus 12 revised ones) states that status, and none suggests changing it (checked by scan). All 99 earlier entries were revised. The 22 Guided entries are unchanged in number and order.

A diff of the final catalog against `defeb5a` shows only `title`, `summary`, `details`, `group`, `control` (additive `labels`, `practicalRange`, 12 new `unit`/`step` values) and `effectSources` changed. No registration site, type, default, range, flag, `guided`, `menuExposed`, availability, origin, command or `dynamicRegistrations` entry differs.

Also: 16 settings now carry `control.labels`, one carries `control.practicalRange`, and 64 carry `effectSources` pointing at the engine function that establishes the claim (every cited path and function was checked against the 048ff5e export).

## The WS103 copy-gap list

| Gap | What the catalog now says |
| --- | --- |
| `r_picmip` value labels | `control.labels` 0 Extra, 1 High, 2 Normal, 3 Low, the order of the game's own Texture Resolution menu (`ui/options_graphics.menu`, `dvarfloatlist`). The same labels are on `r_picmip_bump` and `r_picmip_spec`, and the sentence is in `details` so History can read it today. |
| `cl_mouseAccel` unit/step | Step 0.01 (zPAM's mouse menu offers 0 and 0.01 to 0.04). No unit: the value multiplies counts per millisecond (`CL_MouseMove`), which is not a player-facing unit; `details` gives the formula and an example. |
| `sensitivity` practical range | `control.practicalRange` 0.01 to 30: the game's slider runs 1 to 30 and ESL's old PunkBuster list stops at 30; the lower bound is the registered minimum because values below 1 are valid. `details` also says the same number turns further on a wider picture (`CG_CalcFov`). |
| `r_anisotropy` unit | Unit "×", the level (2× to 16×); `details` says the hardware caps the level (`RB_SetAnisotropy_core`). |
| `snd_volume` | Details say the engine value is 0 to 1 and the game's Master Volume slider uses the same scale. No `%` unit was added because the stored value is not a percentage; the UI's own conversion stays valid. |
| "Sound quality" row (`snd_khz`) | `control.unit` kHz and labels 11 Low, 22 Medium, 44 High; the sound driver accepts only 11, 22 and 44 and otherwise uses 22 kHz with a warning (`SND_InitDriver`). **Not made Guided**: `tests/launcher/ConfigStudioModelTests.swift:55` pins exactly 22 Guided entries. |
| "Connection presets" row | `rate` carries labels 5000 ISDN and 25000 LAN, cable or DSL from the game's Connection menu. `cl_maxpackets` and `cl_packetdup` say the game's set-rate step (`UI_RunMenuScript`) sets 30 and 1 for any rate of 5,000 or more. There is no preset field in the schema, so this is labels plus text. |
| group blurbs | Not possible from copy: `groups` in the catalog carry only `id` and `title` (generator-owned). The blurbs stay in `ConfigStudioModel.swift`. |

## How the copy was grounded

Order of precedence for every sentence: the exported source at `048ff5e` (the port's own code), then the research files, then the shipped menu data in `dvars.json`.

- **Engine source (048ff5e export):** `cl_input.c`, `cg_view_mp.c`, `cg_draw_mp.c`, `cg_newDraw_mp.c`, `cl_main_mp.c`, `cl_main_pc_mp.c`, `cl_console_mp.c`, `cl_parse_mp.c`, `common.c`, `ui_main_mp.c`, `snd_driver.c`, `mac_input.c`, `linux_input.c`, `macos_rawmouse.m`, `macos_input_policy.h`, `macos_display.c`, `CDirect3DDevice.c`, `rb_state.c`, `r_model.c`, `r_dvars.c`, `win_net.c`, `cod2x_runtime.c`, `cod2x_features.c`, `cg_cod2x_radar.c`. Three registered settings (`input_viewSensitivity`, `cl_analog_attack_threshold`, `cl_stanceHoldTime`) and `r_sse_skinning` are only ever registered; the copy says so.
- **Research:** `_agents/research/dvars/` (`summary.md`, `dvars.json` menu lists, shipped config values, CoD2x descriptions), `_agents/research/web/` (`summary.md`, `dvar-notes.json`, topics), `_agents/research/rulesets/summary.md` (ESL, ClanBase, CyberGamer, WWCL, zPAM, CoD2x competitive clamp). Where a note was tagged INFERENCE or UNCORROBORATED the copy either says "inferred from its name" or says less.
- **Tone:** the Guided board and the "07 · Writing" section of the Design Rules board (sentence case, second person, units on numbers, outcomes not mechanism, calm). Titles are at most four words; every summary is one sentence; no console name containing an underscore appears in any title, summary or detail (the merge step checked this by pattern). Where the source does not establish a unit the number is left bare rather than given an invented one (on-screen sizes and offsets, for example).
- No web browsing was needed; no new URL was fetched.

## Judgement calls

1. **New additive `control` keys.** `ConfigCatalogControl` decodes only `kind`, `step` and `unit`, so `labels` and `practicalRange` are ignored by the launcher until a UI unit reads them. They carry no behaviour now, change no schema version and do not break the Codable decode (`config_catalog.sh` passes). Each label set is also written into `details`, so Expert shows it today.
2. **Label wording.** Extra/High/Normal/Low, Off/Low/Normal/High, ISDN, "LAN, cable or DSL", Automatic/Manual, the browser filter choices and Internet/LAN/No are the game's own menu words (its dvarfloatlist values), not translations. `snd_khz` (Low/Medium/High) and `r_presentMode` (Show every frame/Skip while busy) are my words for source-established values.
3. **Regrouped 138 entries** so the Expert rails make sense (`group` is an allowed copy key; no test depends on group sizes): 52 server-rule and debug names from Other to Network, 26 on-screen text and icon settings from Other to HUD & crosshair, 25 view, FOV, tracer and viewmodel settings from Other to Visibility, 24 sound and voice settings from Other to Audio, 7 keyboard and mouse input names from Network to Aim & input, plus three one-offs (`r_drawSun` to Visibility, `r_presentMode` and `timescale`/`fixedtime` to Frame & movement). The full before/after is `git diff defeb5a -- tools/catalog/copy.json`.
4. **Corrected earlier copy that was wrong or misleading:** `cg_drawRadar` was described as a mode picker but is registered as a switch, and it is a spectator/demo tool, so it is now described as such; `con_restricted` was "read" copy on an editable setting and the game's menu shows it inverted (Enable Console: Yes is 0); the browser filters `ui_browserShowDedicated` and `ui_browserShowPure` read as "show" but the menu means "only"; the research notes call `cl_masterServer` and `cl_masterPort` gone since CoD2x 1.4.3.3, but this version registers and reads them (`CL_GlobalServers_f`), so they are described as live; `con_restricted` also began "Read…" although it is editable. `Speed up on fast movements` was five words, so it is now "Mouse acceleration".
5. **Competitive context** is only ever "servers using X enforce Y" or "ESL's old PunkBuster list required Z". Nothing says a player must do anything. FOV (`cg_fov`, `cg_fovScale`, `cg_fovMin`) is described as protected; the copy says whether a config can set it is untested and offers no control.
6. **`in_rawmouse`** says the game reads the mouse through Apple's GameController framework while you play; it does not repeat the board's "without macOS pointer acceleration" because the source does not establish that.
7. **The in-game Connection menu** is "expected to" run the set-rate step: the step itself is in `ui_main_mp.c`, but the menu text that calls it is in game data we do not have (research marks this INFERENCE).
8. **Name-derived wording** is always flagged ("inferred from its name", "its name suggests") and the count is listed below.

## Entries that stay low-information

These are described honestly and briefly because no source documents more. None is shown as a recommendation.

- **Effect not documented (48):** `activeAction`, `cg_drawScriptUsage`, `cl_shownuments`, `com_animCheck`, `com_statmon`, `developer_script`, `fs_debug`, `fs_useOldAssets`, `g_listEntity`, `inertiaAngle`, `loc_warnings`, `loc_warningsAsErrors`, `motd`, `player_breath_fire_delay`, `player_breath_gasp_lerp`, `player_breath_gasp_scale`, `player_breath_gasp_time`, `player_breath_hold_lerp`, `player_breath_snd_delay`, `player_breath_snd_lerp`, `player_dmgtimer_flinchTime`, `player_dmgtimer_maxTime`, `player_dmgtimer_minScale`, `player_dmgtimer_stumbleTime`, `player_dmgtimer_timePerPoint`, `player_turnAnims`, `r_debugEntCounts`, `r_ignore`, `r_ignorehwgamma`, `r_logFile`, `r_outdoorAwayBias`, `r_outdoorDownBias`, `r_outdoorFeather`, `r_portalBevels`, `r_portalBevelsOnly`, `r_portalMinClipArea`, `r_pvsStats`, `r_testFill`, `r_testFillEnable`, `r_testTransform`, `r_vc_makelog`, `r_vc_showlog`, `sv_allowedClan1`, `sv_allowedClan2`, `sv_debugReliableCmds`, `sv_packet_info`, `sv_showAverageBPS`, `sv_showCommands`.
- **Role inferred from the name (37):** `bg_bobAmplitudeDucked`, `bg_bobAmplitudeProne`, `bg_bobAmplitudeStanding`, `bg_bobMax`, `bg_foliagesnd_fastinterval`, `bg_foliagesnd_maxspeed`, `bg_foliagesnd_minspeed`, `bg_foliagesnd_resetinterval`, `bg_foliagesnd_slowinterval`, `cg_viewsize`, `g_clonePlayerMaxVelocity`, `g_dropForwardSpeed`, `g_dropUpSpeedBase`, `g_dropUpSpeedRand`, `g_no_script_spam`, `g_useholdtime`, `g_voiceChatTalkingDuration`, `g_voteAbstainWeight`, `g_weaponAmmoPools`, `mantle_check_angle`, `mantle_check_radius`, `mantle_check_range`, `mss_q3fs`, `player_adsExitDelay`, `player_toggleBinoculars`, `r_depthPrepassModels`, `r_glowBloomCutoff`, `r_glowBloomDesaturation`, `r_glowBloomIntensity0`, `r_glowBloomIntensity1`, `r_glowRadius0`, `r_glowRadius1`, `r_glowSkyBleedIntensity0`, `r_glowSkyBleedIntensity1`, `r_monitor`, `r_objectiveColorDx7Max`, `r_objectiveColorDx7Min`.
- **Registered but never read in this build (4):** `cl_analog_attack_threshold`, `cl_stanceHoldTime`, `input_viewSensitivity`, `r_sse_skinning`.

A further group is developer, debug and engine-internal state (the `r_show*`, `sc_*`, `fx_*`, `player_offset*` and `ui_*` families, read-only status such as `sv_running`): they say what they are, whether the game protects them, and give the range and default, but no behaviour beyond what the registration and notes establish.

## Not done, for the orchestrator

- **Swift side:** nothing reads `control.labels` or `control.practicalRange` yet. A UI unit can turn `r_picmip` History into "Texture detail High → Normal" and show a labelled `snd_khz` control. `ConfigValues.display` prints the `r_anisotropy` unit as "8 ×" (with a space) where the Picker prints "8×".
- **`snd_khz` as a Guided row** needs `ConfigStudioModelTests.swift` updated from 22 to 23 first.
- **Commands:** 169 of 235 commands still carry the generic command copy; this unit covered settings only.
- **Group blurbs** and the Guided tips remain in Swift.
- `m_enable`, `r_vsync`, `r_anisotropic` and the two `scr_*` entries are supplemental references and were not edited.

## Verification

Everything ran under `taskpolicy -b nice -n 19`. The source export is `git -C /Users/stix/Projects/cod2-native archive 048ff5e` extracted into a scratch directory (never checked out).

- **Baseline:** before any edit, regenerating from the untouched `copy.json` reproduced the committed `ConfigCatalog.json` byte for byte (`cmp`).
- **Per commit:** `python3 tools/catalog/catalog.py generate --source-root <export> --source-revision 048ff5e --copy <snapshot of copy.json> --output <path>` ran twice to distinct paths and the two outputs were `cmp`-identical; the installed resource is that output and equals a faster cached-facts regeneration used while iterating. Then `python3 -m unittest discover -s tools/catalog -v`: **24 tests OK**, and `sh tests/launcher/config_catalog.sh`: **PASS: ConfigCatalog Swift 6 tests (733 settings, 235 commands)**. Both passed for all eleven copy commits.
- **Source coverage gate, final catalog:** `catalog.py check --mode stock`: 672 settings, 223 registered commands + 8 dispatch-only, **0 differences**; `--mode cod2x`: 728 settings, 227 registered commands + 8 dispatch-only, **0 differences**.
- **Wider catalog consumers, final catalog:** `config_studio.sh`, `config_checkup.sh`, `config_rules.sh`, `config_ready_check.sh`, `config_import_session.sh` and `config_bindings.sh` all pass (their last lines are PASS).
- **Not run:** the full `tests/launcher/run.sh` and the snapshot stage. No Swift, build list or test changed, and the session-lock failure recorded in WS103 is unrelated.
- Each cited engine function in `effectSources` was found by name in the export; no cited path is missing.
- Leftovers: the scratch export, helper scripts and generated outputs lived under `/tmp` and were removed; no `.app` was built or registered, nothing was pushed or tagged, and nothing outside this worktree was written.

## Commits

| Commit | Subject |
| --- | --- |
| `6867a85` | Ground the catalog copy for the WS103 gap list |
| `4855804` | Rewrite the copy for menu-exposed and Guided settings |
| `e96347f` | Write grounded copy for the Aim & input settings |
| `6991caa` | Write grounded copy for the Network settings |
| `0028954` | Write grounded copy for the Visibility settings |
| `59d92d2` | Write grounded copy for the HUD & crosshair settings |
| `55426eb` | Write grounded copy for the Audio settings |
| `e276766` | Write grounded copy for the Graphics settings |
| `fd7c043` | Write grounded copy for the Frame & movement settings |
| `7dbbc71` | Write grounded copy for the Other settings and regroup by purpose |
| `3aace3d` | Cite the engine functions behind the verified copy claims |

This report is the twelfth commit. All carry DCO sign-off. `r_gamma`, a Guided and menu-exposed setting, was written in the Visibility commit rather than the menu-exposed commit because it belongs to that group.
