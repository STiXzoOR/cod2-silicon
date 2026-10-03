# WS17 — selected-resolution fullscreen and the 333 FPS cap

Branch: `port/fullscreen-333`. Base: `7a94368`. Worktree:
`/Users/stix/Projects/cod2-native-wt/fullscreen-333`. Host: Apple M6
(`sysctl -n machdep.cpu.brand_string`). Target display: PA27JCV, 6016×3384
physical pixels, 3008×1692 logical pixels, 60 Hz, as verified in WS13.

**Constant 333 FPS is not achieved. No tested resolution is certified to hold
approximately 330 FPS 1% lows.** Selected-resolution GL backing, both fullscreen
choices, six render resolutions, precise cap waits, a launcher and a signed
local app are delivered. Game Mode eligibility is recognized, but activation
and its performance benefit remain unverified. Display restoration passes
focused tests but is intermittent on this shared desktop. These are partial
results for integration, not acceptance of the whole performance objective.

## Changes and merge boundaries

| Commit | Result |
| --- | --- |
| `641df09` | Fix the CGL surface backing to the selected render size; exact SDL exclusive modes, mandatory resolution choices, native CGL stub removal and real default-buffer tests. |
| `ed2c987` | Stop exiting fullscreen on every renderer/map reset. |
| `66cc949` | Native main-thread interactive QoS and Mach deadline waits inside existing cap slack; preserve frame arithmetic. |
| `befd289` | Set the Cocoa Spaces hint before video initialization, retain fullscreen flags, correct fallback focus policy and disable the surface override on return to windowed mode. |
| `8d6cbfb` | Use SDK curl prototypes on native macOS so the CoD2x bundle's enabled download feature passes the ABI audit. |
| `2f9a0f9` | Bundle launch arguments, Game Mode keys, fullscreen launcher and usage notes. |
| `05170dd`, `b00bc17` | CGL/physical-display/CPU/API trace data, LaunchServices captures, a signal compatible with CoD2x and observer ownership checks. |
| `ba147b7` | Correct the compressed-subimage symbol and cover the native volume-texture upload calls in the observer. |

There are no changes to top-level `CMakeLists.txt`, `src/headers/*`, lighting,
sky, shader translation, network protocols or the VM. The shared engine hunks are
small native guards in `common.c`, `win_shared.c`, `linux_common.c` and
`dl_main.c`. Retain both the main-thread QoS call and the bundle argument prefix
when merging `linux_common.c`. The curl include is needed with `COD2_CODX=1`;
do not replace the ABI baseline to hide those findings. The CGL stub exclusion
is necessary: the old linked stub returned null and shadowed Apple's real
`CGLGetCurrentContext`, even though the isolated platform test worked.

## Fullscreen behavior

`r_fullscreen 1; r_borderless 0` selects an exact SDL display mode at the
requested dimensions/refresh, then enters exclusive fullscreen. The build
uses the installed SDL2 compatibility layer over SDL3. `r_borderless 1`
selects desktop fullscreen. Fullscreen windows are created without HiDPI and
use `kCGLCPSurfaceBackingSize` plus `kCGLCESurfaceBackingSize`; the renderer's
presentation and inverse mouse mapping use that real backing size. The
compositor scales the fixed surface to the desktop window. Apple documents
this [fixed-backing scaling mechanism](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_contexts/opengl_contexts.html).

The test clears the default GL back buffer and reads its last pixel, in
addition to querying CGL. This verifies actual pixel storage, rather than
relabeling SDL's view-size result. All 24 corrected matrix captures showed
fullscreen bits and a CGL backing exactly equal to the requested render size
at capture start and in the periodic geometry checks.
SDL's drawable query can still describe view bounds and is reported separately.

The modes include `1920x1080`, `2560x1440`, `3008x1692`, `3840x2160`,
`5120x2880`, `6016x3384`, and additional SDL display modes. No exact
6016×3384 SDL display mode was exposed on this host; an exclusive request
logs the reason and falls back to desktop fullscreen with a genuine 6K GL
surface. It does not pretend that a nearby display mode is an exact match.

Exclusive focus loss requests minimization so SDL can restore the desktop;
desktop fullscreen retains its desktop mode. Release exits fullscreen before
destroying the GL context. Tests cover explicit minimize/restore, resolution
changes, return to windowed mode, CGL override removal and display release.
Actual keyboard Cmd-Tab and the menu-bar Game Mode indicator were not verified:
the UI tool returned `cgWindowNotFound (-10005)` for the owned app.
This was repeated after a completed active-map capture with the process held
open: owned PID 24427 was confirmed alive before and after both bundle-ID and
exact-app-path lookups. The UI service recognized the bundle as running but
could not bind its window. `app-ui-held2/` and `game-mode-live-ui-system.log`
retain that run's evidence; the process was then released and only its PID
was killed. The error's underlying cause was not established.

Cocoa reads its fullscreen-Spaces policy at SDL video initialization, which
mode enumeration triggers before window creation. Setting it only at window
creation was too late. The native default now sets Spaces off at initialization;
[SDL's Cocoa implementation](https://raw.githubusercontent.com/libsdl-org/SDL/main/src/video/cocoa/SDL_cocoavideo.m)
confirms where it caches that policy. The environment can opt into Spaces for
further experiments. With Spaces enabled, this client returned success from
SDL's fullscreen call but later showed flags `0x6` (windowed). That experiment
is excluded from fullscreen acceptance. The working default prioritizes a
completed fullscreen transition; it does not establish active Game Mode.

## Measurement method and limits

Licensed data at `~/Games/CoD2` was read-only. Binaries, profiles, JPEGs,
configs, FIFOs and raw traces remain under ignored `output/ws17/`; none are
committed. All captures join a local Toujane deathmatch player, select an
Enfield and use WS13's street-view setup. Requested eye position is
`(-299,1001,121)`, yaw 90; the engine reports `(-299,1001,88) : 45`.
The corrected 1080p JPEG was visually checked: street, tank, buildings and HUD.
That render-buffer JPEG does not verify screen focus or Game Mode.

The before matrix uses the saved Release `7a94368` binary with CoD2x features
off, five-second warmed captures. The corrected matrix uses Release with
`COD2_CODX=1`, ten-second captures after five seconds of warmup: 75,579 frame
intervals in total. Its binary snapshot predates only final fallback bookkeeping,
windowed override cleanup and the SDK curl include; separate final-binary
6K, app and launcher checks follow. The intervening `after-matrix.json` used
a too-late Spaces hint and included windowed borderless requests; it is
diagnostic evidence only. Four before rows were rerun after an observer compile
error; their replacement results are in `before-matrix.json`.

The measurements are serial within this workstream. Our main matrices did
not overlap our builds/tests; the later eligibility control/Spaces attempts
overlapped our ABI/legacy checks and are not a performance A/B. Other clients
were observed in the process inventory, and the global desktop changed
between 6K and 800×600. No other agent's files were read and no other process
was stopped. Initial physical display dimensions are recorded, but are not
sampled every frame. Neither focus nor exclusive ownership of the desktop
is established for the whole capture. Several SDL flags lack input focus.

Consequently these tables are observations, **not controlled before/after
speedup claims**. Feature configuration, capture duration, other GPU work,
focus and desktop changes differ. A quiet, single-client, foreground acceptance
rerun is required. The global state is outside this workstream's authorized
process ownership; another agent's run was not interrupted to manufacture an
isolated result.

FPS = 1000 / mean swap-to-swap interval. The 1% low is 1000 / mean of the
slowest ceil(N/100) intervals; P99 uses nearest rank. Intervals use the Mach
clock. CPU time uses `CLOCK_THREAD_CPUTIME_ID`. The first timestamp-only
sample is discarded; CSV writes and screenshots happen after capture.
`swap_ms` is the preceding swap call's wall time, aligned with the current
interval. Instrumentation covers CPU API durations, not GPU execution.

## Before/after tables

Cap `0` means uncapped; vsync is off throughout. Every requested fullscreen
mode, resolution and cap is included. `N/A` means the before build rejected
6K and rendered 640×480 instead; those fallback FPS values are not 6K results.
A dagger marks a corrected run whose initial physical mode does not support
the requested exclusive/6K-desktop comparison. A double dagger marks a
windowed before request, retained only as diagnostic data. All corrected
render backings still matched the requested resolution.

### Exclusive requested

| Render size | Cap | Before FPS | Before 1% low | After FPS | After 1% low | After P99 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 333 | 249.3 | 74.6 | 309.3 | 55.2 | 7.539 |
| 1920x1080 | 0 | 395.6 | 148.5 | 475.6 | 143.4 | 5.002 |
| 2560x1440 | 333 | 294.6 | 93.7 | 305.1 | 42.4 | 7.089 |
| 2560x1440 | 0 | 344.4 | 94.6 | 347.3 | 124.5 | 6.010 |
| 3008x1692 | 333 | 280.9 | 88.9 | 302.5 | 119.6 | 6.138 |
| 3008x1692 | 0 | 292.4 | 88.8 | 349.5 | 174.5 | 5.058 |
| 3840x2160 | 333 | 263.8 | 90.6 | 270.3 | 144.5 | 6.187 |
| 3840x2160 † | 0 | 268.6 | 89.8 | 385.5 | 135.4 | 6.533 |
| 5120x2880 | 333 | 202.3 | 77.2 | 307.5 | 146.3 | 6.096 |
| 5120x2880 | 0 | 222.1 | 79.2 | 313.8 | 131.2 | 6.812 |
| 6016x3384 | 333 | N/A | N/A | 230.2 | 90.4 | 9.171 |
| 6016x3384 | 0 | N/A | N/A | 203.4 | 75.6 | 12.341 |

### Desktop fullscreen requested

| Render size | Cap | Before FPS | Before 1% low | After FPS | After 1% low | After P99 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 333 | 196.1 | 74.1 | 310.1 | 110.3 | 7.930 |
| 1920x1080 | 0 | 194.1 | 78.8 | 475.1 | 102.9 | 8.189 |
| 2560x1440 | 333 | 190.7 | 77.1 | 318.2 | 116.3 | 7.409 |
| 2560x1440 | 0 | 187.5 | 79.7 | 396.2 | 108.3 | 8.187 |
| 3008x1692 † | 333 | 182.7 | 78.7 | 325.5 | 205.6 | 4.421 |
| 3008x1692 † | 0 | 183.8 | 73.5 | 343.7 | 188.4 | 4.642 |
| 3840x2160 † | 333 | 171.7 | 69.8 | 286.0 | 173.7 | 5.330 |
| 3840x2160 † | 0 | 179.1 | 72.3 | 270.8 | 32.1 | 6.496 |
| 5120x2880 | 333 | 205.3 | 91.8 | 271.4 | 94.9 | 9.767 |
| 5120x2880 ‡ | 0 | 144.4 | 57.4 | 293.6 | 105.1 | 8.415 |
| 6016x3384 | 333 | N/A | N/A | 232.7 | 97.4 | 8.942 |
| 6016x3384 † | 0 | N/A | N/A | 233.5 | 136.8 | 6.713 |

The before 6K exclusive fallback measured 302.6/380.0 FPS (333/uncapped),
but its viewport was 640×480 and drawable 800×600. The before 6K desktop
fallback measured 316.7/282.3 with a 640×480 viewport and 1280×960 drawable,
flags `0x2006`. Those results are explicitly excluded as unsupported 6K.

### Actual backing and initial physical display

Within each cell, sizes are `cap 333 / uncapped`. Corrected CGL backing equals
the render size in every row. `0x7` is exclusive fullscreen; `0x1007` includes
desktop-fullscreen bits. Before desktop 5K uncapped was actually windowed
(`0x2006`); other before modes also had transition/focus changes.

| Requested mode | Render size | Before drawable | After CGL backing | After physical display | After flags |
| --- | --- | --- | --- | --- | --- |
| fullscreen | 1920x1080 | 3840x2160 / 1920x1080 | 1920x1080 / 1920x1080 | 1920x1080 / 1920x1080 | 0x7 / 0x7 |
| fullscreen | 2560x1440 | 2560x1440 / 2560x1440 | 2560x1440 / 2560x1440 | 2560x1440 / 2560x1440 | 0x7 / 0x7 |
| fullscreen | 3008x1692 | 3008x1692 / 3008x1692 | 3008x1692 / 3008x1692 | 3008x1692 / 3008x1692 | 0x7 / 0x7 |
| fullscreen | 3840x2160 | 3840x2160 / 3840x2160 | 3840x2160 / 3840x2160 | 3840x2160 / 6016x3384 | 0x7 / 0x7 |
| fullscreen | 5120x2880 | 5120x2880 / 5120x2880 | 5120x2880 / 5120x2880 | 5120x2880 / 5120x2880 | 0x7 / 0x7 |
| fullscreen | 6016x3384 | 800x600 / 800x600 | 6016x3384 / 6016x3384 | 6016x3384 / 6016x3384 | 0x1007 / 0x1007 |
| borderless | 1920x1080 | 6016x3384 / 6016x3384 | 1920x1080 / 1920x1080 | 6016x3384 / 6016x3384 | 0x1007 / 0x1007 |
| borderless | 2560x1440 | 6016x3384 / 6016x3384 | 2560x1440 / 2560x1440 | 6016x3384 / 6016x3384 | 0x1007 / 0x1007 |
| borderless | 3008x1692 | 6016x3384 / 6016x3384 | 3008x1692 / 3008x1692 | 800x600 / 800x600 | 0x1007 / 0x1007 |
| borderless | 3840x2160 | 6016x3384 / 6016x3384 | 3840x2160 / 3840x2160 | 800x600 / 800x600 | 0x1007 / 0x1007 |
| borderless | 5120x2880 | 6016x3384 / 10240x3168 | 5120x2880 / 5120x2880 | 6016x3384 / 6016x3384 | 0x1007 / 0x1007 |
| borderless | 6016x3384 | 1280x960 / 1280x960 | 6016x3384 / 6016x3384 | 6016x3384 / 800x600 | 0x1007 / 0x1007 |

No resolution meets the 330 FPS 1% low criterion. The launcher and bundle
retain exclusive 1080p as the provisional default: corrected uncapped 1080p
was essentially tied (475.6 exclusive vs 475.1 desktop), with the exclusive
1% low higher (143.4 vs 102.9). Exclusive also had more headroom at 5K.
Desktop was faster in the 1440p observation. This is not evidence of a universal
winner; repeat a matched A/B on the intended 6K desktop before changing the
default globally. `r_borderless 0` was already the engine default; no renderer
dvar or shared renderer header was changed just to restate it.

Final-binary retests of the 6K exclusive request confirm the desktop fallback
after the final bookkeeping change. Both used a 6016×3384 CGL surface,
6016×3384 initial physical display, flags `0x1007`, and no recorded geometry
changes. The lower FPS illustrates the need for an isolated acceptance run.

| Render size / actual mode | Cap | FPS | 1% low | P99 ms | Thread CPU ms/frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| 6016x3384 / desktop fallback | 333 | 185.8 | 82.7 | 10.409 | 4.724 |
| 6016x3384 / desktop fallback | 0 | 209.4 | 92.7 | 9.041 | 3.930 |

Evidence: `final-fallback-6k/results.json` and
`final-fallback-6k-uncapped/results.json` under `output/ws17/`.

## Hitch evidence and pacing decisions

The main/render thread explicitly requests `QOS_CLASS_USER_INTERACTIVE`.
The probe observed `0x21` before the change too; no improvement or performance-core
pinning is claimed. QoS measurement does not establish that the thread never
runs on efficiency cores. UDP reception is synchronous through `Sys_GetEvent`
and `NET_GetPacket`, on the event/main thread. Audio mixes in CoreAudio's
AudioUnit callback and uses `pthread_mutex_trylock`; the live audio callback's
QoS/core residency was not measured, and its scheduling policy was not
changed. Generic CThread workers and the one-second CoD2x freeze watchdog
were inspected; they were not retagged without evidence of a frame-critical
wait.

The cap's existing integer arithmetic and `Sys_Milliseconds` clock remain
unchanged. Native client cap slack now waits at most 500 µs per event-loop
iteration using `mach_wait_until`, then spins for the final approximately
80 µs. Raw mouse/network processing still occurs between bounded waits.
Dedicated and uncapped paths retain their original sleep behavior. A focused
pre-bundle 1080p observation reduced mean main-thread CPU from 2.841 to
2.242 ms/frame (about 21%); its 1% low worsened from 198.4 to 125.6, so that
is an observed CPU-saving result, not a latency or FPS win.

Selected slow frames from the corrected capped exclusive captures:

| Render size | Interval ms | Thread CPU ms | Swap wall ms | SDL poll wall ms | Blit wall ms | Fence test wall ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 211.373 | 10.658 | 29.674 | 177.103 | 0.194 | 0.002 |
| 1920x1080 | 86.718 | 5.541 | 66.117 | 16.006 | 0.129 | 0.003 |
| 2560x1440 | 446.050 | 8.373 | 396.451 | 44.531 | 0.123 | 0.004 |
| 2560x1440 | 41.024 | 4.984 | 9.114 | 28.231 | 0.131 | 0.002 |
| 3840x2160 | 9.891 | 2.586 | 7.431 | 0.016 | 0.049 | 0.001 |
| 3840x2160 | 8.480 | 3.082 | 5.561 | 0.032 | 0.052 | <0.001 |

The largest stalls are late in the trace: the 211.373 ms 1080p frame is data
row 1351 and the 446.050 ms 1440p frame is row 2553 (zero-based). They are
not the initial interval containing the observer's metadata logging.

Long intervals are mostly outside main-thread CPU execution, inside swap or
SDL/Cocoa polling, or unclassified off-CPU time. These traces identify the
stalled calls; they do not distinguish GPU/driver/compositor waits from OS
descheduling, and they do not prove which other process caused a stall.
Mean main-thread CPU at capped exclusive 4K was 3.450 ms, already larger
than the 3 ms frame budget in that observation. At 6K the desktop scaled-blit
API averaged 1.671 ms in the corrected capped desktop run. Reducing backing
size removes the Retina overdraw, but does not remove all remaining waits or
render work.

Every corrected matrix capture counted zero calls to the instrumented
`glTexImage2D`, `glTexSubImage2D`, `glCompressedTexImage2DARB`,
`glProgramStringARB` and `glBufferDataARB`. Those probes missed the core
compressed-subimage symbol and 3D texture calls. Final review corrected
that coverage: the probe now wraps `glCompressedTexSubImage2D`,
`glTexImage3D`, `glTexSubImage3D` and `glCompressedTexImage3DARB` too.
The native renderer calls its texture/program functions directly; native
buffer allocation does not use `glBufferData` (the web backend does).
The final `final-upload-coverage` 1080p capture counted zero uploads,
program uploads and buffer reallocations with the corrected probe. It
measured **333.2 FPS average, 294.7 FPS 1% low, P99 3.254 ms**, still below
the requested 1% low. This does not rule out first-use creation while loading,
CPU allocation elsewhere, driver-internal allocation or other maps/effects.
No speculative shader prewarm, texture cache or renderer
allocation change was made amid WS15's shader/lighting work. Fence-test
aggregate time in the sampled hitches is tiny; `r_gpuSync` stays `adaptive`.
`logfile 0` and `developer 0` are launch defaults; benchmark developer output
is disabled before capture. The 333 physics fixture still produces the exact
3 ms sequence; live engine intervals are 3 ms in 93.44%, 93.84% and 46.52%
of capped exclusive 1080p, 1440p and 4K frames respectively, with longer
intervals on hitches. A physics quantum is not a presentation-time guarantee.
An ignored, temporary observer prototype also paced presentation deadlines
at 3 ms, using Mach waits and an 80 µs final spin. It tested whether capping
frame starts leaves avoidable render-end jitter; no engine arithmetic or
source was changed. The following control returned to the passive observer.
Both fifteen-second captures retained exclusive 1080p backing and a matching
1920×1080 physical mode. Other clients were still running, so these are
consecutive observations rather than an isolated causal comparison.

| 1080p experiment | FPS | 1% low | P99 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
| Temporary presentation pacer | 325.4 | 122.3 | 6.185 | 11.758 |
| Following passive control | 332.6 | 222.8 | 4.004 | 7.497 |

The prototype did not meet the target and was not added to the engine.
Evidence is in `present-pacing-1080/` and `present-pacing-control-1080/`.
Remaining late off-CPU stalls make a presentation-only pacing change
insufficient under these conditions.

The existing packet/UI trace files are bounded to the first 12–40 messages
or refreshes and still occur during startup independently of `logfile`.
They were observed in the default-settings check, moved into its ignored
output directory, and were not changed across WS14's network hunks. Their
bounded startup logging does not account for the warmed capture's late stalls.

## App bundle and Game Mode

Local deliverable: `output/ws17/CoD2x Native.app`, an arm64 executable in a
proper app layout with an ad-hoc code signature. The builder is based on WS10
and requires `COD2_CODX=1` plus the new bundle-argument symbol. Its plist sets
`LSApplicationCategoryType=public.app-category.action-games`,
`LSSupportsGameMode=true`, `NSHighResolutionCapable=false`, a direct executable
and the existing cod2x URL registration. No icon was available in this repo
(`rg --files` for PNG/ICO/ICNS returned none); no asset was fetched or invented.
Game data stays outside the bundle. Normal app writes use the existing
`~/Library/Application Support/CoD2-native` home; tests explicitly choose
worktree-owned homes. Caller arguments override bundle defaults.

`plutil -lint` and `codesign --verify --deep --strict` pass. A LaunchServices
capture (`app-final-game-mode`) retained exclusive 1920×1080 backing and
measured 330.0 average FPS, 190.3 1% low, P99 5.163 ms. Game Policy recognized
the owned bundle PID 67864 with `labelReason=LSSupportsGameMode (Info.plist)`.
It logged Game Mode as **paused**, then available, and off/unavailable when
the owned app ended. Those states do not prove active Game Mode. Apple's
[plist key](https://developer.apple.com/documentation/bundleresources/information-property-list/lssupportsgamemode)
and [Game Mode presentation](https://developer.apple.com/videos/play/wwdc2025/209/)
describe eligibility; declaring the key alone is not activation evidence.

A separately signed `--no-game-mode` control measured 328.2/176.3 FPS/1% low,
but no active ON state was established and the control overlapped the ABI
check. **No Game Mode speedup is claimed.** The normal bundle Spaces attempt
(`app-final-spaces`) also logged paused/available/unavailable and ended with
windowed flags `0x6`. The menu-bar indicator could not be checked through the
available UI surface. Foreground fullscreen and
an isolated ON/OFF capture remain required acceptance work.
The further `spaces-sync` experiment enabled both fullscreen Spaces and
`SDL_VIDEO_SYNC_WINDOW_OPERATIONS=1`, which SDL documents as a
[window-operation completion barrier](https://wiki.libsdl.org/SDL3/SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS).
It also ended with windowed flags `0x6`; waiting for the asynchronous
operation alone did not resolve this transition.

The observer now uses SIGUSR1. SIGUSR2 belongs to the CoD2x freeze diagnostic;
an early tool attempt triggered that handler instead of the observer and timed out,
which is excluded as a probe error, not a real freeze. The tool uses an owned
FIFO and constructor-written PID file for LaunchServices, kills only that
exact app PID, and rejects uninstrumented launches before signaling them.
The shell launcher is not a valid `--binary` for DYLD profiling: the system
shell can strip DYLD variables; profile the native executable instead.
Live captures still end by killing the owned process because the inherited
listen-server quit path can hang. Clean app quit and restoration after forced
termination are not established by those captures; the explicit platform
release test is the clean teardown evidence.

## Build, verification and reproduction

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos --target cod2_macos -j8
python3 tests/fixes13/run.py --build build-macos
python3 tests/fixes13/legacy.py --base 7a94368
sh tools/abi/check.sh build-macos/compile_commands.json output/ws17/abi-final

cmake -S tests/platform -B output/ws17/platform-tests \
  -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build output/ws17/platform-tests -j8
ctest --test-dir output/ws17/platform-tests --output-on-failure
ctest --test-dir output/ws17/platform-tests -R fixed_ --output-on-failure

python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --window-mode fullscreen --resolution 1920x1080 --maxfps 333 \
  --seconds 10 --output output/ws17/new-exclusive-1080
# Repeat for borderless, all six resolutions, and --maxfps 0.
# Use a NEW output directory for each capture.

python3 tools/cod2x/make_macos_app.py build-macos/cod2_macos \
  'output/CoD2x Native.app' --game-dir "$HOME/Games/CoD2"
plutil -lint 'output/CoD2x Native.app/Contents/Info.plist'
codesign --verify --deep --strict 'output/CoD2x Native.app'
python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --app 'output/CoD2x Native.app' --window-mode fullscreen \
  --resolution 1920x1080 --seconds 10 --output output/ws17/new-app-capture
/usr/bin/log show --last 4m --style compact \
  --predicate 'process == "gamepolicyd" OR process == "GamePolicyAgent"'
```

Release build passes. `tests/fixes13` passes all fixtures, including 1,000
frames each at 125→8 ms, 250→4 ms, 333→3 ms, and limited-333→4 ms. Packet
spacing remains 8, 8, 9 and 8 ms for those cases respectively. The Mach-wait
test performs 50 bounded real waits,
never returns before the deadline, and accepts an already-passed deadline.

The native platform suite passed 24/24 earlier. After strengthening the
fullscreen/restore assertions, both focused tests passed. The final complete
rerun passed **23/24**: the exclusive test expected the original 3008×1692
logical/6016×3384 physical desktop at minimize, but observed 800×600. An
immediate focused rerun passed in 11.91 seconds. This global-state failure
is retained in `platform-final.log`; it is not suppressed or reclassified as
a pass. Restore behavior on a quiet 6K desktop remains unaccepted. Comparison
uses mode dimensions, pixel dimensions and refresh rather than opaque mode
IDs, which can identify equivalent modes differently.

ABI audit: **636 translation units, 0 errors, 0 mismatches, 0 new**;
**218 renderer bindings, 0 table/named-cast mismatches, 0 unprototyped floating
calls**. Import audit: **514 symbols, 0 proven extra/missing dereferences**,
185 existing placeholders. The initial CoD2x audit found seven SDK-vs-handwritten
curl declaration mismatches; the native SDK include removed them without
baseline edits. Evidence: `abi-final.log` and `abi-final/`.

Legacy inactive-source parity: **10 changed source files, 10 legacy
configurations, 0 mismatches** against `7a94368`. All production changes are
native guarded; no legacy build flags or targets changed. This host cannot
produce a fresh historical i386 binary, so source/preprocessor parity is the
verified result. A minimal `xcrun clang -arch i386` link probe failed with
`linking for i386 is no longer supported` (`i386-sdk-check.log`). The
orchestrator/legacy CI must still perform a byte-for-byte i386 binary comparison
using a compatible linker. No system packages or additional toolchains were
installed; native build/test CLI tools were present. Python syntax, shell
syntax and `git diff --check` pass.

## User settings and remaining acceptance work

One command after the native build:

```sh
tools/macos/fullscreen.sh
# Local bundle already delivered in this worktree:
open 'output/ws17/CoD2x Native.app'
# Other render sizes and desktop fullscreen:
COD2_RESOLUTION=3840x2160 tools/macos/fullscreen.sh
COD2_RESOLUTION=5120x2880 tools/macos/fullscreen.sh
COD2_RESOLUTION=6016x3384 COD2_BORDERLESS=1 tools/macos/fullscreen.sh
```

Recommended starting settings are 1920×1080 exclusive, `com_maxfps 333`,
`r_swapInterval 0`, `in_rawmouse 1`, `m_filter 0`, `cl_mouseAccel 0`,
`logfile 0`, `developer 0`. Tearing is allowed. 4K/5K/6K are selectable,
but none is certified for constant 333. The launcher uses ignored
`output/fullscreen/home`; `COD2_BINARY`, `COD2_DATA_DIR`, `COD2_RUN_DIR`,
`COD2_RESOLUTION` and `COD2_BORDERLESS` override its defaults. Extra engine
arguments come last. A direct launcher startup check reached an active local
map and printed all nine expected setting values (`launcher-settings3.log`);
it enabled developer output only for that check. A separate direct launch of
the executable inside the signed app, without resolution/fullscreen/cap/input
arguments, also printed those nine values and reached the active map
(`bundle-settings.log`). This verifies the plist defaults are consumed;
the test supplied only an isolated home, developer output and the local map.

The orchestrator can merge these focused commits without any renderer
lighting/sky changes. Acceptance remains blocked by shared display/focus
state, unresolved Cocoa Space completion/Game Mode activation, and remaining
main-thread/driver stalls. After integration with WS15, repeat all 24 captures
with one foreground client on the intended 6K desktop, check display restore
on Cmd-Tab/exit, confirm active Game Mode in the menu and logs, then measure
ON/OFF. If 4K still exceeds the frame budget, profile CPU and driver/GPU work
on that isolated run before choosing further renderer changes. No branch was
pushed, no PR/issue was opened, and no remote was changed.
