# Native launcher

Build: `scripts/build-launcher.sh output/ws25/launcher` (Xcode Swift 6, no
packages). CMake's `cod2_launcher` and `cod2_macos_app` targets and both release
scripts use the same builder. Deployment target: arm64 macOS 13, Swift language
mode 6, complete strict concurrency, warnings as errors.

Unit tests: `sh tests/launcher/run.sh`. Public wire fixtures were captured from
both masters on 2026-10-04; `tests/launcher/fixtures/capture.json` records their
provenance. They contain public server metadata, no game assets or credentials.

Review images: `CoD2Launcher --screens output/ws25/screens` renders nine screens
in light/dark at 1440×900, using only fake servers in RFC 5737's documentation
address range. Snapshot mode does not access preferences, game content or the
network. The secure key field stays empty.

The outer app registers `cod2x`. A nested `CoD2 Game.app` owns the engine and its
fullscreen/Game Mode eligibility. The launcher becomes an accessory app during
play, then restores its Dock/window after the tracked child exits. A game crash
leaves the launcher running and exposes any native crash report in the app home.
Only a single launcher window exists; Play is guarded against duplicate children.

Fast path:

```sh
"CoD2 Silicon.app/Contents/MacOS/CoD2Launcher" --play
"CoD2 Silicon.app/Contents/MacOS/CoD2Launcher" --play --exit-after-game -- +set r_fullscreen 0
open -a "CoD2 Silicon.app" 'cod2x://connect/127.0.0.1:28960'
```

Arguments following `--` are forwarded verbatim to the engine's command parser
for trusted CLI/developer use. GUI settings and links always go through bounded
validators. Passwords are held in memory and never cached. CD keys are validated
by WS21's native CRC routine, written using mode-600 temporary files and rename,
and removed from the child environment. The raw key is never printed.

`launcher-settings.json`, `launcher-library.json`, `main/launcher.cfg` and
`data-path.txt` live in the existing app home. The launcher never rewrites
`config_mp.cfg`. Startup dvars make settings apply before renderer initialization.
Advanced values reject command separators; startup paths/credentials and upcoming
renderer options are reserved. Native Spaces sets
`SDL_VIDEO_MAC_FULLSCREEN_SPACES=1`. Metal/MetalFX/HDR/render scale remain disabled
until the renderer workstreams provide their supported interfaces.

Server discovery sends both protocols to both masters, collects multiple UDP
packets, deduplicates addresses and limits discovery to 2048 endpoints. Queries
use Network.framework with a maximum of six concurrent endpoints, 40 ms between
starts, status/info timeouts, cancellation, bounded parsers and a persistent
last-successful cache. Refresh is explicit; opening the app doesn't scan the
internet. Update checking is explicit HTTPS to GitHub Releases; it only notifies.

WS21's NSAlert setup remains available in engine-only development/test bundles
(`make_macos_app.py --engine-only`). Production bundles set automatic setup off
in the helper because SwiftUI owns setup. The existing native engine URL handler
and server-facing CoD2x 1.4.6.8 identity are untouched.
