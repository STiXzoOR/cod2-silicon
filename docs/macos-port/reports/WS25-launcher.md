# WS25 — native SwiftUI launcher

Branch: `port/launcher`. Base: `3ef770ba088808f87805ba748839db2ca53e9dad`.
Worktree: `/Users/stix/Projects/cod2-native-wt/launcher`.
Verified on arm64 macOS 27.0.1 with Apple Swift 6.4, using the existing tools.

## Result and architecture

The production bundle now opens a native SwiftUI launcher. Play starts the
existing engine in `Contents/Helpers/CoD2 Game.app`; quitting the game restores
the launcher. Swift builds in language mode 6 with complete strict concurrency
and warnings as errors, targeting `arm64-apple-macos13`.

The outer executable is `Contents/MacOS/CoD2Launcher`. The nested helper uses
the outer bundle identifier plus `.game`, the same original icon, and
`LSSupportsGameMode=true`. Only the launcher registers `cod2x`. The engine is
a separate process: its crash cannot tear down SwiftUI. While playing, the
launcher switches to accessory activation and hides its window; the game stays
a regular, foreground app. Termination restores the launcher's regular Dock
presence and window, with a Show Crash Report action for a fresh native report.
This was chosen over in-process hosting for crash isolation, and over a bare
CLI child so fullscreen has a game app identity. Actual macOS Game Mode
activation remains a visual acceptance check; plist eligibility alone is not
proof that the system engaged it.

Normal launcher Quit is deferred while playing. If the launcher is force-quit,
reopening it attaches to the exact surviving helper by bundle ID **and resolved
executable path** instead of starting a second game. Its eventual termination
restores the launcher; an attached process's exit status is unknown. The
synthetic lifecycle test verifies the activation policies, reconnect and crash
report presentation. It does not count visible Dock tiles or certify Game Mode.

`--play` bypasses Home after setup. `--exit-after-game` also terminates the
launcher after the game returns. Trusted engine CLI arguments follow `--`.
Cold `cod2x://` links and server-browser joins start the game after setup, then
deliver the validated link through the existing engine Apple-event handler as
soon as its native app has finished launching. Live links target that same PID.
Passwords stay in memory, never argv or the server cache. This is necessary
because the engine splits startup commands at every `+`, even inside quotes.
The native URL parser and server-facing CoD2x 1.4.6.8 identity are unchanged.

## Screens and behavior

`launcher/Design.swift` supplies reusable panels, labels, metrics, colour-code
text, and original procedural terrain art. Materials, system fonts/accent,
SF Symbols, light/dark appearance and SDK/runtime-guarded Liquid Glass on
macOS 26+ are used. SDKs before 26 compile the material branch without referring
to unavailable Glass symbols. That branch passes a separate strict Swift
typecheck (`fallback-typecheck.log`). Native controls have accessibility labels; primary actions
have keyboard shortcuts. The 13–25 material fallback is compiled but was not
run on an older OS, and a full VoiceOver walkthrough remains to be done.

All review images are local, ignored PNGs in `output/ws25/screens/`, rendered
offscreen with `NSHostingView` at **1440×900**. They use fake documentation-range
servers, invented player names and media entries, an empty secure key field,
and original artwork. Snapshot mode performs no discovery, setup or network
operations. Every listed light/dark image was visually inspected.

| Screen | Description | Light PNG | Dark PNG |
| --- | --- | --- | --- |
| Game data | Three-step welcome, folder selection and explanation of the 16 licensed archives | `output/ws25/screens/onboarding-data-light.png` | `output/ws25/screens/onboarding-data-dark.png` |
| CD key | Secure entry, native checksum validation and private storage explanation | `output/ws25/screens/onboarding-key-light.png` | `output/ws25/screens/onboarding-key-dark.png` |
| Shaders | Native extraction/verification progress; explicit approximate-shader acceptance if unavailable | `output/ws25/screens/onboarding-shaders-light.png` | `output/ws25/screens/onboarding-shaders-dark.png` |
| Play/Home | Large Play action, current settings, last server and release news/update links | `output/ws25/screens/home-light.png` | `output/ws25/screens/home-dark.png` |
| Servers | Search/filter/sort, favorites/recents, colour names, player inspector and direct connect | `output/ws25/screens/servers-light.png` | `output/ws25/screens/servers-dark.png` |
| Settings | Display/frame cap/mouse/audio, advanced dvars and disabled upcoming renderer controls | `output/ws25/screens/settings-light.png` | `output/ws25/screens/settings-dark.png` |
| Demos | Engine-supported `.dm_1` entries, playback action and Open Folder | `output/ws25/screens/demos-light.png` | `output/ws25/screens/demos-dark.png` |
| Screenshots | Local image entries, open image and Open Folder | `output/ws25/screens/screenshots-light.png` | `output/ws25/screens/screenshots-dark.png` |
| About | Version/compatibility, credits, licence notice and notification-only update check | `output/ws25/screens/about-light.png` | `output/ws25/screens/about-dark.png` |

The icon is `output/ws25/screens/app-icon-1024.png` (**1024×1024**), generated by
the rewritten `tools/cod2x/app_icon.swift`: shaded terrain, sun and a folded
compass star in a layered rounded tile. It contains no Activision logo or game
asset. The bundle carries an `.icns`; a layered Icon Composer `.icon` asset is
not produced by this command-line build.

Onboarding directly reuses WS21's native discovery/migration/validation and
shader extractor through a narrow Objective-C bridge. The raw key is never
printed, is removed from the child's test-override environment, and is saved
with exclusive mode-600 temporary storage, fsync and rename. Existing preference
lines are retained. An explicit setup-complete marker remembers approximate
shader acceptance. Shader progress describes coarse phases, not each payload.

The browser sends `getservers 118 full empty` and `getservers 120 full empty` to
both `cod2master.activision.com:20710` and `master.cod2x.me:20710`. Network.framework
collects multiple master packets, deduplicates up to 2048 endpoints, and queries
status with info fallback: at most six endpoints concurrently, 40 ms between
starts, bounded parsers, timeouts and cancellation. Ping measures the first
reply. The inspector shows version, mod, advertised cap, password and players;
the table shows coloured name, map, gametype, players/max and ping. Refresh is
explicit; last successful results, favorites and recent addresses persist
locally. Cached results remain usable when discovery fails.

Settings retain 1080p/exclusive/333 fps/vsync-off/raw-mouse defaults. Resolutions
reach 6016×3384; exclusive, borderless, windowed and native Spaces are exposed.
Spaces uses SDL's existing hint. Frame cap presets are 333/250/125 plus custom.
Sensitivity includes a DPI/cm-per-360 helper; audio and advanced `dvar=value`
entries are supported. Values reject command injection, and the complete
startup is bounded to the engine's 31 commands/3900 bytes. Startup paths,
credentials and upcoming renderer variables are reserved. The writer creates
`main/launcher.cfg` and launcher JSON files; it never rewrites `config_mp.cfg`.
Render scale, Metal renderer, MetalFX and HDR are visibly disabled until WS23/24
provide supported interfaces. Demos use the existing engine playback command.
News links lead to releases; explicit HTTPS GitHub Releases API checks only
notify, with no automatic download/install.

## Build and packaging evidence

`scripts/build-launcher.sh` uses xcrun clang/swiftc, Foundation/AppKit/SwiftUI and
Network.framework without SwiftPM dependencies or an Xcode project. CMake adds
`cod2_launcher` and makes `cod2_macos_app` depend on it only in the native CoD2x
block. Source builds require Swift 6/Xcode 16 or newer; the prebuilt executable
targets macOS 13. Release/install entry scripts keep their existing interface.

The nested helper owns the SDL Frameworks directory, retaining the engine's
existing executable-relative rpath. Four real Mach-Os (launcher, engine, SDL2
compatibility and SDL3) are arm64 with `minos 13.0`, only bundle/system links,
ad-hoc signatures and successful `codesign --verify --deep --strict`. The app
contains licences/notices and no game archives, shaders, Python or credentials.
`--engine-only` keeps WS21's engine fixtures and NSAlert setup available without
changing any engine source. Production disables helper setup because SwiftUI
owns it. Atomic replacement safeguards remain in the bundler.

Commands run from this worktree (build/fixture compilation used
`taskpolicy -b nice -n 19`):

```sh
scripts/build-launcher.sh output/ws25/launcher
sh tests/launcher/run.sh
output/ws25/network/LauncherNetworkTests --live
python3 tests/launcher/lifecycle.py
python3 tests/launcher/cold_url.py
python3 tests/packaging/first_run.py
python3 tests/packaging/shaders.py \
  "$HOME/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer" \
  "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
COD2_BUILD_BACKGROUND=1 scripts/package-release.sh --build-dir build/package/ws25
COD2_BUILD_BACKGROUND=1 scripts/install.sh \
  --prefix output/ws25/install/Applications --build-dir build/package/ws25
cmake --build build/package/ws25/client --target cod2_macos_app --parallel 3
ditto -x -k dist/CoD2-Silicon-0.1.0-macos-arm64.zip output/ws25/extracted-release
python3 tests/packaging/launcher_bundle.py 'output/ws25/extracted-release/CoD2 Silicon.app'
python3 tests/packaging/launcher_bundle.py 'output/ws25/install/Applications/CoD2 Silicon.app'
'build/package/ws25/CoD2 Silicon.app/Contents/MacOS/CoD2Launcher' --screens output/ws25/screens
python3 tests/packaging/release_smoke.py 'build/package/ws25/CoD2 Silicon.app' \
  --game "$HOME/Games/CoD2" \
  --mac-binary "$HOME/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer" \
  --output output/ws25/release-smoke
```

| Check | Evidence/result |
| --- | --- |
| Strict Swift build, core and Network UDP tests | PASS; `launcher-build.log`, `launcher-tests-final.log` |
| Public master/status fixtures | PASS; both masters/protocols captured 2026-10-04, provenance committed in `tests/launcher/fixtures/capture.json`; malformed data, Latin-1 text, colour codes, native CRC, private storage, config and URL injection tested |
| Actual Network.framework discovery | PASS; 464 endpoints, three live status/info replies and first-reply RTT; `live-network.log` |
| Isolated child, live links, reconnect, crash/report and Dock policies | PASS with synthetic helper; `lifecycle-final.log` |
| Actual cold LaunchServices URL | PASS; scratch onboarding, valid `+` password deferred without argv exposure, return to launcher; `cold-url.log` |
| WS21 first run/migration | PASS; `first-run.log` |
| Native shader parity | PASS with both owned Mac binaries: 834 payloads plus manifest byte-identical to Python; corruption, reuse and fallback checked; `native-shaders.log` |
| Release script, scratch install and CMake app target | PASS; `package-final-refresh.log`, `install-final.log`, `cmake-app.log` |
| Extracted ZIP and installed bundle audit | PASS; `extracted-bundle-audit.log`, `install-bundle-audit.log` |
| Real menu → Toujane → quit → launcher | PASS in a scratch home; exit 0, two JPEGs, original shaders and nested bundled SDL2/3; `release-smoke/results.json` |
| Final installed smoke | PASS; scratch-home onboarding → menu → Toujane → quit/launcher, exit 0 and bundled SDL2/3; `install-smoke/results.json`, `install-smoke.log` |
| Final ZIP after SDK guard | PASS; fresh extraction audit and another scratch-home menu → Toujane → launcher run; `final-zip-audit.log`, `final-zip-smoke/results.json`, `package-sdk-guard.log` |
| Refreshed installer after packaging commit | PASS; another scratch-home menu → Toujane → launcher run with rebuilt helper; `final-install-smoke/results.json`, `final-install-smoke.log` |
| Screens/icon | PASS dimensions and visual inspection; `screens-final.log`, `icon.log` |
| Static checks | PASS shellcheck, bash syntax, Python compilation, `git diff --check` |

All log paths in this report are under ignored `output/ws25/`. Real-game
captures and extracted shaders stay there locally and are not committed. Test
bundles that register with LaunchServices are unregistered, including nested
helpers. The real `~/Applications/CoD2 Silicon.app` was not overwritten.
The smoke inventory records PID/comm only, avoiding credentials in command
lines. Tests clean up only their own process groups. No performance measurement
was performed, so no benchmark lock was acquired. Real smoke launches waited
while other agents' game processes were active.

The last SDK-aware release was extracted into `output/ws25/extracted-sdk-release`
and run with the same `release_smoke.py` command above, substituting that app
path and `--output output/ws25/final-zip-smoke`. The review PNGs were refreshed
from that final extracted executable. The scratch installer was refreshed from
the same cached launcher build (`install-sdk-guard.log`). Its launcher UUID and
all bytes before `LC_CODE_SIGNATURE` match the smoke-tested ZIP executable.
A whole-file SHA equality assertion failed: the outer embedded signature seals
the rebuilt nested helper's different code hash. Both signatures independently
pass deep verification. The corrected comparison is recorded in
`launcher-code-comparison.log`. This was an overly strict provenance assertion,
not a launcher code mismatch. A final smoke against the refreshed installation
also passes (`final-install-smoke/results.json`).

## Merge gate

Both Release clients built successfully with the CONTRIBUTING configurations:
`build-macos` (stock) and `build-macos-codx` (`-DCOD2_CODX=1`), `COD2_X64=ON`,
arm64, target `cod2_macos`, parallel 3. Logs: `configure-stock.log`,
`configure-codx.log`, `build-stock.log`, `build-codx.log`. Actual engine compile
databases were preserved as `compile_commands.engine.json`; fixture databases
use `-UNDEBUG` as CONTRIBUTING instructs.

The entire listed CONTRIBUTING suite was attempted. Individual commands,
exit codes and logs are recorded in `gate/results.json`. Passing suites:

- Tool tests (14/14 on direct retry), datagen tests and synthetic ABI checkers.
- Shader setup and Wine trace parser tests; sanitized full CoD2x fixtures,
  native CoD2x fixtures and SDK identity.
- Stock online fixtures and CoD2x fixes13 fixtures; perf source fixtures.
- LP64 game/script and all renderer source/raster/mips/volume fixtures.
- HUD/impact/all FX fixtures and dedicated FX fixture.
- Platform CTest: 25/25 sanitized and 27/27 plain, including diagnostics/crashes.
- Private typed-data round trip, game layouts and fixes13 reference comparison.

Two **unresolved harness failures** prevent claiming a completely green gate:

1. `python3 tests/online/run.py build-macos-codx/compile_commands.json` fails to
   link the timeout fixture: `_Cod2x_Frame` and `_Cod2x_DemoClientFrame` are
   missing. The stock invocation passes. See `gate/10.log`.
2. `python3 tests/fixes13/run.py --build build-macos` passes its initial cases,
   then tries to extract native-CoD2x-gated `Cod2x_LimitedFPS` from the stock
   configuration and raises `ValueError`. The CoD2x invocation passes. See
   `gate/11.log`.

Their source/harness files are untouched by this branch; no launcher regression
has been inferred from these failures and no unrelated fix was made. The
initial background tools test had an EPERM subprocess failure; direct retry
passed all 14 tests (`tools-tests-retry.log`). An initial ABI driver timeout and
one missing scratch-driver invocation were retried, not counted as passes.

Full ABI audit final result: **zero mismatches** for both builds. Stock audits
620 translation units and 1776 native import sites; CoD2x audits 637 translation
units and 1778 native import sites. Each reports 218 renderer bindings, zero
named cast mismatches, zero unprototyped floating calls, and zero proven missing
or extra import dereferences. JSON evidence is `abi-stock/{functions,callbacks,
imports}.json` and `abi-codx/{functions,callbacks,imports}.json`; final logs are
`abi-stock-final.log` and `abi-codx-retry.log`.

The initial `sh tools/abi/check.sh ...` timed out at the local runner's 900-second
limit. Its constituent checks were all completed with these commands for each
build, after independently passing `test_audit.py` and `test_imports.py`:

```sh
# Substitute build-macos-codx and output/ws25/abi-codx for the second audit.
nice -n 19 python3 output/ws25/abi-driver.py \
  build-macos/compile_commands.engine.json --output output/ws25/abi-stock/functions.json \
  --baseline tools/abi/baseline.json --jobs 4
python3 tools/abi/callback_tables.py build-macos/compile_commands.engine.json \
  --audit output/ws25/abi-stock/functions.json --baseline tools/abi/callback-baseline.json \
  --output output/ws25/abi-stock/callbacks.json
python3 tools/abi/imports.py --compile-commands build-macos/compile_commands.engine.json \
  --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386" \
  --json output/ws25/abi-stock/imports.json --check
```

The retry uses the unchanged auditor with four workers and per-build caches;
an ignored wrapper keeps Clang compilations in background scheduling while
allowing Python AST parsing to use available cores. No flags, baselines or
checker logic were changed. Synthetic audit/import tests passed separately.

Legacy checks against the base commit all pass:

```sh
python3 tests/fixes13/legacy.py --base 3ef770ba088808f87805ba748839db2ca53e9dad
python3 tests/cod2x/check_inactive_gates.py --base 3ef770ba088808f87805ba748839db2ca53e9dad
python3 tools/abi/legacy.py --base 3ef770ba088808f87805ba748839db2ca53e9dad
```

They report zero changed legacy source files, zero mismatches over respectively
10, 17 and 5 configurations (`legacy-fixes.log`, `legacy-codx.log`,
`legacy-abi.log`). No engine C file, shared header, top-level CMake, original
data blob or legacy flag changed. A real i386 object/binary comparison requires
the Linux multilib host from CONTRIBUTING; this Darwin arm64 host cannot prove
it. No system packages were installed to work around that limitation.

## Review, remaining acceptance and merge notes

A separate read-only reviewer checked the branch. Findings fixed with
regressions: encoded `+` in a cold URL password must use deferred Apple events;
startup must respect the engine's 31-command bound; launcher force-quit/reopen
must reconnect to its surviving child; inventories must avoid full argv;
shutdown-racing URLs must be drained; offline favorites must not mask discovery
failure; only engine-supported `.dm_1` demos are listed.

Remaining acceptance: actual fullscreen Game Mode and visible single-Dock
behavior, macOS 13–25 execution/fallback appearance, thorough VoiceOver and
keyboard walkthrough, high-resolution display mode acceptance and real demo
playback. Master queries are live-tested, but authenticated public gameplay and
all stock/CoD2x mods are not. Render scale and Metal/MetalFX/HDR need WS23/24's
interfaces; news is a releases link rather than a fetched editorial feed.
Updates only notify. macOS 26 layered Icon Composer assets can be added later.

Merge this branch as focused signed-off commits. New launcher, tests and builder
files own most of the change. Shared integration is confined to a small native
CoD2x block in `cmake/macos-arm64.cmake`; no `CMakeLists.txt` or `src/headers/*`
changes. Existing packaging fixtures explicitly request `--engine-only`.
WS23 may need to map new presentation dvars to `GameSettings`; WS24 can enable
the reserved renderer settings once supported. Keep the nested game app's
identity, Frameworks rpath, automatic-setup-off flag and sole outer URL owner.
The two harness failures above and Linux byte comparison need orchestrator
follow-up. No pushes, PRs, issues, remote changes or sibling-worktree file access
occurred.
