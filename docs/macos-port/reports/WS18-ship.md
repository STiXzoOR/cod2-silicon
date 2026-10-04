# WS18 — playable release, firing and first-launch setup

The first-shot crash is fixed and the signed app is installed at
`~/Applications/CoD2x Native.app`. Real Steam shaders provision automatically;
grounded combat passes 10 minutes and all twelve benchmark runs complete.
Active Game Mode/input focus, a literal otherwise-empty system, plain URI
default-handler selection and stable capped 333 FPS remain acceptance gaps.

Worktree: `~/Projects/cod2-native-wt/ship`, branch `port/ship`,
base `7dbfcea`. Host: Apple M6, 24 GiB, macOS 27.0.1 (26A434), SDL 2.32.72.
No packages installed, remotes changed, pushes, PRs or issues. Licensed data,
extracted shaders, demos, screenshots and diagnostic binaries remain outside
tracked files. No sibling worktree files were inspected or modified. One
plain-URI test unintentionally launched an older registered test bundle via
LaunchServices; only that test-started process was terminated, using its exact
new PID and start time. Its files were not inspected.

## Installation and play

From the merged source tree, using the existing Xcode/SDL/Python tools:

```sh
cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos-codx --target cod2_macos_app -j12
open -a "$HOME/Applications/CoD2x Native.app"
```

Double-click `~/Applications/CoD2x Native.app` thereafter. It uses the licensed
data in `~/Games/CoD2`; if that folder is absent, choose the folder containing
`main/iw_00.iwd` through `iw_15.iwd`. The choice is remembered in
`~/Library/Application Support/CoD2x Native/data-path.txt`. Configs, demos and
screenshots use `~/Library/Application Support/CoD2x Native`. The CD key remains
in `~/.cod2/preferences`; no key migration or replacement was added.

Defaults are exclusive 1920×1080 fullscreen, `com_maxfps 333`,
`r_swapInterval 0`, `in_rawmouse 1`, `m_filter 0`, `cl_mouseAccel 0`,
`com_introPlayed 1`, `developer 0`, `logfile 0`. Explicit launch arguments can
override them. These are startup defaults, not a promise of sustained 333 FPS.

The first launch displays a preparation progress bar and extracts 834 shader
and constant files from the user's Steam Mac binary:

```text
~/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer
```

The destination is `~/Library/Application Support/CoD2x Native/shaders`.
Subsequent launches verify each SHA-256 entry in `manifest.json` and reuse it.
The launcher sets `COD2_MAC_SHADER_CACHE` itself. A damaged cache is rebuilt;
concurrent setup is serialized by a lock. If the primary binary cannot be
extracted, the alternate is
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`. If neither extraction
nor Python 3 is available, the launcher prints a clear message and continues
with approximation rendering. It never installs Python or downloads assets.

```sh
open -a "$HOME/Applications/CoD2x Native.app" 'cod2x://connect/127.0.0.1:28960'
```

Use the multiplayer menu/server browser or replace that local example with
the intended server address. Plain `open 'cod2x://…'` selected an older test
bundle on this host, which has duplicate registered bundle identifiers. The
explicit app command above is verified. No global handler preference or
sibling bundle was changed; removing old registrations/selecting the desired
default handler remains an orchestrator/user step. The builder refuses to overwrite an app with a
different bundle ID; `--replace` rebuilds only this app. It stages the new
bundle, verifies its ad-hoc signature, and then replaces the old bundle.

## Firing crash: root cause and fix

This was a command-producer type error, not tessellation capacity or an ARB
index-count overflow. A muzzle flash adds a point light. `R_RenderScene` then
queues a full-screen stencil clear. Its local declaration and sole call had
the material and color arguments reversed. The command received `colorWhite`
as a `Material *`. `RB_DrawStretchPic` began that bogus material and the next
`RB_SetLightPropertiesCmd` flushed it through `RB_EndSurface`. Float color
bytes became the technique-set address `0x3f8000003f800000`, leading to the
reported read at `0x3f8000003f800020`.

Temporary command and surface probes identified the exact `colorWhite`
address using the loaded binary's symbol slide. The reference Mac decompile
confirms color-before-material in both the declaration and call. No dump was
copied into the repo. `src/PC/gfx_d3d/r_scene.c` now uses that order under
`COD2_X64`, retaining the original declaration and call in the legacy branch.
All temporary probes were removed before committing.

Before the fix, the grounded first Sten shot reproducibly exited `-11` with
real shaders. After the fix, the identical test fired repeatedly, threw smoke
and frag grenades, saved the final impact screenshot and exited 0. Evidence:
`~/Library/Application Support/CoD2-native-ws18/evidence/ws15-baseline.log`,
`ws15-fixed.log`, and corresponding native crash/screenshot files. The base
binary is preserved only in ignored `output/ws18/before-client`.

An ASan recover-mode run reproduced the same invalid material read. It also
reported pre-existing placeholder/global bounds issues (`bg_numItems`, the
viewport behavior table); it is not an ASan-clean whole-game acceptance run.
There was no evidence that enlarging tess buffers would fix this crash.

The final grounded local combat soak, with the real shader cache enabled,
completed 628.948 seconds and 20 cycles with exit 0. It repeatedly fired the
Sten and Webley, reloaded, moved, threw frag grenades and threw smoke grenades.
A separate Lee-Enfield session completed 62.811 seconds, two cycles and exit
0. Rifle, SMG and pistol screenshots show live view models and ammo
consumption; all grenade commands reached the engine. Evidence is in
`output/ws18/combat-sten/` and `combat-rifle/`, including command timestamps,
demos, screenshots and results JSON. The soak had no scene-entity overflow
message or crash.

```sh
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2x Native/shaders" \
  python3 tools/macos/combat.py build-macos-codx/cod2_macos \
  --output output/ws18/combat-sten --seconds 600
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2x Native/shaders" \
  python3 tools/macos/combat.py build-macos-codx/cod2_macos \
  --output output/ws18/combat-rifle --primary enfield_mp --seconds 45
```

Choose a new output directory when repeating these commands. Early driver
attempts that used `kill` to switch loadouts left a dead player; they are
excluded from acceptance. The final driver starts alive with the chosen
primary and cycles the pistol without respawning. Some gazebo grenade
screenshots show stray arm/view-model geometry; world/render parity while
firing or throwing was not remeasured against Windows. This is a visual
follow-up, distinct from the fixed crash, and no new parity MAE is claimed.

## Shader and bundle verification

The primary Steam binary is the requested symbolized i386 build and extracts
successfully. Both primary and reference yield 834 files; comparing every
payload found zero byte differences. The primary binary SHA-256 is
`a6aa70d4e2b0bf5f388b0653752847bddac95398ee0d860e757ec79e466f5d1b`.
No fallback was needed on this host.

```sh
python3 tests/rendering/shader_setup.py
python3 tools/macos-port/extract_shaders.py /nonexistent \
  "$HOME/Library/Application Support/CoD2x Native/shaders" --setup
plutil -lint "$HOME/Applications/CoD2x Native.app/Contents/Info.plist"
codesign --verify --deep --strict --verbose=2 \
  "$HOME/Applications/CoD2x Native.app"
```

The three synthetic manifest tests pass; they reject changed/missing bytes,
path traversal, wrong counts and malformed manifests. Cache reuse succeeds
even with an absent input binary. A separate empty-cache invocation with two
absent binaries returns failure and prints the approximation message. The
bundle passes plist and signature validation. Its icon is generated from
original geometric shapes and text by `tools/cod2x/app_icon.swift`.

`output/ws18/app-cold/console.log` shows actual LaunchServices startup via
`open -n -a`, first-launch extraction progress through 834/834, and queried
defaults/path values exactly as listed above. A second cold start via a
`cod2x://connect/127.0.0.1:29999` link also extracted a missing cache, reached a
local UDP listener with `getchallenge`, and quit cleanly; evidence is in
`output/ws18/cold-url/`. Existing encoded CoD2x command URLs are still accepted;
the requested simple path form now has regression coverage. SDL's Cocoa
initialization replaces the original AppleEvent handler, so native CoD2x
restores its validated handler after `SDL_InitSubSystem` and installs it
before first-launch setup pumps events. Cache setup strips client observer
environment variables from its Python child so it cannot overwrite the
client ownership PID. Runtime URL capture contains identity data and stays
ignored; only the command type is recorded here.

`sh tests/cod2x/run_native.sh` passes after updating the existing input fixture
for WS17's SDL pump/peep API and the bundle fixture for prepared launch paths.
The initial fixture run failed to link three now-used SDL functions; no engine
fix was needed. Tests cover URL validation, AppleEvent command queue, app
arguments, mouse input and native watchdog/crash reporting.

Post-launch signature verification initially failed because Python imported
the bundled helpers and wrote `__pycache__` into sealed Resources. Extraction
now uses Python `-B`; the integration fixture verifies the signature after
runtime setup. The installed app was rebuilt, launched again from a cold
shader cache via a URL, quit, and passed `codesign --verify --deep --strict`.
Final evidence: `output/ws18/cold-url-sealed/`, `signature-final.log` and
`cod2x-native-final.log`. The final `-B` change affects setup only; the six
benchmark captures preceded it and use identical engine/shader defaults.

A separate test-only bundle with its extraction helper removed, and the
support cache temporarily absent, printed the approximation message plus
`Using generic shaders`, loaded local Toujane, and exited through `quit`.
The real support cache was restored. This verifies runtime fallback rather
than only the helper CLI's failure return. See `output/ws18/fallback-runtime/`.
That test bundle registers no URL scheme and is not the installed deliverable.

## ABI and legacy verification

```sh
sh tools/abi/check.sh build-macos-codx/compile_commands.json output/ws18/abi
python3 tests/fixes13/legacy.py --base 7dbfcea
python3 tests/fixes13/run.py --build build-macos-codx
python3 tests/perf/run.py
ASAN_OPTIONS=symbolize=0 sh tests/lp64/renderer/run.sh
python3 tests/platform/run_impact_marks.py
python3 tests/platform/run_fx_events.py
python3 tests/platform/run_fx_primitives.py
python3 tests/platform/run_fx_cloud.py
```

The ABI audit passes with 636 TUs, zero errors/mismatches/new mismatches and
218 renderer bindings with zero table/cast/unprototyped floating-call
mismatches. Legacy preprocessing compares four changed source files in ten
configurations with zero mismatches against `7dbfcea`; it verifies unchanged
legacy source expansion rather than claiming an unavailable i386 macOS link.
The fixes13, renderer, performance fixtures, impact marks and FX fixtures pass.
Build and audit logs are under ignored `output/ws18/`.

## Runtime acceptance and performance

The diagnostic app run `output/ws18/shader-draw-proof/console.log` confirms
actual `Native ARB draw: program=14 ...` dispatch, with no ARB GL-error message.
Diagnostics were disabled for the six measurements. The benchmark environment
had no `D3D_PROG=0` override. Program/texture creation counters are zero during
the warmed captures; program creation happens before measurement.

Runtime launch, URL, Game Mode and benchmark evidence follows. Benchmarks do
not overlap our other clients,
compilers, tests or profilers. Pre-existing user processes are left running:
other user applications and an unowned powermetrics process. Therefore these
measurements cannot honestly meet a literal "nothing else running" condition.

Game Mode **activation is not verified**. The plist has the requested action
game category and `LSSupportsGameMode=true`. System logs identify the exact
bundle process with `labelReason=LSSupportsGameMode (Info.plist)`, then report
paused/available and off/unavailable on exit, with no active ON state.
`output/ws18/game-policy-app-cold.log`, `game-policy-spaces.log` and
`game-policy-active.log` preserve that evidence. A controlled
`SDL_VIDEO_MAC_FULLSCREEN_SPACES=1` attempt and a temporary explicit Cocoa
foreground-activation experiment did not establish ON or input focus. The
temporary activation change was removed. Desktop control could not bind this
game window: first `timeoutReached`, then `cgWindowNotFound` (server -10005).
Therefore the menu-bar indicator and actual mouse/keyboard focus were not
verified. Apple's [Game Mode requirements](https://support.apple.com/en-mide/105118)
require macOS's built-in fullscreen; an eligible plist alone is insufficient.
SDL's [Cocoa implementation](https://raw.githubusercontent.com/libsdl-org/SDL/SDL2/src/video/cocoa/SDL_cocoaevents.m)
also verifies why the URL handler needs reinstalling after initialization.

The first-run folder picker is implemented but its interactive UI was not
exercised; the existing default data folder and prepared filesystem paths were
verified through engine dvar queries. The cold extraction console progress
is verified; visibility of the Cocoa progress window was not established
through the unavailable game UI surface.

### Measurement commands and results

All six live runs launch the installed signed app via LaunchServices. Each
has a fresh test home, a warmed stock `devmap mp_toujane`, Lee-Enfield fire,
reloads, forward/back movement, frag and smoke every 30 seconds, and 120
seconds of observer data. The view starts at the stock street spawn. The
1080p capped run recorded `ws18_bench.dm_1`; the same 2,420-frame demo is used
in all timedemos. Assets are read through an external symlink-only data alias
at `~/Library/Application Support/CoD2-native-ws18/bench-data`; no data or demo
was added to the repo. Captures and JSON/CSV results are in
`output/ws18/bench/{live,demo}-RESOLUTION-CAP/`. All live shutdowns report
`quit`; all timedemos report completion and matching frame CSV counts.

Repeat for each resolution (`1920x1080`, `2560x1440`, `3840x2160`) and cap
(`333`, `0`), using new output directories:

```sh
SDL_VIDEO_MAC_FULLSCREEN_SPACES=0 python3 tools/macos/live-bench.py \
  "$HOME/Games/CoD2" --app "$HOME/Applications/CoD2x Native.app" \
  --output output/ws18/bench/live-1920x1080-333 \
  --window-mode fullscreen --resolution 1920x1080 --maxfps 333 \
  --seconds 120 --combat --record ws18_bench
COD2_BINARY="$HOME/Applications/CoD2x Native.app/Contents/MacOS/cod2_macos" \
  python3 tools/macos/benchmark.py \
  "$HOME/Library/Application Support/CoD2-native-ws18/bench-data" ws18_bench \
  --output output/ws18/bench/demo-1920x1080-333 \
  --window-mode fullscreen --resolution 1920x1080 --maxfps 333
```

For a new machine, make that external alias's `main/*.iwd` symlinks point to
the owned game install and `main/demos/ws18_bench.dm_1` point to the first live
recording. Omit `--record` on later live runs. Timedemos disable file logging
and use the existing `cl_freezeDemo 1` measurement path (50 ms demo stepping,
1 ms engine timestamp precision). Live intervals use Mach monotonic time.
The average is reciprocal mean interval; 1% low is reciprocal mean of the
slowest ceil(1%) intervals; P99 uses nearest rank. Timedemo average is the
engine's completed summary. The observer increases its fixed sample capacity
to 262,144, so no two-minute uncapped run was truncated.

| Render size | Cap | Live average FPS | Live 1% low FPS | Live P99 ms | Timedemo average FPS | Timedemo 1% low FPS | Timedemo P99 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920×1080 | 333 | 331.2 | 205.2 | 4.261 | 330.2 | 219.3 | 4 |
| 1920×1080 | 0 | 798.6 | 354.1 | 2.459 | 843.2 | 324.7 | 2 |
| 2560×1440 | 333 | 332.4 | 233.0 | 3.827 | 333.2 | 308.6 | 3 |
| 2560×1440 | 0 | 663.5 | 326.8 | 2.696 | 788.0 | 304.9 | 3 |
| 3840×2160 | 333 | 332.3 | 230.8 | 3.936 | 331.6 | 238.1 | 4 |
| 3840×2160 | 0 | 417.1 | 256.9 | 3.407 | 413.3 | 219.3 | 4 |

The live viewport and CGL backing match each requested render size, with
swap interval 0, 60 Hz display and QoS `0x21`. 1080p uses exclusive fullscreen
(`windowFlags=0x7`). Exact 1440p and 4K display modes were unavailable in this
session, so they use the existing desktop-fullscreen fallback
(`windowFlags=0x1007`) on the 1920×1080 physical display. These are real 1440p
and 4K render workloads, **not physical 1440p/4K exclusive display tests**.
All six captures have no presentation changes. None establishes input focus
or active Game Mode. 1080p remains the requested conservative default. The
capped runs do not satisfy a stable 333 FPS/333 FPS 1% low acceptance target.

### Hitch evidence, no optimization

Across all six live captures, 52 frames exceed 6 ms. Measured sources:

1. **Buffer swap:** 45 of those frames spend more than half their interval in
   `SDL_GL_SwapWindow`. The worst 1080p capped frame is 16.8575 ms, including
   14.2028 ms swap and only 2.9731 ms main-thread CPU. Uncapped 1440p has a
   12.0706 ms frame with 10.4560 ms swap and 1.7648 ms CPU. GPU/compositor
   backpressure or scheduling is an inference; these timings do not separate
   the driver, WindowServer and OS wait causes.
2. **Cocoa/SDL event pumping:** three frames over 6 ms spend more than half in
   pump/peep/poll. The 1440p capped maximum is 7.3601 ms overall, 4.7576 ms
   polling; the 1080p capped event is 7.3007/4.8321 ms. The observer includes
   pumping and peeping, not just `SDL_PollEvent`.
3. **OpenGL clear:** one 4K capped frame is 7.7317 ms, with 4.7164 ms in clear,
   0.4546 ms swap and 2.8587 ms CPU. This is another measured driver-call
   stall, not evidence for resizing tessellation buffers.

No >6 ms frame spends 80% of its interval in main-thread CPU. None is
dominated by the measured blit or fence calls. Warmed texture/program/buffer
creation counters are zero. These facts narrow the hitch locations; they do
not prove every possible hitch absent, and background user activity remains
a confounder. `output/ws18/bench/summary.json` contains phase maxima and worst
frames, derived from the raw `frames.csv` files. No renderer, pacing or shader
optimization was made in this workstream.

## Merge notes

Focused commits, all based on `7dbfcea`:

| Commit | Scope |
| --- | --- |
| `36ddf15` | Native stencil-clear producer argument order; legacy branch retained. |
| `697087f` | Manifest verification, one-time extraction, fallback and cache tests. |
| `b0d0a12` | Private native Cocoa setup header, bundle tools/icon, arm64 CoD2x-only CMake app target. |
| `744e740` | Two-minute combat benchmark observer/driver tools; no engine optimization. |
| `a1fe17a` | Keep client observers out of the Python setup child. |
| `cc3f013` | Connect-path URLs and early first-launch URL queue. |
| `142c2c7` | Restore Cocoa URL handling after SDL initialization; progress-window setup. |
| `beb8325` | Native integration fixtures for current input and prepared app paths. |
| `6ca9d98` | Disable timedemo file logging. |
| `676039a` | Forward native shader diagnostics/program choices in the app benchmark tool. |
| `e8dadc4` | Prevent Python bytecode writes into signed Resources; verify sealing after runtime setup. |

No `src/headers/*` or top-level `CMakeLists.txt` changes. The only build-file
change is in native `cmake/macos-arm64.cmake`; it explicitly finds the Python
interpreter before defining `cod2_macos_app`. An earlier version omitted that
find and CMake silently dropped the empty command; the committed version
actually creates and verifies the app. Rebuild the bundle after merging;
never merge or distribute the external shaders, game data, demos or CD key.

`r_scene.c` is the only renderer behavior change. `macos_display.c` adds only
a guarded post-SDL URL-handler reinstall. Keep both when resolving nearby
changes; no common engine structs or tess buffer sizes changed. The private
Cocoa setup header is included only in the Apple arm64 CoD2x client branch.
The branch is ready for orchestration merge, with the acceptance gaps above
preserved rather than presented as verified Game Mode or steady 333 FPS.
