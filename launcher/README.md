# Native launcher

Build: `scripts/build-launcher.sh output/ws28/launcher` (Xcode Swift 6, no
packages). CMake's `cod2_launcher` and `cod2_macos_app` targets and both release
scripts use the same builder. Deployment target: arm64 macOS 26, Swift language
mode 6, complete strict concurrency, warnings as errors.

Source layout:

- `Core.swift`, `Presentation.swift`: validated settings, links, key storage and the
  Foundation-only formatting rules (maps, server badges, cm/360, release notes,
  scrims). Unit-tested.
- `Network.swift`, `MapArtwork.swift`: discovery, and the player's own loading screens
  decoded into a private cache.
- `Model.swift`: the one `LauncherModel`. It keeps the stored state (in a region per
  owner), start-up, navigation, the two windows, sheets and confirmations, the menu
  commands and the game's lifecycle. Each area's methods live in an extension file:
  `ModelSetup.swift`, `ModelServers.swift`, `ModelLibrary.swift`, `ModelSettings.swift`
  and `ConfigLauncherModel.swift`, each with its review-harness states.
- `Theme.swift`: colour tokens for dark and light, the bundled typefaces, and the
  glass surfaces and buttons with their accessibility material fallback.
- `Components.swift`: the shared building blocks from the design's Foundations board:
  page header and panels, form section, group and row, sheet scaffold and confirmation
  alert, empty state, banner, toast, status label, badge, chip, key caps, ping bars,
  progress bar, toolbar pill, scope switch, search pill, symbol button, pop-up button,
  text fields, checkbox, switch and the map-art frame every loading screen goes through.
- `Artwork.swift`: original fallback art drawn from the design's vector data, icon G
  at sidebar scale and the dog tags.
- `RootView.swift` and the page files: Home, Servers, Library and Configs in the main
  window, with Setup in its place until setup is done. Config Studio's document,
  profile, history, import, rules, binding, directory and match-summary models supply
  Configs through the shared session and store.
- `SettingsWindow.swift` and `SettingsPanes.swift`: the Settings window, a second
  window (scene `settings`) with General, Display, Game Data, Privacy, Updates,
  Advanced and About panes. `model.openSettings(_:)` opens it at a pane; it reopens on
  the pane it was left on, ⌃Tab and ⌃⇧Tab step through the panes, and the window takes
  each pane's height. `SettingsAcknowledgements.swift` holds every shipped component's
  licence in full, checked by hand against the build scripts (its header says how).
- `AppSheets.swift` and `ServerSheets.swift`, `SettingsSheets.swift`, `ShellSheets.swift`:
  the app's sheets, one enum presented over whichever window is in front.

First-run setup walks through game data, a CD key, shaders and a completion summary.
Game folders can be dropped onto the data well or chosen in a window-attached sheet;
validation stays beside the input. A verified stored key can be kept or replaced.
Continue saves the folder/key choice. Repeating setup offers Cancel (Escape), which
restores an unconfirmed folder choice and returns to the originating page; first-run
setup offers Quit. Shader preparation may finish in the background after leaving.
The summary states whether original or approximate shaders are available and offers
repairs. Open CoD2 Silicon opens Home, Find a Server opens Servers, and a pending
server link or `--play` gets an explicitly labelled Join Server or Open Game Menu action.

The native menus own navigation (⌘1–4), Launch (⌘Return), Open Game Menu
(⇧⌘Return), Settings (⌘,), config import (⌘O / ⇧⌘V), and Servers refresh (⌘R).
Commands follow setup, game and update availability. Page commands and the Dock reopen
the launcher window after it closes. Settings, About (the app menu's About opens the
About pane) and Check for Updates open the Settings window. Help links to the project
manual, game data instructions and releases, and opens the Keyboard Shortcuts and
Report a Problem sheets. Status messages reserve space below the page,
wrap without truncation, and are announced to assistive technology. A Quit request
during play brings back the launcher with an explanation and Return to Game.

Launcher checks: `sh tests/launcher/run.sh`. This runs core, network, artwork,
Config Studio, directory and match-summary tests, the font checksum check, a
strict typecheck of the material branch under the current SDK, and real-window
captures, accessibility checks and actions. Every page is captured in both
appearances and at the minimum window size; Config Studio also covers accessibility
display variants, inactive windows and sidebar resizing. The
material-branch typecheck is compile evidence, not runtime testing on older macOS.
Public wire fixtures were captured from both masters on 2026-10-04;
`tests/launcher/fixtures/capture.json` records their provenance.
`fixtures/releases.json` is a synthetic GitHub Releases response.

Review images come from a test-only harness that is never shipped. Its core is
`tests/launcher/Snapshots.swift`; each area lists its screens, expected VoiceOver labels and
window-driving suites in its own `Snapshots*.swift` file, and `SnapshotsFoundations.swift`
draws the building-block gallery. A screen named `settings…` or `about…` renders the Settings
window; `--list` prints every screen with its window, `--list-suites` the suites, and
`--suite NAME` runs one.

```sh
scripts/build-launcher.sh output/ws28/harness --snapshots
output/ws28/harness/LauncherSnapshots output/ws28/no-data
output/ws28/harness/LauncherSnapshots output/ws28/fallback --fallback
output/ws28/harness/LauncherSnapshots output/ws28/with-data --data ~/Games/CoD2
```

It hosts the real views in a window configured like the app's, and captures it
with `CGWindowListCreateImage`, so Liquid Glass and toolbars render for real.
`with-data` images contain the player's licensed loading screens: keep them
local. Snapshot mode uses authored synthetic servers, players, keys and directory
signatures. Config Studio screens create synthetic loadouts in private temporary
homes and inspect real transactions there. No production directory is fetched,
and the harness is excluded from release builds.

Typefaces: Big Shoulders Stencil Display and Courier Prime (SIL OFL 1.1) live in
`Resources/Fonts` with their licences. `scripts/fetch-launcher-fonts.sh --check`
verifies the pinned SHA-256 of every file. Bundles register them through
`ATSApplicationFontsPath`, and development builds load them from beside the
executable. The app icon is `tools/cod2x/icon-source/CoD2 Silicon.icon`.

The outer app registers `cod2x`. A nested `CoD2 Game.app` owns the engine and its
fullscreen/Game Mode eligibility. The launcher becomes an accessory app during
play, then restores its Dock/window after the tracked child exits. If the launcher is force-quit,
reopening it reconnects to the exact running helper (bundle ID and executable path) rather
than spawning another engine. Normal Quit waits until the game closes. A game crash
leaves the launcher running and exposes any native crash report in the app home.
There is one launcher window and one Settings window; Launch is guarded against duplicate children.

The game app carries two engines built from the same sources: `cod2_macos` with CoD2x and
`cod2_macos_stock`, the original 1.3 (CoD2x is compiled in, so it can't be switched at run
time). Settings › General › Game version chooses which one starts; CoD2x is the default and the choice
is saved in `launcher-settings.json`. Config Studio's dialect and settings, Check-up's rules and
the server ready check follow it. Both engines carry the app layer (link handler, bundle
arguments, freeze watcher, crash and freeze reports in the app home), so links, passworded ones
included, reach either engine the same way. Launching on a server that runs the other version
asks (the Switching game version sheet) and then starts that version for the match only
(`matchEngine`, which `chosenEngine` follows until the game ends); “Don’t ask again” skips the
question. The original 1.3 can't join a CoD2x server, so a server it hasn't seen yet is asked
for its version first. A bundle without `cod2_macos_stock` (the CMake `cod2_macos_app` target)
disables the choice, and CoD2x then joins both kinds.
Both engines read and write each loadout's `config_mp.cfg`, and an engine that can't hold a
saved value keeps its own default and writes that back: CoD2x's `cl_maxpackets` above 100,
`snaps` above 30, `r_lodScale` (protected in the original) and a window position past ±4096
(`vid_xpos`, `vid_ypos`), and the original's `com_hunkMegs` (CoD2x pins 512). The list comes
from the catalogue's two modes. Before each game the launcher puts back what the other engine
replaced (one History entry per loadout) and remembers what this engine can't hold in
`main/players/.studio-engine-handoff.json`.

CoD2x's own menus and icons live in `iw_CoD2x_01.iwd`, which CoD2x carries inside its Windows
`mss32.dll`. CoD2x has no licence, so neither ships with the app. Settings › Game Data › CoD2x menus and
icons gets the archive only when the player asks: Download fetches the pinned 1.4.6.8 release from
GitHub (refused unless its size and SHA-256 match `CoD2xFiles` in `Core.swift`), and Copy From
Folder reads, without changing it, the `mss32.dll` of a Windows game folder with CoD2x. The archive is
found by the engine's rule (`Cod2x_IwdFromDll` in `cod2x_features.c`, reimplemented in Swift) and
only it is kept, in `<app home>/cod2x`. Before a CoD2x game the launcher puts it in
`<app home>/main` (the engines' `fs_homepath`, searched after `fs_basepath`), and before an
Original 1.3 game it removes that copy when it is identical to the kept one. A game data folder
that already has `main/iw_CoD2x_01.iwd` keeps its own, the game data folder is never written, and
missing files never stop a game.

Fast path:

```sh
"CoD2 Silicon.app/Contents/MacOS/CoD2Launcher" --play
"CoD2 Silicon.app/Contents/MacOS/CoD2Launcher" --play --exit-after-game -- +set r_fullscreen 0
open -a "CoD2 Silicon.app" 'cod2x://connect/127.0.0.1:28960'
```

Arguments following `--` are forwarded verbatim to the engine's command parser
for trusted CLI/developer use. GUI settings and links always go through bounded
validators. Cold links wait for the helper to finish launching, then use the same validated
Apple-event path as live links. Passwords are held in memory and never put in
process arguments or cached. CD keys are validated
by WS21's native CRC routine, written using mode-600 temporary files and rename,
and removed from the child environment. The raw key is never printed.

App settings and library state live in the app home. Settings save automatically
after a short debounce and flush before quitting; invalid advanced text stays in
the editor while the last valid options remain in force. Library refreshes on
appearance, app activation and game exit, and orders files by modification date,
newest first. Config Studio edits the
selected loadout's `main/players/<profile>/config_mp.cfg` through the shared
profile store, with history and disk-conflict checks. The selected loadout and
the one used for the next game are separate. After migration succeeds,
`main/launcher.cfg` and startup arguments retain machine settings; personal
values come from the active profile. A failed migration keeps the previous
working launch behavior. Advanced chains use the validated launcher-owned
companion file after profile initialization. Startup dvars apply before renderer
initialization, with migrated launch settings excluding loadout-owned values.
Advanced values accept `dvar value` or `dvar=value`, optionally after `+set`, `+seta`, `set`
or `seta` as copied from a shortcut or the console, are kept as typed, reject command separators,
and the complete startup is limited to the engine's 31 commands/3900 bytes;
startup paths/credentials and upcoming renderer options are reserved. Native
Spaces sets `SDL_VIDEO_MAC_FULLSCREEN_SPACES=1`. Anisotropic filtering maps to
`r_anisotropy`. The current release uses Classic OpenGL. Renderer choices, render
scale, MetalFX and HDR are not shown until they are available.

Server discovery sends both protocols to both masters, collects multiple UDP
packets, deduplicates addresses and limits discovery to 2048 endpoints. Queries
use Network.framework with a maximum of six concurrent endpoints, 40 ms between
starts, status/info timeouts, cancellation, bounded parsers and a persistent
last-successful cache. Refresh is explicit; opening the app doesn't scan the
internet. Checking for updates is an HTTPS request to GitHub Releases, on Check Now or, with
Settings › Updates › Check for updates automatically on (the default), at most once a day while
the launcher is open; an automatic check that fails says nothing, and the review harness and the
lifecycle tests (`COD2_SETUP_NONINTERACTIVE=1`) never make one. It refreshes Home's Dispatches
and What's New, which lists the newest release's Added, Changed and Fixed notes. When a newer release carries
`CoD2-Silicon-<version>-macos-arm64.zip` and `SHA256SUMS`, Install Update (only on the player's
click) downloads both into a folder on the app's volume, refuses any size or SHA-256 mismatch,
unpacks with `ditto` and checks the app inside (bundle identifier, offered version,
`codesign --verify --deep --strict`, and the running app's Team ID when it has one), swaps it into
the app's place, moves the old app to the Trash and restarts (`AppUpdate` in `Core.swift`). It
refuses while a game runs, and where the app can't replace itself (a read-only or translocated
location, or a folder it can't write) it opens the Releases page as before. Cancel stops the
download or check until the new app is about to take the old one's place.

Report a Problem opens GitHub's bug form in the browser, filled in by field id with the lines
Copy Version Info copies (app version, game versions, macOS, Mac model, shaders, display; never the
CD key or player name). With its switch on, the last game log (the console log, or the newest crash
or freeze report, shortened like a crash report) goes to the clipboard for the player to paste; the
app itself sends nothing. Reset Settings returns `launcher-settings.json` to its defaults and leaves
loadouts, demos, screenshots and the CD key alone. Delete Summaries removes
`main/stats/matches.jsonl`.

Crash reports leave the Mac only with Settings › Privacy switches the player turned on. A fault in
CoD2x's own code leaves `<log>.pending.json` beside CoD2x's log; with "Send CoD2x crash reports to
the CoD2x project" on, the launcher sets `COD2_SILICON_COD2X_REPORTS=1` for a CoD2x game and the
engine sends it to CoD2x at its next start. Every other report goes, with "Send crash reports to
CoD2 Silicon" on and a build-time receiver (`make_macos_app.py --sentry-dsn`, or `COD2_SENTRY_DSN`
for the release scripts), to the project's Sentry as one envelope: home folder names shortened to
`~`, command-line lines dropped, refused if it holds anything that passes the CD key checksum, and
recorded in `crash-reports-sent.json` so it is never sent twice (`CrashReporting` in `Core.swift`).

WS21's NSAlert setup remains available in engine-only development/test bundles
(`make_macos_app.py --engine-only`). Production bundles set automatic setup off
in the helper because SwiftUI owns setup. The existing native engine URL handler
and server-facing CoD2x 1.4.6.8 identity are untouched.

Lifecycle checks: `python3 tests/launcher/lifecycle.py` then
`python3 tests/launcher/cold_url.py` (build the launcher into
`output/ws25/launcher` first). These use a synthetic helper, public wire parser
and fake game archives/key in scratch homes, exercise real activation policies
and Apple events, and unregister both test bundles. Media playback lists only
the engine-supported `.dm_1` format.
