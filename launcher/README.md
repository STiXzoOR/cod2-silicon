# Native launcher

Build: `scripts/build-launcher.sh output/ws28/launcher` (Xcode Swift 6, no
packages). CMake's `cod2_launcher` and `cod2_macos_app` targets and both release
scripts use the same builder. Deployment target: arm64 macOS 13, Swift language
mode 6, complete strict concurrency, warnings as errors.

Source layout:

- `Core.swift`, `Presentation.swift`: validated settings, links, key storage and the
  Foundation-only formatting rules (maps, server badges, cm/360, release notes,
  scrims). Unit-tested.
- `Network.swift`, `Model.swift`, `MapArtwork.swift`: discovery, lifecycle, and the
  player's own loading screens decoded into a private cache.
- `Theme.swift`: colour tokens for dark and light, the bundled typefaces, and the
  glass surfaces and buttons with their macOS 13–25 material fallback.
- `Artwork.swift`: original fallback art drawn from the design's vector data, icon G
  at sidebar scale and the dog tags.
- `RootView.swift` plus one file per screen: Home, Servers, Settings, Setup,
  Library and About.

Unit tests: `sh tests/launcher/run.sh`. This runs the core and network tests, the
artwork decoder tests, the font checksum check, a strict typecheck of the
pre-macOS 26 material branch, and a render of every screen in both appearances.
Public wire fixtures were captured from both masters on 2026-10-04;
`tests/launcher/fixtures/capture.json` records their provenance.
`fixtures/releases.json` is a synthetic GitHub Releases response.

Review images come from a test-only harness that is never shipped:

```sh
scripts/build-launcher.sh output/ws28/harness --snapshots
output/ws28/harness/LauncherSnapshots output/ws28/no-data
output/ws28/harness/LauncherSnapshots output/ws28/fallback --fallback
output/ws28/harness/LauncherSnapshots output/ws28/with-data --data ~/Games/CoD2
```

It hosts the real views in a window configured like the app's, and captures it
with `CGWindowListCreateImage`, so Liquid Glass and toolbars render for real.
`with-data` images contain the player's licensed loading screens: keep them
local. Snapshot mode uses fake servers in RFC 5737's documentation ranges,
invented player names and an invented key, and performs no I/O.

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
Only a single launcher window exists; Deploy is guarded against duplicate children.

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

`launcher-settings.json`, `launcher-library.json`, `main/launcher.cfg` and
`data-path.txt` live in the existing app home. The launcher never rewrites
`config_mp.cfg`; it only reads the player's `name` from it for the sidebar tag.
Startup dvars make settings apply before renderer initialization.
Advanced values accept `dvar value` or `dvar=value`, reject command separators,
and the complete startup is limited to the engine's 31 commands/3900 bytes;
startup paths/credentials and upcoming renderer options are reserved. Native
Spaces sets `SDL_VIDEO_MAC_FULLSCREEN_SPACES=1`. Anisotropic filtering maps to
`r_anisotropy`. The renderer choice, render scale, MetalFX and HDR are shown as
upcoming until the Metal renderer provides `r_renderer`.

Server discovery sends both protocols to both masters, collects multiple UDP
packets, deduplicates addresses and limits discovery to 2048 endpoints. Queries
use Network.framework with a maximum of six concurrent endpoints, 40 ms between
starts, status/info timeouts, cancellation, bounded parsers and a persistent
last-successful cache. Refresh is explicit; opening the app doesn't scan the
internet. Checking for updates is an explicit HTTPS request to GitHub Releases;
it refreshes Home's Dispatches and only notifies.

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
