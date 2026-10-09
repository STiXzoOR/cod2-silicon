# WS114 — Config Studio: the Binds and Check-up tabs

2026-10-09. Worktree `config-ui-f3b`, branch `codex/config-ui-binds-checkup`, built on `codex/config-studio-int` at `defeb5a` (WS103 rebased onto B1 binds and devices, B2 chains, A1 rules and Check-up, I1/I2 import and directory, A3 ready check). This unit adds the Binds and Check-up tabs to Configs, the Guided tips and Expert rule badges deferred in WS103, and harness coverage for both tabs. It uses the B1, B2 and A1 models as they are; it changes no engine source, no Theme primitive and no model unit's behaviour. Evidence: `~/Library/Application Support/CoD2-native-current-readiness/WS114.noindex/` (logs and review renders; nothing from it is committed).

## What was built

**Tab bar.** Configs now has Settings, Binds, Check-up and History (`ConfigStudioTab.available`). The content area switches on `ConfigStudioModel.destination` before the tab. Today `destination` has one case, `.loadout`. Discover adds its own case and view there and in the loadout column; nothing in this unit builds or stubs Discover. The header shows List / Visual on Binds and Tune for (Competitive, Balanced, Best looking) on Check-up. The More menu gains **Export for Windows…**, which writes B2's stock-compatible config through a save panel. B2 refuses an export into the game's own home, and success shows a dismissible notice.

**Binds › List.** Search matches an action or a key name. The catalog's game actions are grouped as Movement, Combat, Communication and Game & UI. Each row has a primary and a second key chip, and other keys bound to the action are listed under its name.
- Pressing a chip listens for the next key, mouse button or wheel move through B1's `ConfigBindingCapture`. The chip reads "Press a key…". The console key and keys the game can't use stay listening and show B1's reason.
- Escape, focus loss (window or app), leaving the tab and a game starting all cancel listening. Nothing is written in those cases, and B1 drains held keys.
- If the key already does something else, the row shows an inline review: "G already does “Throw frag grenade”. Use it for Reload instead? “Throw frag grenade” loses this key." The choices are **Use G for Reload** and **Cancel**.
- A chip's context menu clears or changes its key.
- **Other binds** lists keys bound outside the catalog's actions, described in plain words with their exact line. A key owned by an advanced bind offers **Edit…**; any other key offers **Clear**. Nothing is ever rewritten unless changed.

Every write is one guarded transaction (`ConfigStudioSession.mutate` → `ConfigBindings.apply`) with one History row and Undo.

**Binds › Advanced binds.** This panel sits beside the list in wide windows and below it in narrow ones.
- Each card shows the key, a summary, what each press does and the kind. **New** starts a draft.
- The editor has:
  - key capture
  - an "Each press" segment: Several (commands run together on one press), Toggle (a setting from the catalog's toggleable list) or Cycle (steps in order, one per press)
  - steps with a kind (Set my name to, Say to everyone, Say to team, Do an action, Run a console command), text, reordering and removal
- **Press preview** comes from B2's `ConfigChainMachine`. It runs the saved profile, then the chain file, then key events. Toggles show the value each press leaves; cycles show each step, then "back to the start". Release effects are shown for Several, and incomplete traces are flagged.
- **Notes:**
  - "Competitive CoD2x servers turn off wait, so there everything in this bind happens in the same frame."
  - other wait and release behaviour
  - the 1,022-character step limit
  - files or settings the bind needs that the loadout doesn't have
  - the setting-slot count
- I1 review labels (chat, name change, key release, script and so on) need an explicit OK before saving; blocked labels stop Save.
- B2's refusals appear in this screen's words, each saying what to change:
  - `helperTooLong`: “One step is longer than the 1,022 characters the game keeps. Split it into shorter steps.”
  - `unsafeQuotes` on the stock game: “The original game drops quote marks when it loads a bind, so text with spaces or quote marks can’t be saved here. Use single words, or play with CoD2x.” There are separate wordings for CoD2x and for Windows export.
  - every other `ConfigChainError` case has its own message.
- **Show the lines** reveals the exact `bind`/`set` lines. **Save Bind** asks before replacing a key that already does something or saving a bind that needs an OK. **Delete Bind** / **Discard** removes it.
- Saving or deleting is one B2 transaction with Undo.

In wide windows the panel scrolls on its own beside the list, so Save Bind stays reachable however long the editor grows.

**Binds › Visual.**
- **Device chips** open popovers.
  - The keyboard popover has key shape (ANSI/ISO/JIS), size and modifier labels. Each row has B1's explanation of where its value came from, and changes go through `InputDevicesModel.setOverride`. Layout comes from the macOS input source.
  - The pointer popover has mouse or trackpad, mouse DPI (loadout metadata, "you enter this") and polling rate **Not measured**.
- **Keyboard drawing.** Apple key positions by virtual key, for each shape and size. Labels come from the current input source and update when it changes; saved binds stay on the same engine keys.
  - Keys are coloured by what they do, with a legend; keys the game can't use are hatched.
  - Keys grow to fill the panel, from 28 to 44 points wide plus a 4-point gap, so a key is never below the 28-point hit target.
  - The keyboard is focusable, and arrow keys move to the nearest key in that direction on the drawing.
- **Pointer panel.** Mouse 1–5 and the wheel, or trackpad click, two-finger click and scrolling, each with its binding. The panel notes that buttons 6 and up can't be used by the original game.
- **Inspector** for the selected key or button:
  - the key cap, the plain action and its group, and what pressing it does
  - for advanced or external binds, the press/release preview from B2's `inspect`
  - the exact `bind` line
  - **Assign Action / Change Action** (a menu by group), **Clear**, and **Edit Steps…** for advanced binds
- **Find by Pressing…** selects the next key or button pressed.
- Below 1,040 points of content width the panels stack: keyboard, inspector, pointer. Below 720 points the inspector's buttons move under its text.

**Check-up.**
- **Rules.** A rules menu selects one or more rule sets for this client mode; the default is Public CoD2x. A status line says how many settings break them, whether the rules disagree, or that the loadout follows them. **Show Rules** lists every judged setting with:
  - its plain value and status ("Set by the server" where it applies)
  - A1's reason and its first source
  - "Elsewhere" notes for conflicts in rule sets that weren't picked
- **Dials** reflect only what is known:
  - Movement shows the frame cap ("From settings"); the real frame rate is Not measured.
  - Aim shows cm per full turn from sensitivity, `m_yaw` and the DPI you entered; polling rate is Not measured.
  - Connection is Not measured.
  - Visibility reads the chosen rules for the visibility settings, or says why it can't.
- **Info banner.** It says advice comes from settings and rules, not measurements.
- **Suggestion cards** (A1's, for the chosen rules and tuning). Each card shows:
  - the title, why, "You'll notice", the evidence and sources
  - **Apply** (prominent on the top card), **Show Me** (the exact changes in plain names, such as "Frame rate cap: 333 fps → 250 fps", each with **Open in Expert**) and **Not Now**
- **Applied cards** read "Applied: … Saved to History." with **Undo**. Undo uses A1's receipt, so it restores the exact bytes as a new transaction.
- **Not Now** is per loadout, rules and tuning, and a count offers **Show Again**.
- **Limitations and footer.** A1's limitations are listed, followed by a footer linking History and Expert.
- FOV is never suggested; A1 never offers it and a test checks that. Contradictions between rule sets are shown as "Rules disagree", never resolved silently.
- **Advanced binds hold advice back.** A1 gives no suggestions when a loadout contains lines it can't follow before the game runs them, and that includes any advanced bind. The screen says so: "Check-up can’t suggest changes for this loadout. Its advanced binds change values while you play, …". It does not show "Nothing to suggest".

**Settings.**
- **Guided.** A row whose setting breaks the chosen rules shows a tip. The tip is the rule's reason, with the console name replaced by the plain name. When A1 has a matching suggestion, the tip offers a one-click fix ("Use 250 fps").
- **Expert.** The tools row adds the rules menu and a count of settings that break them. Each row that a rule covers carries a badge: the range ("125 fps–250 fps"), the exact value, "Set by the server" or "Rules disagree". Rows that break a rule show the reason and a faint tint.

**Running game.** Binds and Check-up follow the WS103 read-only state: mutating controls disable, listening stops when a game starts, and Apply refuses.

## Model and session changes

- **`ConfigStudioSession.mutate(profileID:_:)`** (commit `9fe61dc`) lends the store to one write. It refuses while a game runs, while a draft is pending, or when the loadout on screen isn't the one named, and it refreshes afterwards. B1 and A1 keep their store-based APIs, and every write still goes through the same guards as `update`.
- **`ConfigBindsModel` and `ConfigCheckupModel`** are view stores in `ConfigStudioBinds.swift` and `ConfigStudioCheckup.swift`. They hold no copies of config data and recompute from the session's current version.
- **Additions in this unit:**
  - `selectedVirtualKey` for Visual
  - `pressPreview` toggle values
  - `holdBack`, which detects lines A1 can't follow the same way A1 does
  - `ConfigCheckupModel.plain` for Guided tip wording
  - a `height` on `StudioKeyCap`
- **Capture window.** The window that capture listens to is now reported when the Binds view joins a window (`viewDidMoveToWindow`). The first version read it asynchronously and could miss it, so the first click on a key chip showed "Can't listen for keys here". The harness's real-click action found this.

## Verification

All compiles and tests ran under `taskpolicy -b nice -n 19`, with `TMPDIR` inside the evidence directory, never more than two compile jobs at once. The session was unlocked and on the console for this unit's final runs (`CGSessionCopyCurrentDictionary`: no `CGSSessionScreenIsLocked`, `kCGSSessionOnConsoleKey` 1). Window captures were therefore real, and dark and light captures differ.

```sh
evidence="$HOME/Library/Application Support/CoD2-native-current-readiness/WS114.noindex"
TMPDIR="$evidence/scratch" sh tests/launcher/config_studio.sh               # 22 groups (12 new for Binds and Check-up): PASS
TMPDIR="$evidence/scratch" COD2_LAUNCHER_TEST_OUTPUT="$evidence/run" \
  sh tests/launcher/run.sh                                                  # at 71e8f06: 79 PASS lines, then the snapshot stage (below)
TMPDIR="$evidence/scratch" python3 tests/launcher/snapshots.py "$evidence/run/snapshots"   # at 11bc5bf: PASS
bash scripts/build-launcher.sh "$evidence/build/app"                        # 0 warnings; minos 13.0, SDK 27.0
git diff --check defeb5a..HEAD                                              # clean
```

**`run.sh` at `71e8f06`** (`logs/run-final-71e8f06.log`) printed 79 PASS lines, covering every focused runner:
- Core, UDP, artwork, F1, F2 (15 cases), F3a launch (10)
- Config Studio (22: the 10 WS103 groups and 12 new ones)
- catalog (733 settings, 235 commands), I1 and I2 import, directory and its tools
- rules (8 policies, 133 rules), Check-up, bindings, input devices
- ready check, connection isolation, chains (with ASan/UBSan engine traces)
- fonts, the Theme-only button-style check, and the strict Swift 6 / macOS 13 material-branch typecheck, which now includes `ConfigBindsView` and `ConfigCheckupView`

The snapshot stage then rendered:
- 50 window captures (25 screens in dark and light, including the six new ones), 536 shapes, 0 findings
- the material fallback
- Increase Contrast and Reduce Transparency for all 12 Config Studio screens, 342 shapes each, 0 findings

It stopped at the WS103 assertion that every Reduce Transparency capture differs from the standard one. `configs-running-light` was pixel-identical. That assertion had never run against real captures. Reduce Transparency swaps glass for opaque fills, and Guided and the read-only screen have no glass in their content, so they legitimately capture identically; in dark they differed only by a few hundred anti-aliased pixels. `11bc5bf` keeps the requirement for every screen whose shape audit lists glass and still requires Increase Contrast to change every screen.

**The snapshot stage rerun at `11bc5bf`** (`logs/snapshots-final.log`) passes end to end, including `--actions` with four real-click checks:
- the **Show crosshair** switch and **Restore This Snapshot** (WS103)
- **Binds:** with the search set to “frag”, a synthesized click on Throw frag grenade's primary key chip starts listening; a G key press sent through `NSApp.sendEvent` saves exactly `bind g "+frag"` (expected bytes built with `ConfigDocument.setBind` from the file before) as one “Changed Throw frag grenade binding” row, and listening stops
- **Check-up:** under zPAM 4.08, a click on the frame-cap suggestion's **Apply** writes exactly `seta com_maxfps "250"` as a Check-up transaction, and a click on the applied card's **Undo** restores the file's previous bytes as a restore transaction

Only the snapshot test script changed between the two runs, so the 79 earlier groups stand for `11bc5bf` too.

**Focused tests** (`tests/launcher/ConfigStudioTabsTests.swift`, run by `config_studio.sh`) drive the stores with a fake ABC keyboard and mouse and a synthetic event source against private temporary homes:
- **Binds:**
  - rows and labels; capture and save of one exact bind
  - Escape, focus loss, game start and the console key never write
  - review and replacement of exactly one binding
  - Visual assign, clear, undo and find
  - advanced toggle/cycle preview values, review labels and confirmation, the wait note, the 1,022 refusal in plain words, delete, Windows export and its refusal into the game home
  - read-only while running; keyboard drawings for ANSI/ISO/JIS at every size
- **Check-up:**
  - suggestions, plain changes, dials, badges, tips and plain tip names
  - Not Now, Apply/Undo with exact bytes and undo-manager integration
  - contradictions, no rules and the running game
  - the advanced-bind hold-back

**Renders** (`shots/` and `run/snapshots/` in the evidence folder; nothing committed):
- window captures for every screen in dark and light, Increase Contrast and Reduce Transparency (`run/snapshots`)
- offscreen `--scale2` renders of the new screens in dark and light (`r4`, `r5`), Increase Contrast (`final-ic`), Reduce Transparency (`final-rt`) and the forced material fallback (`final-fallback`)
- the 1100 × 760 minimum (`r4min`), all 0 shape findings

Defects these found and fixed before the final commits:
- a short Visual layout that overlapped the pointer panel
- unequal device-chip heights and a non-concentric inspector button
- clipped keys and a wrapped legend at the minimum size
- Check-up dials that overflowed at the minimum size
- the unreachable Save Bind
- the capture window race

## Canvas comparison

Inspected at full resolution against BindsList, BindsVisual and CheckUp (and ConfigsGuided/ConfigsExpert for tips and badges): window captures at 1440 × 900 in dark and light, offscreen `--scale2` renders, Increase Contrast and Reduce Transparency, the 1100 × 760 minimum, and the forced material fallback.

**Matches:**
- **Binds list.** Search capsule, PRIMARY / SECOND headers, grouped opaque panels, dashed empty chips, the inline conflict strip, and the Advanced binds panel (cards with key cap, summary and kind chip; key, Each press segment, numbered steps, preview well, Show the lines).
- **Binds visual.** Device chips, keyboard panel with role colours and legend, hatched unavailable keys, pointer panel, and the selected-key inspector with key cap, title, group chip, description, Courier line, Change Action and Clear.
- **Check-up.** Tune for in the header, four dials, top-suggestion card with eyebrow, gradient, prominent Apply / Show Me / Not Now, applied card with Undo, set-aside count with Show Again, and the footer.

**Differences, with reasons:**

| Board | This build | Why |
| --- | --- | --- |
| Conflict strip “K is also X, so one press does both” with **Keep K for X only** / **Keep both** | “G already does “X”. Use it for Y instead? “X” loses this key.” with **Use G for Y** / **Cancel** | A CoD2 key holds one binding; B1 replaces or refuses. “Keep both” can't exist and “does both” would be untrue. |
| Advanced bind **Name** field | No name; cards are titled from what the bind does (“Show frame rate”, “Cycle of 3”) | `ConfigChain` has an id, key and behaviour, no display name. A name needs B2 metadata. |
| “Saved as you go” | Explicit **Save Bind** (with a confirmation when a key is replaced or a label needs an OK) and **Discard** | Each save is one reviewed B2 transaction; saving per keystroke would write half-built binds and fill History. |
| Turns: frame rate counter, connection graph, crosshair, voice chat | Toggle setting menu from the catalog's toggleable settings | Uses catalog titles; no invented list. |
| Layout menu (ABC (auto), British, German, French, Spanish, Nordic) | Layout shown as the macOS input source (“ABC · from macOS”), read-only | B1 reads labels from the current input source; it has no tables for other layouts. Change the input source in macOS and the drawing follows. |
| **Press keys to verify…**, **Learn button positions…** | Not built | No B1 flow behind them; no disconnected buttons. |
| Polling rate “1000 Hz · measured” | “Not measured” | Nothing measures polling. |
| “Tap to click is off in System Settings…” | “Trackpad clicks and scrolling reach the game as these mouse buttons and wheel moves, as set in System Settings › Trackpad.” | B1 doesn't read trackpad preferences. |
| Size “a guess from the keys you have” | Each row shows B1's own explanation of where the value came from | B1 states its basis per value. |
| Visual pointer panel beside the keyboard | Beside it at ≥ 1,040 points of content; stacked (keyboard, inspector, pointer) below that, which includes the 1440 × 900 window with the loadout column | The keyboard needs about 680 points at a comfortable key size; the board's content column is wider than the real one. |
| Dials with a bar and ratings (“MEDIUM”, “GOOD”, “CLEAR”) | Value and status, no bar; aim shows cm per turn; ratings only from rules | No measurement to fill a bar; ratings would be invented thresholds. With nothing measured the board's bars are empty too. |
| Movement dial “Not measured” | “From settings” (the cap) with “The frame rate you actually get isn’t measured.” | The value shown is a setting; the status says where it comes from. |
| “Get advice from your own games / Turn on match summaries” card | Info banner: advice comes from settings and rules; nothing claims a frame rate, ping or hit rate you didn't record | Match summaries don't exist; a button would be disconnected. |
| Header eyebrow “Check-up · Main” | The shared loadout header (“Loadout · In use / MAIN”) | One header for all four tabs. |
| Discover › Players / Saved in the loadout column | Not shown | Built by the Discover unit; this unit leaves `destination` for it. |

## Copy gaps

For the A1 rules owner: rule reasons name settings by console name (“CoD2x forces cl_maxpackets to 125 during competitive play.”, “com_maxfps must be within its cod2x registration range (0–1000); …”). Expert shows them as written. Guided substitutes the plain title mechanically, which reads well for most reasons, but some read awkwardly, for example “zPAM 3.36 pushes updates you receive = 30 on connect”. Plain-language variants are needed.

For the A1 optimizer owner: suggestion text uses jargon on the plain-language card:
- “picmip”, “mip levels”, “Manual mode … retains the saved mip levels” (`conservative-texture-detail`, `full-texture-detail`)
- “averages the current and previous samples” (`unfiltered-aim`)

For the B1/B2 owners:
- advanced binds need a display name
- B2's error descriptions are technical (“vstr cycle”, “1022-byte set value limit”, “exec/vstr”), so the launcher maps every `ConfigChainError` case to its own wording; B2 could adopt those
- keys with no drawn label and no readable name in the launcher read from the engine's spelling (“Kp Home”); B1 could supply names

For the catalog owner: the toggle menu uses catalog titles, some of which are mechanical for non-guided settings.

Launcher copy written here (all within the board's tone):
- review and refusal messages
- the hold-back banner
- dial details
- the popover footnotes
- the advanced-bind notes listed above

## Left for later

- **Advanced binds:** names (B2 metadata), autosave semantics if wanted, and a “Keep both” equivalent if B2 ever writes a combined chain for a conflicting key.
- **Keyboard and pointer:** a layout chooser independent of the input source; the key-verify and button-learn flows; measured polling rate.
- **Check-up measurements:** match summaries, which would feed A1's measurements, the dials and bars.
- **Advanced binds and Check-up:** A1 treats every B2 chain as unresolved, so a loadout with any advanced bind gets no suggestions. B2's machine can simulate owned chains exactly; A1 could treat B2-owned chains as resolved. That is an A1/B2 decision, not changed here.
- **Not verified:** a VoiceOver walk-through and keyboard-only operation in a live window; the harness checks labels and captures but not spoken output.

## Composition

Compose `defeb5a..` in order: `9fe61dc` (view stores), `bcb7c66` (Binds screens), `6b960f6` (Check-up screen), `7e64e4d` (tabs, Export, Guided tips and Expert badges), `71f0712` (harness), `d09b539` (plain names in Guided tips), `18360b9` (conflict copy), `71e8f06` (scrolling editor, plain B2 refusals), `11bc5bf` (Reduce Transparency check), then this report. All carry DCO sign-off and the requested author email.

The build lists (`scripts/build-launcher.sh`, the `tests/launcher/run.sh` typecheck) include both new views. `tests/launcher/config_studio.sh` compiles B1, B2, A1, I1 and the two view stores.

No engine source, push, tag, remote or merge. No `.app` bundle was built or registered with LaunchServices (the app build is a bare executable plus resources). Harness, app and test binaries, temporary homes and scratch folders were removed afterwards, leaving only logs and review renders under `WS114.noindex`. Nothing was written to `~/Library/Application Support/CoD2 Silicon/`.
