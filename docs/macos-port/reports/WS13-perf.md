# WS13 — native performance and frame pacing

Branch: `port/perf`. Machine: Apple M6, 12 cores, 24 GiB, macOS 27.0.1.
Display verified with `system_profiler`: PA27JCV, 6016×3384 physical pixels,
3008×1692 logical pixels, 60 Hz. Worktree: `cod2-native-wt/perf`.

**The stable 333 FPS acceptance criterion is not established.** The native
client runs Release, records/replays Toujane, and has substantially more CPU
headroom after the changes below. High-resolution live measurements still
have hitches; average FPS and an integer 3 ms counter do not prove a 333 FPS
1% low. Final tables and remaining blockers follow.

## Changes to merge

1. `9fda653`: native `G_FindItem` uses `sizeof(gitem_t)` instead of the legacy
   44-byte stride. The optimized Release map launch otherwise crashes in
   `RegisterItem` after pointer subtraction on a misaligned item pointer.
   Legacy expressions remain in the inactive branch.
2. `ed89484`: native mantle transition data uses 32-bit words. Darwin's
   `UInt32` here is an unsigned long, so the table was 192 rather than 96 bytes;
   demo initialization reported a mantle height of 0 instead of 57.
3. `1482025`, `09690e8`: native `r_fullscreen` becomes archived/latched instead
   of read-only, registration retains the selected fullscreen state, and
   window sizing obtains the Retina pixel ratio before the GL drawable has
   settled. Previously a requested 1920×1080 window presented to 3840×2160.
   Now its drawable is 1920×1080 and its logical window is 960×540.
4. `863e917`: native input explicitly pumps SDL at most once per SDL
   millisecond, then drains queued events with nonblocking `SDL_PeepEvents`.
   The cap loop calls `Com_EventLoop` repeatedly; each `SDL_PollEvent` used to
   pump Cocoa again. An exploratory cap run counted about 144 polls/frame.
   The changed path measured about 1–3 pumps/frame. Keyboard/window input
   gains at most 1 ms of pump latency; raw mouse reads and network handling
   still run on every event-loop iteration. No frame quantum changes.
5. `be69634`: three per-packet native loopback diagnostics use the existing
   `COD2_DEBUG_ONLY` gate. Legacy debug output expands to the original tokens.
6. `0dae8b0`, `bff5d0b`: native RGBA arrays use the existing direct/interleaved
   GL color pointer. ARGB/BGRA conversion finds the actual unsigned-short
   indexed span and converts only that span, retaining scratch offsets so
   GL indices still select the same color bytes. A temporary validation run
   checked 750,000 draws: zero indices outside declared bounds; declared
   spans totaled 9,180,160,002 vertices, actual spans 2,089,627,397 (22.8%).
   That instrumentation was removed before the final build.
7. `1b6cbb6`, `65bf5b4`: the timedemo tool accepts resolution and display
   mode before startup. The optional DYLD live observer captures mach-clock
   frame intervals and input/clear/blit/fence/swap CPU durations, with a
   separate main-thread CPU sampler for profiler-attachment failures.
   The follow-up observer also records geometry/focus changes once per
   second and queries the selected renderer dvars before capture.

No top-level CMake or `src/headers/*` edits. Renderer merge hunks are small:
`r_dvars.c` fullscreen flags, the non-D3D9 registration block in `r_init.c`,
and the private color helper plus its two callers in `CDirect3DDevice.c`.
WS15 should retain its own shader/parity work around these changes. WS16
owns the VM blockers described below. No network protocol changes for WS14.

## Build and regression evidence

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos -j12 --target cod2_macos
python3 tests/perf/run.py

cmake -S . -B output/ws13/test-build -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release
python3 tests/fixes13/run.py --build output/ws13/test-build --test timing
python3 tests/fixes13/legacy.py --base 4ddccfd

cmake -S tests/platform -B output/ws13/platform-tests \
  -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build output/ws13/platform-tests -j12
ctest --test-dir output/ws13/platform-tests --output-on-failure
sh tools/abi/check.sh build-macos/compile_commands.json output/ws13/abi-final
```

Release compile commands contain `-O3`, `-ffp-contract=off` and
`-fno-strict-aliasing`, with no trailing `-O0`. The native item, mantle and
color fixtures pass at `-O3` under ASan/UBSan. Color tests include interleaved
padding, RGBA/ARGB/BGRA bytes, repeated/out-of-order indices, nonzero indexed
spans, and untouched scratch prefixes/suffixes. The platform suite passes
21/21 tests (39.85 s). The input test verifies that repeated calls in one SDL
millisecond drain events without another pump.

`tests/fixes13` passes 1,000 frames each: 125→8 ms, 250→4 ms, 333→3 ms;
the CoD2x limited-333 case stays 4 ms. Packet cadence stays 8/8/9/8 ms.
The existing frame arithmetic, `com_frameTime`, physics and `Sys_Milliseconds`
were not changed. Native `Sys_Milliseconds` ultimately uses
`MacSystem_Nanoseconds`, backed by `mach_absolute_time` and its timebase.

The inactive-source check reports **8 changed source files, 10 legacy
configurations, 0 mismatches**. Native-only platform source is also guarded
at the changed call. This checks source/preprocessor parity; this Mac cannot
build or execute the historical i386 binary, so fresh byte-identical i386
binary comparison remains an orchestrator/CI check. No legacy compile flags
or target configuration were changed.

ABI audit: **620 translation units, 0 errors, 0 mismatches, 0 new**;
**218 renderer bindings, 0 table/named-cast mismatches, 0 unprototyped
floating calls**. Import-slot audit: 514 symbols, 0 proven extra or missing
dereferences (185 existing placeholders). Logs are in
`output/ws13/abi-final.log` and `output/ws13/platform-final.txt`.

## Measurement method and limitations

Licensed data at `~/Games/CoD2` remained read-only. Every run has a
new home and output directory under ignored `output/ws13`. Demos, JPEGs,
profiles and binaries are local evidence only and are not committed.

Live runs join one local deathmatch player with an Enfield, position the player
at the stock BSP spawn `(-299,1001,61)`, and warm up for five seconds. The
command uses eye Z=121. The engine reports `viewpos (-299 1001 88) : 45`,
despite requested yaw=90. A JPEG confirms a real street/tank/buildings view
above ground. An earlier guessed camera `(0,0,180)` was under the map and is
excluded from the final gameplay tables. Its input measurements are labeled
exploratory and do not establish the acceptance goal.

The live observer samples immediately before each `SDL_GL_SwapWindow` using
`mach_absolute_time`; FPS is the reciprocal of the mean interval. The 1% low
is 1000 divided by the mean of the slowest ceil(N/100) intervals. P99 uses
nearest rank. CPU phases cover only the named calls, not complete engine
phases or GPU execution. The integer engine histogram is measured separately.
The first timestamp-only frame is discarded, and CSV writing and screenshot
readback happen after capture. All FPS comparisons disable the CPU sampler.

Runs are serial within this workstream, without our builds/tests running
during measurement. Other autonomous agents share the machine; another native
client was observed in the process inventory. Its worktree was not inspected
and its process was not touched. These measurements therefore do not establish
isolated GPU/CPU performance. Re-run the commands after integration with one
client and a quiet machine before making a stable-FPS claim.

Timedemo uses WS5's `bench.sh` and `cl_freezeDemo 1`, which advances demo time
in 50 ms steps and emits a 1 ms resolution CSV. It measures replay work,
not a live server or display scanout. Fullscreen requests that cannot change
the display mode fall back to desktop fullscreen; actual SDL flags/drawable
dimensions are recorded by the live probe.

## Before/after and final results

Live captures are 15 seconds each, with the final non-LTO Release binary.
The table records **requested** mode and initial actual presentation;
fullscreen/desktop rows are provisional because transitions and focus vary.
`0x2006` has neither fullscreen nor input-focus flags; `0x2007` has fullscreen;
`0x3007`/`0x3707`/`0x3407` have desktop-fullscreen bits. A later probe version
also logs presentation changes once per second.

| Render resolution | Requested mode | Cap | FPS | 1% low | P99 ms | Engine 3 ms % | Drawable | SDL flags |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1280x720 | windowed | 333 | 332.8 | 214.0 | 3.406 | 99.76 | 1280x720 | 0x2006 |
| 1280x720 | windowed | 0 | 646.4 | 292.0 | 2.045 | 0.28 | 1280x720 | 0x2006 |
| 1920x1080 | windowed | 333 | 331.8 | 175.0 | 3.506 | 99.52 | 1920x1080 | 0x2006 |
| 1920x1080 | windowed | 0 | 450.1 | 249.9 | 3.132 | 25.23 | 1920x1080 | 0x2006 |
| 2560x1440 | windowed | 333 | 332.5 | 200.6 | 3.945 | 99.74 | 2560x1440 | 0x2006 |
| 2560x1440 | windowed | 0 | 487.9 | 204.8 | 3.397 | 7.32 | 2560x1440 | 0x2006 |
| 1280x720 | fullscreen | 333 | 333.0 | 230.5 | 3.886 | 99.88 | 2560x1440 | 0x2006 |
| 1280x720 | fullscreen | 0 | 682.9 | 197.6 | 2.465 | 0.20 | 2560x1440 | 0x2006 |
| 1920x1080 | fullscreen | 333 | 331.7 | 174.0 | 4.533 | 99.22 | 3840x2160 | 0x2006 |
| 1920x1080 | fullscreen | 0 | 416.5 | 124.1 | 6.702 | 25.54 | 3840x2160 | 0x2006 |
| 2560x1440 | fullscreen | 333 | 267.7 | 92.0 | 9.947 | 59.94 | 5120x2880 | 0x2006 |
| 2560x1440 | fullscreen | 0 | 213.1 | 49.4 | 15.843 | 37.66 | 5120x2880 | 0x2007 |
| 1280x720 | borderless | 333 | 329.7 | 166.4 | 4.867 | 98.20 | 800x600 | 0x3007 |
| 1280x720 | borderless | 0 | 251.7 | 106.3 | 8.304 | 42.77 | 6016x3384 | 0x3707 |
| 1920x1080 | borderless | 333 | 230.4 | 82.4 | 10.868 | 37.15 | 6016x3384 | 0x3407 |
| 1920x1080 | borderless | 0 | 316.7 | 97.7 | 9.083 | 40.96 | 3840x2160 | 0x2006 |
| 2560x1440 | borderless | 333 | 302.4 | 115.2 | 7.289 | 84.62 | 5120x2880 | 0x2006 |
| 2560x1440 | borderless | 0 | 344.3 | 144.6 | 6.047 | 55.35 | 5120x2880 | 0x2006 |

Windowed 1080p cap: 4,953/4,977 engine intervals are 3 ms; the remaining
intervals include 10–12 ms hitches. Windowed 1440p cap: 4,975/4,988 are
3 ms, with 11–12 ms hitches. There are no engine 2/4 alternations at the
333 cap; it is overwhelmingly 3/3/3 with occasional longer intervals.
High-resolution presentation intervals still vary: p99 3.506/3.945 ms,
1% low 175.0/200.6 FPS. This fails the requested acceptance criterion.

At these capped windowed settings, average input CPU time is about 0.062 ms,
swap 0.144/0.185 ms, blit 0.058/0.063 ms, clear 0.059/0.066 ms,
and fence testing 0.00022/0.00041 ms (1080p/1440p). The Retina presentation
blit itself is small in these windowed runs; driving larger desktop drawables
can substantially reduce throughput. These are CPU call times, not GPU times.

Before/after experiments (same real street view for color/LTO/clear; input
row is explicitly the earlier exploratory view):

| Experiment | Before FPS | After FPS | Before 1% low | After 1% low | Decision |
| --- | ---: | ---: | ---: | ---: | --- |
| RGBA direct arrays, 1080p uncapped | 383.5 | 387.9 | 136.3 | 133.3 | Keep: removes identity copying |
| Indexed ARGB/BGRA spans, 1080p uncapped | 387.9 | 543.7 | 133.3 | 158.1 | Keep: +40.2% average, exact indexed bytes |
| Skip presentation clear, 1080p uncapped | 394.4 | 403.1 | 205.7 | 143.5 | Revert |
| ThinLTO, 1080p uncapped | 569.2 | 580.4 | 246.8 | 216.3 | Do not enable by default |

Exploratory input cost fell from about 0.46–0.63 ms/frame (repeated SDL
polls) to 0.031 ms/frame in the first cap comparison. Final real-view capped
input is about 0.062 ms/frame; the longer cap slack permits more pumps.
The original exploratory resolution matrix used the under-map view and
unverified display state; it is retained in `output/ws13/baseline-matrix.json`
for audit only and is excluded from gameplay acceptance.

The startup commit `4ddccfd` cannot supply a usable Release gameplay FPS
baseline because of the item-stride crash. The two native layout fixes were
necessary before recording/replay measurements. Preserved binaries under
`output/ws13/` include `baseline-client`, `before-clear-client`,
`before-range-client` and `final-no-lto-client`.

Timedemo replays the same **620-frame** street recording (619 timing
samples). Each invocation completes normally according to the engine
summary; the wrapper then stops its owned group. These short replays include
initial replay work and have only 1 ms timing precision. Modes are requested
settings; unlike the live observer, the timedemo tool does not independently
verify the actual display flags. Fullscreen/borderless results remain
provisional given the live transition failures.

| Render resolution | Requested mode | Cap | Timedemo FPS | 1% low (1 ms clock) | P99 ms | Max ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1280x720 | windowed | 333 | 330.1 | 166.7 | 4 | 9 |
| 1280x720 | windowed | 0 | 639.8 | 125.0 | 4 | 19 |
| 1920x1080 | windowed | 333 | 331.2 | 194.4 | 4 | 9 |
| 1920x1080 | windowed | 0 | 589.4 | 170.7 | 4 | 8 |
| 2560x1440 | windowed | 333 | 331.0 | 205.9 | 4 | 6 |
| 2560x1440 | windowed | 0 | 499.6 | 137.3 | 6 | 9 |
| 1280x720 | fullscreen | 333 | 333.3 | 291.7 | 3 | 6 |
| 1280x720 | fullscreen | 0 | 654.0 | 162.8 | 4 | 11 |
| 1920x1080 | fullscreen | 333 | 264.1 | 13.9 | 5 | 448 |
| 1920x1080 | fullscreen | 0 | 540.5 | 179.5 | 5 | 6 |
| 2560x1440 | fullscreen | 333 | 331.9 | 233.3 | 4 | 6 |
| 2560x1440 | fullscreen | 0 | 448.6 | 159.1 | 6 | 7 |
| 1280x720 | borderless | 333 | 282.5 | 22.7 | 6 | 231 |
| 1280x720 | borderless | 0 | 539.6 | 200.0 | 4 | 7 |
| 1920x1080 | borderless | 333 | 331.9 | 218.8 | 4 | 5 |
| 1920x1080 | borderless | 0 | 521.0 | 189.2 | 4 | 6 |
| 2560x1440 | borderless | 333 | 332.3 | 233.3 | 4 | 5 |
| 2560x1440 | borderless | 0 | 473.6 | 179.5 | 4 | 7 |

## Profiling and limiters

External `/usr/bin/sample` attachment did not finish, including a sleeping
Python control process. Native attach attempts timed out at 15 s and 180 s.
`xcrun xctrace record --template 'Time Profiler' --attach PID --time-limit 5s`
reached the recording limit but did not finish finalizing before its timeout;
the interrupted trace was not usable. No external profile is claimed valid.
No packages, security settings or system permissions were changed.

The optional main-thread signal/frame-pointer sampler produced local CPU
stacks, separately from FPS runs. It initially sampled color conversion and
Apple GL/Metal command submission heavily. After the RGBA path, symbolication
with `atos` located the remaining hot conversion in `DrawIndexedPrimitive`'s
ARGB/BGRA helper. This led to the indexed-span optimization. Signal delivery
and process CPU-time timers bias these samples; their percentages are
qualitative and must not be treated as GPU execution times.

The final five-second CPU sample collected 3,136 main-thread PCs: 2,330
landed in `iokit_user_client_trap` under GL/Metal submission, and 188 in
the remaining color conversion. Its perturbed FPS is excluded from all
comparison tables. The CPU stacks and symbol summaries are retained in
`output/ws13/final-cpu-profile`.

Code and measurements confirm:

- SDL swap interval is 0 in the measured live runs. The registered engine cvar
  is `r_swapInterval`; there is no registered `r_vsync` setting here.
- `Com_Frame` busy-polls `Com_EventLoop` and calls `NET_Sleep(0)` while below
  its integer minimum. No new sleep, timer or physics pacing was introduced.
- `SDL_WaitEventTimeout` is not called by the engine input path. SDL2-compat's
  internal pump was the repeated work; the nonblocking peep path addresses it.
- Per-frame APPLE fence tests are short in the observer. Adaptive GPU-sync
  mode 3 has a busy-wait loop and a reconstructed zero-valued `rdtsc_lo`;
  it should not be selected as a pacing fix.
- No per-frame `glGetError`/`glGetString` loop was found in `CDirect3DDevice`.
  Platform GL strings are queried for initialization/capabilities.
- Vertex buffers in this D3D layer are CPU-backed client arrays; the relevant
  draw path does not repeatedly call `glBufferData`/`glBufferSubData`.
  It rebinds VAO 0, programs and fixed-function state per draw. Caching or VBO
  migration needs complete invalidation across renderer/shadow/shader paths
  and belongs after WS15 parity, rather than a speculative partial cache.
- `glReadPixels` is in screenshots/readback, not the measured normal frame
  loop. The harness takes screenshots after the capture window.

Skipping the default framebuffer clear when a blit covers the whole drawable
was tested and reverted: matched runs were 394.36→403.09 FPS (+2.2%) while
the 1% low worsened 205.69→143.55. The GL submission sample moved from clear
to blit, so this was not a convincing removal of the limiting work.

Release ThinLTO was also built and run. The first link failed because
`declEnd_131795` aliases `_declEnd` in renderer assembly naming; explicitly
retaining `_declEnd` fixed the experimental link. Matched 1080p uncapped
runs were 569.20→580.39 FPS (+2.0%), but 1% low worsened 246.79→216.26.
LTO is therefore not enabled by default and no renderer alias refactor was
made. To reproduce the experiment:

```sh
cmake -S . -B output/ws13/build-lto -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,-u,_declEnd
cmake --build output/ws13/build-lto -j12 --target cod2_macos
python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --binary output/ws13/build-lto/cod2_macos --output output/ws13/repro-lto \
  --resolution 1920x1080 --maxfps 0 --seconds 15
```

## Reproduce recordings, live runs and replay

```sh
python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --output output/ws13/repro-record --resolution 1920x1080 \
  --window-mode windowed --maxfps 333 --seconds 30 --record ws13_street

# Read-only IWD links and an output-owned demo alias; never alter base data.
python3 - <<'PY'
from pathlib import Path
out = Path('output/ws13/repro-data/main')
out.mkdir(parents=True)
for p in (Path.home() / 'Games/CoD2/main').glob('*.iwd'):
    (out / p.name).symlink_to(p)
(out / 'demos').symlink_to(Path('output/ws13/repro-record/home/main/demos').resolve())
PY

COD2_BINARY="$PWD/build-macos/cod2_macos" tools/macos/bench.sh \
  output/ws13/repro-data ws13_street --output output/ws13/repro-demo \
  --resolution 1920x1080 --window-mode windowed --maxfps 0 --timeout 90

python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --output output/ws13/repro-live --resolution 2560x1440 \
  --window-mode borderless --maxfps 333 --seconds 15

MTL_HUD_ENABLED=1 python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --output output/ws13/repro-hud --resolution 1920x1080 \
  --window-mode windowed --maxfps 333 --seconds 15

python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --output output/ws13/repro-profile --resolution 1920x1080 \
  --maxfps 0 --seconds 5 --cpu-profile
```

Use new output directories for every run. To reproduce each table, replace
resolution with 1280×720/1920×1080/2560×1440, mode with
windowed/fullscreen/borderless, and maxfps with 333/0. The probe compiles only
its observer with installed clang/SDL; it does not change the engine binary.
The full result sets are `output/ws13/final-matrix.json` and
`output/ws13/timedemo-matrix.json`; each row's run directory contains its
console, configuration, CSV and results. `output/ws13/experiment-log.json`
records the kept/reverted decisions. These local evidence files remain
ignored and are not part of the merge.

## Remaining work and user settings

Use **windowed 1920×1080, `com_maxfps 333`, `r_swapInterval 0`** as the
current practical starting point; 2560×1440 windowed also has uncapped
headroom but does not meet the strict low-FPS criterion. 720p gives the most
measured headroom if visual resolution is secondary. Inspect the actual
drawable rather than assuming the requested mode succeeded. Fullscreen
and borderless currently need a quiet, interactive retest: several requests
had no fullscreen flag, some returned an 800×600 desktop, and true 6K desktop
fullscreen was considerably slower. Do not select a mode merely because
`r_fullscreen` reads 1.

The `SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0` experiment did not establish correct
borderless behavior: the actual drawable was 1920×491 without a fullscreen
flag. The subsequent 15 s windowed Metal-HUD-enabled run had a stable
1920×1080 drawable, 332.67 average FPS, 228.23 FPS 1% low, p99 3.918 ms and
no recorded geometry/focus changes. `r_gpuSync 0` was accepted as `off`, but
its 15 s 1080p cap run gave 329.03/147.17 FPS (average/1% low), with p99
5.396 ms; this is not evidence for a default-setting change. Fence-test
cost was tiny in the original adaptive runs as well.

A 60 Hz panel cannot display 333 distinct scanouts per second, though
uncapped engine rendering still controls input/update latency. Keep rendering
at 1080p or 1440p instead of 6K; desktop fullscreen still presents a scaled
frame to the 6016×3384 drawable. Request vsync off with `r_swapInterval 0` and
check actual SDL swap interval, rather than setting an unregistered `r_vsync`.

`cg_drawFPS Simple` is accepted and queried in the live console, but no FPS
counter is visible in the saved game JPEG. `MTL_HUD_ENABLED=1` was passed on
native runs, but the HUD's GPU timings could not be read: computer-use access
was pending macOS Accessibility/Screen Recording permissions, and game
readback does not include the Metal overlay. Thus no Apple HUD frame-time
number is reported. WS15 should resolve the FPS overlay/parity issue; visual
HUD verification remains open for an interactive run.

Work is stopped at verified dependencies rather than declaring the FPS goal
met: the shared machine cannot provide an isolated GPU test while parallel
clients are running; fullscreen/focus transitions are not reliable in this
session; the macOS profiler/HUD access needed to attribute the remaining
hitches is unavailable; and long listen-server stability depends on WS16.
After WS15/WS16 merge, re-run the serial matrix with one client, inspect the
counter and Metal HUD, and profile the residual tail before adding broader
draw batching, program/state caches or a VBO backend. Those changes cross
WS15's renderer work and require rendering-parity validation.

A prolonged listen-server profiling run exhausted the script allocator
(`MT_AllocIndex` failed allocating 16 bytes; 65,535 used buckets), then failed
during error cleanup. Normal listen-server shutdown also hangs in VM cleanup.
The harness kills only its own process group after collecting evidence and
records that fact. WS16 must fix these before long-match stability can be
accepted. No script/VM work was attempted in this branch.

No pushing, PRs, issues, remote changes, system installs, sibling-worktree
access or licensed-data commits. Local evidence is ignored. A safety hook
required preserving the temporary range-check source before restoring it;
the named stash `WS13 temporary indexed-range validation only` contains only
that experiment and must not be applied to the final branch.
