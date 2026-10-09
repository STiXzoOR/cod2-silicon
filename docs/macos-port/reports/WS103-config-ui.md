# WS103 — Config Studio F3b: Configs, Settings (Guided/Expert) and History

2026-10-09. Worktree `config-ui-f3b`, branch `codex/config-ui-f3b`, on `codex/config-studio-int` plus F4 (catalog) and F3a (launch/lifecycle) at `0e96f3c`. This unit builds the Configs page and moves personal settings out of app Settings. It binds to the F3a session and the F2 store; it adds no engine, Theme or Config Studio presentation-primitive change. Evidence: `~/Library/Application Support/CoD2-native-current-readiness/WS103.noindex/` (logs and review renders; nothing from it is committed).

## What was built

**Navigation.** `LauncherPage.configs` sits between Library and Settings (SF Symbol `square.stack`, Command-4). The page is an `HStack` inside the existing two-column split: a 252-point loadout column (title, My Loadouts, New Loadout, footnote) and the loadout detail. Selecting a loadout focuses it for viewing and editing only; **Use for Next Game** (header button or the More menu) is the one control that writes `active.txt`. Arrow keys move through loadouts, groups and snapshots.

**Toolbar.** A native segmented control in the principal slot switches Settings and History. The trailing group has **Jump to Any Setting** (Command-K, only while Configs is shown), **Save a Snapshot Now** on History, and a More menu (New Loadout…, Duplicate…, Rename…, Use for Next Game, Show in Finder). `ConfigStudioTab.available` lists only Settings and History; Binds (B1/B2) and Check-up (A1) add their cases when their screens exist, so no tab is a dead end.

**Header.** Stencil eyebrow (“Loadout · In use” or “Loadout”), 46-point stencil name, provenance and the last History time in Courier. Guided shows the in-use badge, Expert its “N changed from the game’s defaults”, History its source filter. A `ViewThatFits` wraps the controls under the name rather than truncating.

**Guided.** The catalog's 22 `guided` entries in the eight catalog groups, in a fixed order of importance, on the existing opaque `SettingsGroup`/`SettingsRow` panels: switches, enumeration menus, sliders with a plain readout when the registered range has at most 200 steps, a field otherwise, frame-cap crates (125/250/333 from `FrameCap.presets` plus your own number, clamped to the catalog's 0–1000), Normal/Inverted for the signed `m_pitch` scale (magnitude kept), filtering levels as powers of two inside the catalog range, the colour-coded name preview, and a mouse DPI row (loadout metadata; cm per full turn uses the effective `m_yaw`). “All N settings ›” opens Expert at that group. Below 480 points of editor width a group menu replaces the rail.

**Expert.** Every catalog setting that `isEditable(in: .cod2x)` (425), grouped, guided entries first. Each row: plain title, changed dot, lock glyph, Courier console name, “Servers can set this” for `serverControlled`, a typed control (switch + On/Off, segmented frame cap + field, slider + field + unit, menu, field, secure field for passwords, vector field), Reset (to the variant default), Lock and Note (popover). Launcher-owned display settings (`GameSettings.machineDvarNames`, e.g. `r_swapInterval`) are read-only rows with **Change in Settings**, because the launcher overrides them at every start. The 303 registered settings the game protects or keeps read-only (FOV among them) never become rows; each group says how many it has. Unavailable, reference-only and script/mod entries are not shown. Changed and Locked filters, Find a setting (Command-F), Compare with another of your own loadouts (blue diamond value, slider marker, **Take Theirs**, “Locked, so it stays yours”). Rows render in a `LazyVStack` with segmented panel backgrounds so the 127-row Other group stays fast.

**Edit as Text.** An `NSTextView` (native undo/find, no smart quotes) in an opaque well, coloured from the engine tokenizer, with blocking problems underlined in red and notes dotted. A diagnostics list gives line, column, plain message and excerpt, and selects the text. Problems that block Apply: characters above Latin‑1, unclosed quotes, over-long lines, 512-word lines, a config over 65,535 bytes. Notes: settings the game doesn’t register, protected settings, a NUL cut-off. Apply writes the exact text bytes as one “Edited as text” transaction; Revert reloads. A newer file on disk blocks Apply until Revert. Leaving text mode, the tab or the loadout with unapplied edits asks Apply / Discard / Keep Editing; Jump to any setting selects the line instead of discarding edits.

**History.** “Now” plus every snapshot, newest first, on a timeline with source glyphs and badges, plain change summaries (“Mouse speed 2.8 → 2.6.”), Courier times, a source filter (All, You, Imports, Check-up, Game) and the retention note. The 372-point inspector compares a snapshot with now in plain names and units (key bindings, other lines, locks, notes, name and DPI too), **Restore This Snapshot** (disabled when nothing differs), and “Restore the State Before This Change”. Restore and its undo are transactions; the success status reads “Restored. Your previous state is saved as a snapshot.” Below 760 points the inspector stacks under the list.

**Commit, undo and failure.** Each gesture is one `ConfigStudioSession.update` transaction: switches and menus on change, sliders at the end of a drag (keyboard steps after 0.5 s), fields on Return or focus loss, Escape reverts. Every setting, lock, note, DPI, rename, text and restore change registers an undo (Command-Z, Edit menu “Undo Change Mouse speed”) that is itself a transaction; whole-document undos refuse if the file changed since. A failed save keeps the session's draft on screen with a persistent banner: **Try Again** discards the stale draft and reapplies the same intent to the latest disk version (never the stale whole document); **Discard Change**. A session read error offers Try Again and an explicit, confirmed **Keep Version on Disk** recovery (`inspectRecovery`/`keepDiskVersion`). Successful saves are announced once to VoiceOver.

**Running game.** While a game is launched or attached, a calm banner reads “Configs are read-only while Call of Duty 2 is running. Changes you make in game appear here as the game saves them.” (contract wording; the board's “when it closes” is superseded by the live watcher). Mutating controls disable; viewing, searching and comparing continue.

**Settings.** Settings now holds Game data (with Show in Finder), Display (resolution, fullscreen modes, vertical sync), Renderer, Updates (version, release check) and Advanced launch options, with a link card into Configs. The inert render-scale/MetalFX/HDR rows became the board's single renderer row. Until the session reports `migrationCompleted`, the launcher still applies its own frame cap, mouse, volume and filtering, so those controls stay in Settings under a caution banner — the only case where they appear.

## Model, session and API additions

- `ConfigStudioSession.focus(_:)`, `focusedProfileID` and `read(profileID:)` (commit `3339012`): view and edit any loadout or read one for comparison without touching `active.txt`; `prepareLaunch` always launches the active profile. A focused loadout deleted by the game returns the view to the active one. Test: “a viewed loadout is edited and read without changing the active selection or launch profile”.
- `refresh()`/`reconcileGames` now assign published state only when it changed, so the root's 250 ms poll no longer redraws Configs four times a second (commit `2ef0faa`). The poll still reads every profile and up to 50 History files per tick; that is F3a's existing I/O cost, recorded here for the orchestrator.
- `ConfigStudioModel` (main-actor view store): catalog loading off the main thread (bundle `Resources/ConfigCatalog.json`, or beside a development executable), `ConfigSettingSpec`/`ConfigValues` (config text ⇄ typed value ⇄ plain display: bools as `1`/`0`, enumerations by name as the game writes them, vectors space-separated, numbers without locale or grouping), per-document value cache, Guided/Expert/History/text intents, undo, failures and announcements. `ConfigDiff` and `ConfigTextCheck` are pure and tested.
- New loadouts get a profile folder from the name (ASCII, `_` for spaces, de-duplicated case-insensitively, at most 36 + suffix characters) because the game’s own profile menu shows the folder; Rename changes only the display name and says so.

## Verification

All compiles and tests ran under `taskpolicy -b nice -n 19`, with `TMPDIR` inside the evidence directory, never more than two compile jobs at once.

```sh
evidence="$HOME/Library/Application Support/CoD2-native-current-readiness/WS103.noindex"
TMPDIR="$evidence/scratch" sh tests/launcher/config_launch.sh     # 10 groups, incl. the new focus/read case: PASS
TMPDIR="$evidence/scratch" sh tests/launcher/config_studio.sh     # new: 10 groups: PASS
TMPDIR="$evidence/scratch" COD2_LAUNCHER_TEST_OUTPUT="$evidence/suite-final" \
  taskpolicy -b nice -n 19 sh tests/launcher/run.sh               # see below
bash scripts/build-launcher.sh "$evidence/build/app"              # current SDK 27.0, minos 13.0: exit 0
harness="$evidence/suite-final/snapshot-harness/LauncherSnapshots" # built by run.sh's snapshot stage
"$harness" "$evidence/shots/actions-final" --actions               # real clicks: PASS (both)
git diff --check 0e96f3c..HEAD                                    # clean
```

`tests/launcher/run.sh` at `8706151` (`logs/suite-final.log`): Core tests, local UDP, artwork, F1 document, F2's 14 profile cases, all ten F3a launch groups, all ten Config Studio model groups, the F4 catalog (733 settings, 235 commands), I1 import, bundled fonts, the Theme-only button-style check and the strict Swift 6 / macOS 13 material-branch typecheck (now including `ConfigCatalog`, `ConfigStudioModel`, `ConfigStudioStyle` and the three new views) **pass**. The snapshot stage rendered all 38 captures (13 existing screens and the six new Config Studio screens, dark and light) with **216 shapes checked and 0 findings**, then **failed** at the existing `dark != light` assertion for `home`: every capture is the same blank image (one MD5 for all 38) because the login session is locked (`"CGSSessionScreenIsLocked"=Yes` before the first suite run, `logs/lock-state-before-suite.txt`, and again after the final one). That is the known WS101/WS104 environment failure, not a rendering result; no assertion was relaxed. Because `snapshots.py` stops there, its new Config Studio stages (material fallback, Increase Contrast, Reduce Transparency, actions) were run directly with the harness instead; their results are below.

Model tests drive the same intents the controls call against private temporary homes and compare exact bytes: a control edit rewrites only the value bytes and adds one History row (`beforeData` = the old file); undo and redo restore the exact bytes; reset, lock, note, DPI, rename and Take Theirs (refused while locked); viewing a loadout never touches `active.txt` until Use for Next Game; every intent refuses while a game PID is tracked; an externally changed file never gets overwritten and Try Again reapplies the one intent to the new version; invalid or non-Latin‑1 text never reaches disk, valid text is written byte-for-byte as one transaction and a newer file blocks it; History diffs use plain names and units and Restore is a new `.restore` transaction; new loadouts (copy or defaults) never change the active loadout. The harness's actions pass clicks the real **Show crosshair** switch in the window (hit-testing, the switch's own action) and checks that the temporary home's `config_mp.cfg` gained exactly `seta cg_drawCrosshair "0"` in canonical order plus one “Changed Show crosshair” row, then clicks **Restore This Snapshot** and checks the snapshot's exact bytes and a new restore row.

## Canvas comparison

Window captures could not be used: the session stayed locked, so every `CGWindowListCreateImage` capture was blank. All visual comparison used the harness's offscreen `--scale2` renders (material styling; the system draws native glass flat there, and two known render artifacts — the sidebar list and the selected segment of toolbar pickers draw blank — appear identically on the existing Library screen). Inspected at original resolution against the boards: Guided (Frame & movement, Aim & input), Expert with comparison, Edit as Text with a blocking issue, History with a selected snapshot, the read-only banner, and Settings; each in dark and light (`shots/r3`, `shots/r7`), under Increase Contrast and Reduce Transparency (`shots/r5-*`), with the forced material fallback (`shots/r8-fallback`, 0 shape findings), and at the 1100 × 760 minimum (`shots/r6-min`). Not verified: native Liquid Glass pixels in a real window capture, the Jump palette, New Loadout and Rename sheets (separate windows the harness doesn't capture), and a VoiceOver walk-through; these need an unlocked session. Two layout defects found this way were fixed before commit: a long banner made AppKit's zero-width minimum-size measurement push the whole page off the window, and the Expert tools squeezed “Compare with” into one letter per line at the minimum size.

Matches (structure, hierarchy, spacing, copy tone, states): ConfigsGuided and ConfigsLight (loadout column, header, rail, opaque groups, crates, footnote, light palette), ConfigsExpert (tools row, rail with counts and Changed/Locked filters, rows with console names, reset/lock/note, compare diamond and Take Theirs, note line, empty “ALL CLEAR”), the Edit-as-text well, History (timeline, nodes, badges, Courier times, inspector, THIS IS NOW, restore footer and retention note), the read-only banner, and Settings (header subtitle, Configs link card, two-column machine sections).

Differences, with reasons:

| Board | This build | Why |
| --- | --- | --- |
| Tabs pill left-aligned at the content column, four tabs | Native segmented control in the toolbar's principal slot; Settings and History only | A two-column split's toolbar can't anchor an item to the content column without a fake spacer; Library already uses principal. Binds/Check-up arrive with B1/B2 and A1. |
| 186-point rail (contract: 160) | 200 points | SF Pro needs 200 for “Frame & movement”/“HUD & crosshair” with counts; names must not truncate. |
| Guided tips (“Use 100”, “Open Check-up”) | None | Contract: tips come from rule evaluation; A1's rules are not in this branch. |
| Ruleset picker, rule badges, violation counts | None | Same; values must come from verified ruleset data. |
| Compare with players | Compare with your own loadouts | Players come from the Directory (I2). |
| Discover › Players / Saved in the loadout column | Omitted | I2. No placeholder rows. |
| Text edits “update the controls as you type” | Explicit Apply/Revert with validation | Contract safety boundary for freeform text. |
| Text gutter with line numbers | Line/column in the diagnostics list | NSTextView has no gutter; a ruler view is a follow-up. |
| Mouse speed slider 0.5–10 | Field (catalog range 0.01–100) plus DPI row | No invented range. |
| Speed-up switch | Slider 0–100 | Catalog type is a float 0–100. |
| “Wait for the display” in Frame & movement | Vertical sync stays in Settings › Display | `r_swapInterval` is machine-owned; a loadout value would be overridden at launch. |
| Sound quality, Connection presets, Texture sharpness labels | Not shown / numeric | Not guided in the catalog, or the catalog has no value labels. |
| Crate captions “Original feel / Common server cap / Highest jumps” | “Low / Default / High” | Existing `FrameCap` copy; board captions make claims the catalog doesn't. |
| History titles as outcomes (“You’ll turn a little slower”) | “Changed Mouse speed” with the plain diff below | Outcome copy per setting isn't authored. |
| Settings: auto-update, release channel, match summaries | Not shown | No backing feature yet; no disconnected controls. Window mode kept (four modes). |
| Group blurbs | Board blurbs, trimmed to what the catalog supports | e.g. “How often you and the server talk.” without “hits land where you aimed”. |

## Copy gaps (for the catalog owner)

Guided settings shown with catalog copy but missing presentation copy: `r_picmip` value labels (shown 0–3; History reads “Texture detail 1 → 2”); `cl_mouseAccel` unit/step; `sensitivity` practical range; `r_anisotropy` unit (UI shows “×”); `snd_volume` shown as a percentage by UI convention; group blurbs. The board's Guided rows without guided catalog entries: Wait for the display (machine-owned), Sound quality (`snd_khz`), Connection presets. In Expert, 338 settings carry the honest generic “engine reference” copy and mechanical titles (“In mouse”, “M forward”). The DPI row's copy is launcher copy for launcher metadata, not catalog copy.

## Left for later units

- **B1/B2 Binds:** the Binds tab (`ConfigStudioTab.binds`), device popovers, advanced binds and chain files.
- **I2 Import/Directory:** Discover › Players and Saved in the loadout column, compare with players, import sources and review (History already labels Import rows).
- **A1 Check-up:** the Check-up tab, Guided tips, Expert ruleset picker and rule badges (hook points: `GuidedRow`, `ExpertRow` badges, `ExpertTools`).
- Command-1 for the Deploy page: the File menu already has a “Deploy” action (Command-Return); naming the page item needs a decision.
- Deleting a loadout (no store API), a line-number gutter, and the Jump palette/sheets in screenshots (sheets are separate windows the harness doesn't capture).

## Composition

Compose in order: `3339012`, `2ef0faa`, `fa6077e`, `62dee71`, `e26db79`, `42420bf`, `e55c659`, `8706151`, then this report. All carry DCO sign-off and the requested author email. The branch depends on F3a's session API and F4's catalog resource; `scripts/build-launcher.sh`, `tests/launcher/run.sh` (new `config_studio.sh`, extended typecheck list) and `tools/cod2x/make_macos_app.py` (copies `ConfigCatalog.json` into the bundle) carry the integration. A1's `ConfigRules.swift`/`ConfigCheckup.swift` touch none of these files; B1 and I2 will need their files added to the same two lists. WS102 is still required before per-profile CoD2x end-to-end acceptance (WS101).

No push, tag, remote change or merge. No `.app` bundle was built or registered with LaunchServices; harness and test binaries, test homes and scratch folders were removed afterwards, leaving only logs and review renders under `WS103.noindex`. Nothing was written to `~/Library/Application Support/CoD2 Silicon/`. Nothing is left uncommitted.
