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

First-run setup has six steps (`SetupView.swift`, `ModelSetup.swift`): Welcome, Game data,
CD key, Shaders, Make it yours and Ready, with Back, the progress dots and the step's
actions in a glass bar. Game folders can be dropped onto the card or chosen in a
window-attached sheet; a wrong folder is explained beside it with the first missing
archive and how many others are missing. The CD key's checksum is checked as you type;
the key can be skipped, and a stored key is shown as stored so Back and Continue never
ask for it again (Replace… opens the Replace CD Key sheet). Shaders show the real count
(or an honest indeterminate bar until the first file appears), then ready, or failed with
Try Again, Choose Mac Copy… and continuing with approximate shaders. Make it yours sets
the in-game name and how the game fills the screen; nothing is saved until Continue.
Ready summarises the real values; Open CoD2 Silicon opens Home and Find a Server opens
Servers. Set Up Again (Settings › Game Data) shows Cancel at every step, and Escape
presses it: nothing is kept that wasn't confirmed. A notification says the shaders
are ready only when setup is in the background.

The native menus (File, Edit, View, Server, Loadout, Window, Help; `Main.swift`) own
navigation (⌘1–4), Launch (⌘Return: the Servers selection when Servers shows one,
else the last server), Open Game Menu (⇧⌘Return), Jump to… (⌘K), Settings (⌘,),
config import (⌘O / ⇧⌘V) and Servers refresh (⌘R). Items are dimmed, never hidden,
when they can't run. Page commands and the Dock reopen the launcher window after it
closes. Settings, About and Check for Updates open the Settings window. Help opens the
project manual, the Keyboard Shortcuts sheet, release notes, Report a Problem and the
logs folder. Jump to… (`JumpPalette.swift`) finds servers already in memory, loadouts,
every Settings pane and row by its visible name, and menu actions: ↑ ↓ move, Return
opens (a server result opens in Servers), and Escape closes. ⌘Return always runs File › Launch,
using the Servers selection when that page is showing one, otherwise the last server. The sidebar's player plate opens
the player card (`PlayerCard.swift`): the name, edited in place with its colours shown
and saved to the next game's loadout as you type, the CD key's state and that loadout.
The Dock menu lists recent servers, Launch on Last Server, Open Game Menu and Connect
to Address…. Opening a `.dm_1` plays it; opening or dropping a `.cfg` opens the import
review. Status messages reserve space below the page, wrap without truncation, and are
announced to assistive technology.

Launcher checks: `sh tests/launcher/run.sh`. This runs core, network, artwork,
Config Studio, directory and match-summary tests, the font checksum check, a
strict typecheck of the material branch under the current SDK, and real-window
captures, accessibility checks and actions. The shape audit checks attached app sheets separately against their own window pixels.
`capture-geometry.json` records the exact parent, child popover and captured union
frames. The image-size check requires the parent’s fixed size and checks composite
captures against that union; states without child windows keep their exact checks.
Shared desktop runs wait 60 seconds after a busy window lease, then watch for its
release so brief gaps between other agents’ runs cannot starve the review harness.
Every page is captured in both
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

The outer app registers `cod2x` and `cod2` links and the demo and config document types.
A nested `CoD2 Game.app` owns the engine and its fullscreen/Game Mode eligibility. The
launcher keeps its Dock tile (and Dock menu) during play. When Hide CoD2 Silicon while
the game is running is on, it hides its windows and brings them back when the tracked
child exits. If the launcher is force-quit,
reopening it reconnects to the exact running helper (bundle ID and executable path) rather
than spawning another engine. Quitting while the game runs asks first: Quit Launcher
Only, Quit Both or Cancel; a game that doesn't start gets a sheet that says why and
offers the fix. A game crash
leaves the launcher running and offers its game session log, falling back to a native crash report.
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
validators. A join link (`cod2x://host:port`, `cod2x://connect/host:port`, or `cod2://`
read the same way) opens Connect to Address with the address filled in and never starts
the game by itself; a link that arrives during setup waits until setup ends. Unattended
runs (`COD2_SETUP_NONINTERACTIVE=1`, the hand-off checks) and command-line link arguments
keep the direct join: cold links wait for the helper to finish launching, then use the same
validated Apple-event path as live links. Passwords are held in memory and never put in
process arguments or cached. CD keys are validated
by WS21's native CRC routine, written using mode-600 temporary files and rename,
and removed from the child environment. The raw key is never printed.

App settings and library state live in the app home. Settings save automatically
after a short debounce and flush before quitting; invalid advanced text stays in
the editor while the last valid options remain in force. Library (Matches, Demos,
Screenshots) refreshes on appearance, app activation, the window coming forward and game
exit; demos and screenshots are ordered by modification date and match summaries by when
they ended, newest first, and each tab keeps its scroll position while the launcher runs.
The library file also records when each recent server was joined, for Home's Jump back in;
a server joined by address that no list has shown is kept with the map of its match
summary. Config Studio edits the
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

<!-- MARK: - ship-configs -->
Deleting a loadout (Loadout › Delete Loadout…, More Actions or the row's context menu) asks
first, never applies to the loadout the next game uses or while the game runs, and moves the
whole `main/players/<profile>` folder to the Trash. Its advanced-binds companion travels inside
as `.studio-advanced-binds.cfg` and goes back to `main/` when the folder is read again, so Undo
(the banner's button or ⌘Z) and Finder's Put Back both restore it. Tests and the review harness
replace `ConfigStudioSession.trashItem` with a scratch folder and never use the Mac's Trash.
<!-- /ship-configs -->

Server discovery sends both protocols to both masters, collects multiple UDP
packets, deduplicates addresses and limits discovery to 2048 endpoints. Queries
use Network.framework with a maximum of six concurrent endpoints, 40 ms between
starts, status/info timeouts, cancellation, bounded parsers and a persistent
last-successful cache. Refresh is explicit unless Settings › General › Refresh the server list
when Servers opens is on (off by default); opening the app itself never scans the server list. Checking for updates is an HTTPS request to GitHub Releases, on Check Now or, with
Settings › Updates › Check for updates automatically on (the default), at most once a day while
the launcher is open; an automatic check that fails says nothing, and the review harness and the
lifecycle tests (`COD2_SETUP_NONINTERACTIVE=1`) never make one. It refreshes the release notes
and What's New, which lists the newest release's Added, Changed and Fixed notes. Home's What's New card
marks 0.3.0 as a preview without a made-up release date, and uses fetched notes for versions without a bundled headline. When a newer release carries
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
CD key or player name). With its switch on, the last game log (the retained session output, or an older console, crash
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

<!-- MARK: - ship-play -->
Display sizes use `DisplayResolutions.offered(native:modes:)` in Core.swift: native
pixels plus the engine’s standard sizes and fitting display modes, at least 640 × 480,
deduplicated and sorted by width then height. MainDisplay uses the CoreGraphics
native-mode flag, with rotation applied. If no native mode is advertised it uses
desktop coordinates conservatively, never the scaled backing or the largest mode.
Fullscreen follows the active loadout’s monitor choice; windowed uses the main display.
Screen changes, loadout changes and launch refresh the list. Unsupported saved sizes
are normalized and explained in Display; Home and Ready read that same setting.
New installs use native dimensions through 3840 × 2160, half the dimensions above it.
Old extra resolution options move to the Resolution control, which owns that choice.
Trusted command-line options after `--` remain developer overrides.

`GameSessionLog` drains stdout and stderr on a serial utility queue, filtering complete
lines before any disk write. Credential labels, key checksum matches, join URLs,
command lines and supplied passwords are removed; split writes and long lines cannot
bypass filtering. The `logs` directory is mode 700 and time-named session files mode
600. Retention is ten files and 20 MiB in total, reserving a new session’s 4 MiB limit
when deleting oldest logs. After that session limit the reader continues draining;
it does not stall the game. These session files are excluded from automatic crash
report delivery. Advanced, failed-start details and the crash notice use them.
If the launcher quits while the game keeps running, the retained log ends there.

Check-up decodes optional schema-1 `hitchCount`, `hitchesOmitted`, `hitches` and
`rendererActivity`. The bounded event list provides match times and durations;
the maximum remains authoritative when a later worst event was omitted. Older
summaries use `frameTimeMaxMS` without inventing counts or times. Renderer activity
is decoded but not displayed. Shared JSON fixtures cover new and old summaries.
About keeps the canvas spacing and all five real credit rows; a bottom fade indicates
more content when scrolling is required.
<!-- /ship-play -->
