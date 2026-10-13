# Config Studio: design spec

- **Status:** implementation authorized on 2026-10-07. Research facts are folded in from `research/` (see the Appendix).
- **Date:** 2026-10-06.
- **Owner:** the user (product decisions). Engineering goes to Codex `gpt-6.1-sol` at xhigh; design to Astra at high effort, as requested on 2026-10-07.
- **Visual source of truth:** the CoD2 Silicon Launcher design canvas, https://claude.ai/artifact/XZmUZKarMJWQ2Qa8NThzdh, specifically its Design Rules board and the Config Studio boards.

## 1. Purpose

Config Studio lets every CoD2 Silicon player understand, tune and share their game configuration from the launcher, without knowing console syntax. It serves newcomers and competitive players equally:
- **Guided:** plain language, safe choices.
- **Expert:** full control, the same polish.

It covers:
1. a reference explaining every setting;
2. editing settings and key binds;
3. an optimizer that explains its advice;
4. a curated directory of configs from well-known players, plus import from anywhere.

### Success criteria

- **Plain language.** A newcomer can make the game feel better without seeing a dvar name. Dvar names appear only in Expert and in "Edit as text".
- **Round-trips exactly.** An expert can do anything a hand-edited `config_mp.cfg` can, including multi-command binds, toggles and per-press cycles. The launcher never corrupts or silently drops lines.
- **Complete coverage.** Every setting and command the game registers is covered by the catalog. A build check enforces this.
- **Safe imports.** Nothing imported applies without review unless the user chooses "Import everything".
- **Apple-grade UI.** Apple HIG and Liquid Glass are mandatory, the UI is accessible as drawn, and it matches the canvas.

### Non-goals

- Binding mouse buttons 6 and up. The game stays faithful: Mouse 1–5 plus the wheel. Users are told how to map extra buttons in their mouse's software.
- Editing a running game's config live.
- Accounts, cloud sync, or uploading configs from the launcher.
- Any change to gameplay. The only engine addition, match summaries, records data and changes nothing.

## 2. Decisions (with who made them)

| Decision | By |
|---|---|
| Audience: newcomers and competitive players equally, via a Guided and an Expert view | user |
| Edit the real active-profile `config_mp.cfg` while the game isn't running, with snapshots and undo | user |
| Named loadouts built on CoD2 player profiles | user |
| Directory: a curated public GitHub repo, `STiXzoOR/cod2-configs`, credited and permissioned, that Windows players can also download from; plus import from files, links, GitHub, localhost and paste | user |
| Imports: Review by default (safe), with Import everything (with a warning) available; the user picks per import | user |
| Optimizer scope: frame rate and physics, network, aim and input, visibility and HUD | user |
| Server-aware ready check with suggestions; no auto-switching | user |
| Architecture: native Swift, with a catalog generated from research plus engine source and a coverage build check | user |
| Configs page: three columns (sidebar · loadouts and discover · detail) | user |
| Settings tab: a Guided/Expert hybrid with shared purpose groups; Expert is polished, typed controls plus power tools (ruleset, compare, locks and notes, edit as text) | user |
| Binds: its own List \| Visual switch; Visual draws the detected keyboard and pointing devices | user |
| Extra mouse buttons: stay faithful (no MOUSE6+) | user |
| Directory redesign: about players and feel, not variables | user |
| Plain language everywhere by default | user |
| A local match summary written by the game, for personal advice | user |
| Personal settings move from Settings into loadouts; the launcher stops forcing them at launch | user (approved section 1) |

## 3. Information architecture

**Sidebar:** Deploy · Servers · Library · **Configs** · Settings · About.

**Configs page:**
- **Middle column:**
  - MY LOADOUTS: a list, the active one marked, plus "New loadout";
  - DISCOVER: Players and Saved.
- **A loadout** has tabs: **Settings** (Guided | Expert) · **Binds** (List | Visual) · **Check-up** · **History**.
- **Discover › Players:** the directory home, player pages, "＋ Add a config" (import) and the import review sheet.

**Settings page (this Mac and the app only):**
- game data location;
- display: resolution and fullscreen mode;
- renderer;
- updates;
- privacy: match summaries on or off;
- advanced launch options.

**Servers page:** a ready check above Join for the selected server.

## 4. Loadouts and config ownership

**A loadout is a CoD2 player profile,** at `main/players/<profile>/config_mp.cfg`.
- The launcher selects the active profile before launch. `players/active.txt` is the current mechanism and must be verified against the engine.
- The in-game profile menu lists the same loadouts.

**Launcher metadata** lives beside each profile in `players/<profile>/studio.json`, which the game never reads. It holds:
- locks and notes;
- origin (created, imported from or copied from), with credit and source;
- the per-setting values from the last import, for "Compare".

**Read-only while the game runs.**
- The game owns the profile config: it rewrites the whole file on the frame after any archived-setting or bind change, and never merges with what's on disk. It does not wait until exit (Appendix A).
- So while `cod2_macos` is running, Config Studio is read-only and shows a banner, and the launcher watches the file to show the game's changes live.
- When the game exits, the launcher records a "Game session" snapshot if anything changed.

**Snapshots (History):**
- Every apply writes a snapshot under `players/<profile>/history/`, capped at 50 per loadout.
- A snapshot records the source (You, Import, Check-up, Game session), the time and a plain-language summary.
- Any snapshot can be restored. Restoring creates a new snapshot.

**What the launcher applies at launch changes:**
- Today `launcher.cfg` and `+set` force `com_maxfps`, `sensitivity`, `in_rawmouse`, `m_filter`, `cl_mouseAccel`, `logfile`, `developer` and `com_introPlayed`, in `Core.swift` `LauncherSettings.dvars()`.
- From now on the launcher applies only:
  - machine settings: `r_mode`, `r_fullscreen`, `r_borderless`, `r_swapInterval`, renderer options;
  - launcher housekeeping: `com_introPlayed`;
  - temporary per-server overrides ("Fix for this server only").
- Loadout settings live only in the profile config.

**Migration.** On first run of this version, the user's current launcher values (frame cap, sensitivity, DPI, raw mouse, volume) are written into a **Main** loadout, created from the current `config_mp.cfg`, so the game feels exactly the same. The original file is snapshotted first.

## 5. Config grammar and the config engine

**Facts come from the grammar research** (Appendix A), not from Quake 3 assumptions. CoD2x replaces `Cmd_TokenizeStringInternal`, so stock 1.3 and CoD2x rules are both documented, and the engine follows the CoD2x rules for the CoD2x build.

**Parser and serializer (Swift):**
- **Line model:** commands separated as the engine separates them (newlines and `;` outside quotes).
- **Command kinds:** `seta`, `set` (and the variants listed in the Appendix), `bind`, `unbind`, `unbindall`, `exec`, `vstr`, comments, and unknown commands. Unknown commands are kept verbatim.
- **Round-trip guarantee:** unedited bytes stay byte-identical. Edits touch only the affected lines, and new lines go where the game would put them (the order from `Com_WriteConfig`).
- **Limits** (line, token, bind and buffer lengths) are enforced with plain errors before writing.

**Advanced binds as first-class models:**
- **Multi-command:** a bind running several commands in one press, for example `bind J "name A; name B"`. Both run; the last wins.
- **Toggle:** two states that alternate on each press, built from `set` and `vstr`.
- **Cycle on each press:** an ordered list of steps (`set step1 "…; set next vstr step2"` … `bind J "vstr next"`). The editor shows "Each press: A → B → C".
- **`+`/`-` binds with extra commands:** key-down and key-up behaviour exactly as the engine does it (Appendix A).
- **Quote workarounds** (variables holding quoted text) are preserved.

**Where chains live: option A approved by the user on 2026-10-07.** The game keeps exactly two kinds of state in `config_mp.cfg`: binds and archived dvars (`seta`). It rewrites the whole file on the frame after any change, never merges, and drops comments, `set`, `exec` and `vstr` lines. The considered options:
- **(A) Recommended: a launcher-owned chain file.** It is `main/cs_<profile>.cfg`, loaded with `+exec` at launch.
  - It uses `set`, so helper variables are not archived and pointer changes do not cause a config rewrite per keypress; escaped quotes are allowed subject to the engine's splitter rules. Helper variables still occupy runtime dvar slots.
  - The triggering `bind` stays in `config_mp.cfg`.
  - Without the launcher, `bind F12 "exec cs_<profile>"` reloads it on demand.
  - An **"Export for Windows"** action converts the chains to the portable `seta` form.
- **(B) Players' convention: quote-free `seta` chains** inside `config_mp.cfg`, as in the user's Windows profiles (`seta cmdcur "vstr tgl1"`).
  - Portable as is, and the cycle position is remembered across sessions.
  - But it costs one dvar slot per step, rewrites the config on every press, allows no quotes, and gets re-sorted alphabetically.

**Either way,** imported `seta` chains are preserved exactly as they are; Config Studio never silently converts them.

**Rewrite model.** A file the game wrote itself is in canonical form:
- the header;
- two `unbindall` lines;
- binds in key-number order;
- `seta` lines sorted case-insensitively.

For such files, the editor inserts new binds and `seta` lines at the positions the game would use, and removes whole lines. Any other file is a script file: only the spans the user touches are edited, and nothing is reordered. In the corpus, 61 of the 71 files are canonical; the rest were hand-edited.

**Evidence from real configs.** The corpus is the user's Windows CoD2 profiles: nine profiles plus backups, kept machine-local at `cod2-native-wt/_agents/research/corpus/windows-players/` with secrets redacted, and never committed. Across about 17,700 `seta` and 1,690 `bind` lines, it shows:
- `toggle <dvar> v1 v2 v3` cycling through a value list on each press (276 uses), for example `toggle com_maxfps 125 250 333`;
- `vstr` chains (334);
- `wait N` inside binds (102), for example `bind MOUSE5 "weapnext; wait 2; weapprev"`;
- multi-command binds, for example `bind MOUSE2 "toggleads; +holdbreath"`;
- `say`/`name` scripts, `exec` (47), and a doubled `unbindall` in files the game wrote itself;
- rcon admin scripts stored in variables.

The editor models all of these. **"Cycle on each press" uses `toggle` with a value list when it cycles one setting, and a `vstr` chain when it cycles commands.** Every file in the corpus must round-trip byte for byte.

**Platform translation on import.** Windows-only settings, resolutions and Mac-only keys (Command) are flagged and mapped in review. The files in `cod2-configs` stay exactly as the players wrote them.

## 6. Catalog (the knowledge base)

**`ConfigCatalog.json`** is bundled in the launcher and generated by `tools/catalog/` from:
- `research/dvars` (engine registrations: name, type, default, range, flags, module; menu exposure and labels);
- `research/web` (explanations, recommendations, sources);
- hand-written plain-language copy.

**Each entry has:**
- `name`;
- `title` and `summary`, in plain language for newcomers;
- `details`, for Expert;
- `group` (one of the eight purpose groups);
- `control` (segmented, slider, switch, menu, colour or text) with its range and step;
- `default`;
- `flags` (saved, cheat-protected, userinfo, latched, read-only);
- `serverControlled`;
- `guided` (shown in Guided?);
- `cod2x` (added, changed or unchanged);
- `sources`.

Commands get the same treatment, with `bindable` and their menu labels.

**Coverage check (CI).** A test compiles the engine's registration list from `src/` (stock and CoD2x builds) and fails on:
- any dvar or command missing from the catalog;
- any catalog entry the engine doesn't register;
- any type or default mismatch.

**Writing rules** for the catalog copy come from the Design Rules board: sentence case, second person, units on numbers, and effects explained as what the player will notice.

## 7. Rule sets

Rule sets are data files (`rulesets/*.json`): Stock 1.3, Public CoD2x, zPAM 4, PAM, and league rule sets (Appendix B). Each holds:
- **per-setting constraints:** allowed range, forbidden values, forced values;
- **ruleset-level facts:** the server frame rate, CoD2x requirements;
- **sources.**

**Evaluation** returns, per setting: ok, warning (legal but unusual) or violation, each with a plain reason. Rule sets drive:
- the Expert ruleset picker;
- the import review flags;
- the check-up;
- the server ready check.

## 8. Settings UI

As drawn on the canvas (ConfigsGuided, ConfigsExpert).

- **Guided:** friendly controls per purpose group, no dvar names, with inline optimizer tips. "All N settings ›" opens Expert at that group.
- **Expert:** every catalog entry as a typed control, with:
  - the plain name over the monospace dvar name;
  - reset, lock and note on each row;
  - rule badges;
  - Changed and Locked filters.

  **Power tools:**
  - the ruleset picker;
  - Compare with a loadout or player, which shows ghost markers and "Take theirs";
  - Command-K "Jump to any setting";
  - Edit as text, with syntax colouring and validation, kept in sync both ways with the controls.

**Locked settings** are never changed by the optimizer, imports or Take theirs unless the user unlocks them.

## 9. Binds UI

As drawn (BindsVisual, BindsList).

**List | Visual switch,** separate from Guided/Expert.
- **List:** grouped actions (Movement, Combat, Communication, Game & UI) using the game's own action names from the Controls menus, with primary and secondary keys and inline conflict notes.
- **Visual:** the detected keyboard, with keys coloured by role, a legend, and hatching on keys the game can't bind; plus pointing devices. Clicking a key opens an inspector.

**Capture.** "Press a key or click" records the physical input inside the launcher, using a local event monitor (no permissions).

**Device detection (no permission prompts):**
- **Keyboard shape (ANSI / ISO / JIS):** from macOS's keyboard type for the device. Third-party keyboards use their Keyboard Setup Assistant identification. It can be overridden.
- **Key labels:**
  - from the current input source via `UCKeyTranslate`, following live layout changes;
  - mirroring the game's SDL3 key mapping, including its Latin-letter fallback for non-Latin layouts such as Greek;
  - a test compares the launcher's mapping with the game's SDL mapping.
- **Size** (Full / TKL / 75% / 65% / 60%) and **modifier style** (⌃⌥⌘ or Ctrl/Win/Alt): guessed from the device name and vendor, overridable, and confirmable with "Press keys to verify…".
- **Pointing devices:**
  - mice and trackpads are enumerated from the I/O registry;
  - button counts are read from each device's HID description;
  - trackpads get the trackpad drawing (click, two-finger click, scroll);
  - mice show Mouse 1–5 plus the wheel, with extra buttons explained and placed using "Learn button positions…";
  - the polling rate is shown from the engine's measurement when available;
  - DPI is entered by the user.

**Advanced binds editor** (multi-command, toggle, cycle) builds the models in §5. Commands that would be risky in an import are labelled the same way here.

**Known game issue to fix in the engine (tracked separately).** On ISO Mac keyboards the `§` key can't be bound, and the console key is the one beside left Shift. Check this against Mac 1.3's behaviour; it's included in the faithful-UI follow-up.

## 10. Directory

**Repository `STiXzoOR/cod2-configs`,** public:
- `entries/<slug>/config.cfg`: exactly as published by the player.
- `entries/<slug>/entry.json`:
  - player, team, era, role;
  - `feel` (one line) and `story`;
  - `styleTags` (plain-language chips);
  - `favoriteMap` (for card art from the user's own game data);
  - `credit`, `sourceUrl`;
  - `permission`: who granted it, when, and a link or evidence; the scope must cover public redistribution to anyone.
- **CI:** parses each config with the same grammar, runs the safety classifier, checks that `permission` is present, and generates a README index (player, era, style, download link) for Windows and Linux players.
- **`index.json`** is signed with an Ed25519 key. The launcher embeds the public key, verifies the signature, caches the index for offline use, and refuses tampered data.

**UI** (as drawn: Directory, PlayerDetail):
- a featured hero;
- shelves, for example eras and "Built for the 333 jump";
- cards with feel, style chips and "Close to your setup";
- player pages with "What changes for you", credit and source;
- "Try this setup", which creates a new loadout and never touches Main;
- Compare and Import;
- Saved.

## 11. Import

**Sources:**
- **"Bring your Windows profiles"** (approved by the user on 2026-10-07). Detect a Windows CoD2 install on any mounted volume (Boot Camp, an external Windows drive or a network share) by looking for `…/Activision/Call of Duty 2/main/players/`. Offer each profile there, active one first, as a new loadout through the same review sheet. Backups such as `config_mp_2.cfg` and Raptr copies are offered as optional extra loadouts.
- a file or folder;
- https URLs (http only for `localhost` and `127.0.0.1`);
- GitHub repo files, raw links and gists;
- this Mac's server over http://localhost;
- pasted text;
- drag and drop.

Imports are text only, with a size limit (Appendix A). An `exec` of another file is resolved only within the same folder or repository, and is then reviewed too.

**Safety classifier (Appendix A, `safety.md`):** each line is one of:
- a safe setting;
- a safe bind (a known action);
- **needs OK**, for example:
  - `connect`;
  - `rcon` or `rconpassword`;
  - `name`;
  - `exec` of an unknown file;
  - `quit`;
  - `writeconfig`;
  - `unbindall`;
  - binds that run any of these;
  - `say` spam;
  - cheat-protected values;
  - download or redirect settings;
- **blocked**, never applied even with "Import everything": CD key, passwords and filesystem paths.

**Review sheet** (as drawn: ImportReview):
- **Review** (the default) or **Import everything**.
- Changes are grouped and in plain language, with "Show details" for exact lines.
- "Needs your OK" items are unticked in Review mode.
- Locked settings are protected.
- The target is a new loadout by default.
- Nothing applies until the user confirms, and every apply is a History snapshot.

## 12. Check-up (optimizer)

**Inputs:**
- the loadout;
- the selected ruleset;
- Mac hardware (model, display refresh rates, GPU class);
- the engine-measured mouse polling rate;
- match summaries (§13).

**Rules** are data (`optimizer/*.json`) plus Swift predicates. Each rule has:
- `when`: a condition;
- `suggest`: setting changes;
- `title` (an outcome);
- `why` (plain);
- `notice` ("You'll notice…");
- `evidence`: which data it used;
- `sources`;
- `tuneFor`: competitive, balanced or best looking.

Rules never touch locked settings or violate the selected ruleset. Every rule is unit-tested.

**UI** (as drawn: CheckUp):
- four dials: Movement, Aim, Connection, Visibility;
- suggestion cards with Apply / Show me / Not now;
- without match summaries: general advice, plus an invitation to turn them on.

## 13. Match summaries (engine feature, local only)

**When:** at the end of each match, or on disconnect.

**What:** the game appends one JSON line to `main/stats/matches.jsonl`:
- map, mode, duration;
- server address and rule hints;
- frame time p50, p99 and max;
- frames under the cap;
- ping average and maximum;
- packet loss;
- snapshots received;
- the cap in use.

**Constraints:**
- On by default; toggled in Settings › Privacy, which the launcher passes as a launch setting.
- Never transmitted.
- Pruned to the last 200 matches.
- Implemented under the faithful-wave rules: no gameplay effect, no per-frame allocation, and the i386 view unchanged.

## 14. Server ready check

- **Source:** the launcher's existing server status query. It reads the mod (`fs_game`, zPAM keys), CoD2x presence, `sv_fps`, the rate limits and any advertised rule variables (Appendix B), then maps them to a ruleset or to "unknown".
- **Evaluation:** the active loadout is checked against that ruleset, and the result is shown in plain words on the Servers page.
- **Actions:**
  - Use another loadout;
  - **Fix for this server only:** temporary `+set` overrides at launch for that connection; the loadout is unchanged;
  - Details.
- **Unknown rules** get general advice, never a false all-clear.

## 15. Design and HIG requirements

- **The canvas's Design Rules board is binding:**
  - tokens (dark and light), type, Liquid Glass materials with a fallback for macOS 13–25, and Reduce Transparency and Increase Contrast behaviour;
  - components and their states, spacing and radii;
  - motion (respecting Reduce Motion);
  - keyboard navigation and focus;
  - VoiceOver patterns;
  - writing rules.
- **Native controls first** (SwiftUI and AppKit): SF Symbols for icons, standard shortcuts, Command-K, and Command-Z (undo) in editors.
- **Every screen ships with:**
  - light and dark appearance;
  - full keyboard navigation;
  - VoiceOver labels;
  - 4.5:1 contrast;
  - a check against its canvas board, with screenshot evidence in the PR.

## 16. Testing

- **Grammar:** fixtures from `research/grammar/fixtures`, plus the machine-local corpus of the user's Windows profiles (34 files). The corpus is used locally only and is never committed; CI runs the committed fixtures. Byte-exact round-trip, and the semantics of key-down and key-up for multi-command, toggle and cycle binds.
- **Catalog coverage** check (CI).
- **Safety classifier:** table tests, one per risky pattern.
- **Rulesets** and every **optimizer rule:** unit tests.
- **Device mapping:** launcher key labels against the game's SDL key mapping, across ABC, Greek, AZERTY, QWERTZ, Dvorak and JIS.
- **Migration:** a test reproduces today's settings into Main.
- **UI:** snapshot tests per screen (light and dark, Reduce Transparency) and an accessibility audit.

## 17. Delivery: sub-projects (each with its own plan)

1. **Foundation:** the catalog plus its coverage check, the grammar engine, loadouts on profiles, migration, Settings (Guided/Expert) and History. **Depends on:** Appendix A and the dvar research.
2. **Binds:** List | Visual, device detection, capture, and the advanced binds editor. **Depends on:** 1, and the ISO key fix in the engine.
3. **Directory and import:** the `cod2-configs` repo, CI and signing; the import sources, safety classifier and review sheet. **Depends on:** 1.
4. **Check-up, match summaries and ready check:** the optimizer rules, the engine match summaries, rulesets and the server ready check. **Depends on:** 1, and Appendix B.

## 18. Risks and open questions

- **Permissions** for player configs. The directory launches with only permissioned entries; it may start small.
- **Rule detection on servers** varies by mod version. Unknown rules must degrade to general advice.
- **Profile switching:** confirm how the engine selects the active profile at start, and that the in-game profile UI behaves correctly with many profiles.
- **Third-party keyboard size** can't be detected. User confirmation is required, and that's acceptable.

## Appendix A: grammar facts

**Source:** `cod2-native-wt/_agents/research/grammar/`: `grammar.md`, `binds.md`, `roundtrip.md`, `safety.md`, and `fixtures/` with JSON cases plus seven whole-file samples.
- The tokenizer model matched two independent implementations on 60,672 fuzz strings with zero mismatches.
- It reproduces the bind blocks of 63 of the 69 game-written corpus files byte for byte.

**Splitting commands**
- A command ends at LF, CR, or `;` when the quote count so far is even. Escaped quotes count.
- Comments don't exist at this stage, so `// x; quit` **runs `quit`**. The safety review must treat text after `;` in a "comment" as live.
- Lines are capped at 4095 bytes.

**Tokenizing**
- Bytes 0x80 and above outside quotes count as whitespace.
- `\"` is the only escape.

**Dispatch**
- Order: built-in commands, then dvars, then cgame commands.
- **Anything unrecognised is forwarded to the server when connected.** The safety review treats unknown commands as network-visible.

**Missing and present commands**
- There is no `alias`, no `echo` and no autoexec file.
- `wait` exists and counts `Cbuf_Execute` calls, two per frame. CoD2x restricts it under `g_competitive`.
- `exec` and `vstr` insert their text at the front of the buffer, with **no loop protection**. The editor must detect cycles that never yield, for example a `vstr` chain with no keypress between steps.
- `toggle <dvar> v1 v2 …` cycles through a list of values. Quake 3 lacks this.

**Binds**
- `bind` lowercases the key name but `unbind` does not, so `unbind A` does nothing. The editor always writes lowercase key names.
- Bind strings are truncated at 1023 bytes.
- `;` is named `SEMICOLON` and `"` is written as `0x22`.

**Key release**
- A bind that starts with `+` appends `<key> <time>` to its **last** command.
- Everything after the first `;` runs **on both press and release**.
- Only the first `+` gets its `-` on release.
- The Binds UI explains this when a bind mixes `+` actions with other commands. For example, `+attack; say hi` says "hi" twice.

**Writing the config**
- Layout: the header, **two** `unbindall` lines, binds by key number, then sorted `seta` lines with unescaped values.
- The game rewrites the file the frame after a change, **never on exit**.
- CoD2x adds `com_writeConfig 0`, which stops automatic writes, and forces the config path to `main/`.

**CoD2x differences**
- the `://` comment rule;
- the launch-line splitter: comma as a separator, and `cod2x://` handling;
- `wait` under `g_competitive`;
- the config path;
- `com_writeConfig`.

**Launch line.** `+exec` file names must be `[A-Za-z0-9_]` with no `+`, commas or spaces. Under CoD2x the line must start with `+`.

**Port deviations** (they go to the faithful-engine fix wave):
- our tokenizer drops `\"` where the original emits `"`;
- our `set` join is capped at 1023 bytes;
- in CoD2x mode, our build treats high-bit bytes as token characters;
- our build uses a bare `main/config_mp.cfg` path.

**Still open:**
- `vstr` on an enum dvar;
- quoted single high-byte key names;
- exactly which keys fire binds while a menu is open;
- the config path under `fs_game`.

## Appendix B: rulesets and competitive limits

**Source:** `cod2-native-wt/_agents/research/web/`.
- `topics/competitive-rules.md`, `cod2x.md`, `fps-physics.md`, `network.md`, `aim-input.md`, `visibility-hud.md`.
- 893 dvar notes in `dvar-notes.json`, and 633 rated sources.
- 27 documented contradictions in `summary.md`.

**The only code-enforced client rules today are CoD2x with zPAM 4.x.**
- When the server sets `g_competitive 1`, the CoD2x client clamps:

  | Setting | Clamp |
  |---|---|
  | `com_maxfps` | 125–250; anything else is reset to 250 |
  | `rate` | 25000 |
  | `snaps` | 40 |
  | `cl_maxpackets` | 125 |
  | `sc_enable` | 0 |
  | `fx_sort` | 1 |
  | `mss_q3fs` | 1 |

- It also **disables `wait`**.
- zPAM 4.x sets this in every mode, including `pub`.
- **Consequences:**
  - **333 fps isn't possible on competitive CoD2x servers.** The ready check says so plainly.
  - Binds that use `wait` silently break there. The Binds UI flags them, and the ready check warns about them.
- **zPAM 3.x/4.x also push about 40 cosmetic, HUD and input dvars** to every client: `r_drawSun 0`, `cg_viewsize 100`, `cl_pitchspeed 140`, `cl_yawspeed 140`, HUD and tracer settings, and others. On those servers, the optimizer treats them as server-owned and never suggests them.

**Everything else is paper rules.** League texts regulate binds and scripts, FPS, modified files and third-party tools, not graphics dvars.
- The ClanBase, ESL, CAL, TWL, CEVO and UKCOD2 rulebooks couldn't be retrieved. Rule sets for them ship only when a primary source is found.
- `r_picmip 3` is listed by zPAM's own authors as "abusable, unenforced". It's legal, but flagged as contested.

**Cheat-protected, so never a player setting:**
- `cg_fov`, `cg_fovScale` and `cg_fovMin` (stock default 80). FOV is **not** a loadout setting, and the canvas boards must not offer it.
- `r_lodBias` (the port currently lacks this protection; see Appendix C).
- CoD2x's `cg_thirdPersonMode`.

**Stock 1.3 versus CoD2x ranges:**

| Setting | Stock 1.3 | CoD2x |
|---|---|---|
| `snaps` | 1–30 | up to 40 |
| `cl_maxpackets` | 10–100 | up to 125 |
| `rate` | up to 25000 | up to 25000 |
| `com_maxfps` | 0–1000 | 0–1000 |

**Compatibility.** CoD2x clients can join stock servers, but not the other way round. CoD2x loads only `iw_*` and `localized_*` IWDs, plus those the server lists.

**Frame-rate physics.** There is no fixed physics step, so `com_maxfps` changes jump height and fall damage. The measured jump numbers come from a single undated study, so the check-up states them as "commonly measured", not as exact values.

**Inert names.** Popular configs contain dvars that don't exist in 1.3: `cl_timeNudge`, `r_vsync`, `cl_run`, `pmove_fixed`, `cvarlist`, and CoD4/CoD1 dvars (12 of the 71 collected configs are contaminated). The import review labels these "has no effect in CoD2" and leaves them off by default.

**Player configs.** None of the 71 collected configs has a licence or confirmed permission, and none states a DPI. The directory launches with permissioned entries only; until then it links to and describes configs, and never mirrors them.

## Appendix C: catalog statistics

**Source:** `cod2-native-wt/_agents/research/dvars/`, decoded statically from the Mac 1.3 binary and cross-checked against CoD2x's decompile and dvarlist. The extraction scripts are in `research/dvars/tools/`, the seed for `tools/catalog/`.

**Dvars: 961 names in total.**
- 668 are registered by the Mac 1.3 engine.
- 95 are added by CoD2x, and 28 stock dvars are changed by CoD2x.
- 20 are registered only by the port.
- 178 are created at runtime without an engine registration: 125 by multiplayer scripts, 49 by menus, 4 by the shipped config.
- Of the 763 registered dvars, only **56 are exposed by a menu**; **707 are console-only**.
  - The graphics menu edits `ui_r_*` proxies; 13 of these pairs are mapped back to the real dvars.
- **248** are cheat-protected and **13** are userinfo.

**Commands: 253.** 221 are registered by Mac code, and **43 are bindable** from the Controls menus.

**Flag bits** are documented with evidence in `research/dvars/flags.md`.
- `0x1000` means "registered by engine code"; `0x2000` means renderer-owned; `0x4000` means created from the console.
- Nothing reads `0x200`.

**Implications for the catalog:**
- Stock dvars have no prose descriptions in any source. The plain-language copy is written by us, from the web research plus engine behaviour. That is a sizeable writing task inside sub-project 1, prioritised by relevance: the 56 menu-exposed dvars and the competitive and network set first.
- Script-created dvars have no type or range. The catalog marks them as "set by the server or mod", read-only in Guided.
- Some values are built at runtime and can't be extracted statically (`fs_*path`, the enum lists for `r_mode` and `r_displayRefresh`). The coverage check treats them as "dynamic".

**Port differences found** (they go to the faithful-engine fix wave, not to Config Studio; full evidence in `research/dvars/port-diffs.md`):
- `r_lodBias` and the seven `r_lightTweak*` dvars lost cheat protection.
- `cl_maxpackets` min is 15 (the reference is 10), and `loc_language` max is 13 (the reference is 14).
- `dvar_bool`/`dvar_int`/`dvar_float` test the wrong flag bit.
- The cgame command `quickmessage` should be `mp_QuickMessage`, which is the command `default_mp.cfg` binds to `V`.
- `motd` and `cl_profileTextHeight` are not registered.
- `r_lodScale` in the stock build mixes CoD2x flags with stock ranges.
- The user requested source verification of `snaps 40` and `cl_maxpackets 125` on 2026-10-07. CoD2x `src/mss32/cgame.cpp:241–245` raises their maximums; `src/mss32/competitive.cpp:54–76` sets those values when `g_competitive` is enabled. These conditional assignments do not establish unconditional startup defaults. Keep the existing port defaults during this work and preserve user profiles; document the distinction.
