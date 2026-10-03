# Run and measure the native Mac client

These scripts are ready for the `cod2_macos` target after WS1/WS2 and runtime
work are integrated. They have been exercised with synthetic engines, not a
running game. Use the owned-data layout in
[game-data.md](../../docs/macos-port/game-data.md): the argument is the parent
of `main/`. All generated logs/configs/results default to ignored `output/`.

## Launch

```sh
# Build with the integrated macOS CMake target; no system packages are installed.
cmake -S . -B output/build-macos -DCOD2_X64=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_C_FLAGS=-ffp-contract=off
cmake --build output/build-macos --parallel 3 --target cod2_macos

tools/macos/run.sh '/path/to/CoD2 data' +set name native-test
# Optional binary override and server connection:
COD2_BINARY="$PWD/output/build-macos/cod2_macos" \
  tools/macos/run.sh '/path/to/CoD2 data' +connect 192.168.1.50:28960
```

`run.sh` sets `com_maxfps 250`, `r_swapInterval 0`, `m_filter 0`,
`cl_mouseAccel 0`, and `logfile 2`, then passes your arguments last for explicit
overrides. It uses a fresh writable `fs_homepath`, captures stdout/stderr in
`console.log`, and retains the engine's `qconsole_mp.log` and saved configs
under `home/`. `launch.txt` records the machine and arguments. Set
`COD2_RUN_DIR` to choose the output directory; use a new directory for a clean
run. Ctrl-C stops the foreground launch; engine failures propagate through
`tee`. Paths with spaces are preserved through the legacy argv concatenation;
paths containing double quotes, semicolons, `+`, or newlines are rejected.

## Timedemo and frame times

Put a fixed, trusted 1.3 multiplayer recording in `main/demos/example.dm_1`.
Use the same file, content, resolution, renderer settings and client version
for each measurement. The source registers `record`, `stoprecord`, `demo` and
`timedemo` in `src/PC/client_mp/cl_main_mp.c`. The benchmark command is
**`timedemo example`**, not `set timedemo 1; demo example`.

```sh
tools/macos/bench.sh '/path/to/CoD2 data' example \
  --output output/bench-warmup --timeout 180
tools/macos/bench.sh '/path/to/CoD2 data' example \
  --output output/bench-uncapped --timeout 180
tools/macos/bench.sh '/path/to/CoD2 data' example \
  --output output/bench-250 --maxfps 250 --timeout 180
```

Each invocation starts a new client and home directory. The first run warms
OS file caches; repeat several runs and retain every result rather than
cherry-picking. Apply identical graphics options in the base data's trusted
config, or edit the wrapper invocation consistently for the experiment.
`--maxfps 0` removes the user FPS cap (the current frame loop still enforces a
minimum 1 ms); `--maxfps 250` targets a minimum 4 ms interval. Neither proves
steady 250 fps during live network play.

`CL_DemoCompleted` prints `<frames> frames, <seconds> seconds: <fps> fps`.
`CL_SetCGameTime` writes headerless `frame,milliseconds` rows to
`home/main/demos/timedemo_<map>_mode_<cl_timedemoMode>.csv` (the actual home/game
subdirectory may vary). **Current-source quirk:** the CSV and fixed 50 ms demo
step are gated by `cl_freezeDemo`, whereas the completion message checks
`isTimeDemo`. The wrapper explicitly sets `cl_freezeDemo 1`; upstream does not
register `cl_timedemoMode` here, and its lookup only labels the CSV. This is a
reconstruction behavior, not a claim about stock CoD2's benchmark internals.

`bench.sh` waits for that completion message, allows the CSV to close, then
stops the entire process group (including surviving engine children) because `nextdemo quit` is not implemented in
`CL_DemoCompleted` here. It fails on missing completion before the deadline, timeout, engine
failure, the engine's truncated-demo diagnostic, missing/ambiguous CSV or a CSV count inconsistent with the summary.
It never fabricates timing samples from average FPS. Logs, CSV,
`benchmark.json` and `results.json` remain in the selected output directory.

Results include engine FPS/seconds, mean/median/p95/p99/max milliseconds and
sample count over 4 ms. Percentiles use nearest rank. The initial timestamp
frame is omitted by the engine, so N reported frames require N-1 samples.
The engine clock has integer-millisecond precision and prints aggregate FPS
rounded to one decimal: 0 ms samples are possible at high speed. This is not a
submillisecond latency measurement. Look for 4 ms pacing and a low tail at the
250 cap, then validate a full live round with the same settings. Loading,
background activity, HUD/logging overhead and thermal state affect results.

## Competitive-play cvars verified in source

The values below come from `cl_main_mp.c`, `cl_input.c`, `common.c`,
`r_dvars.c` and `sv_init_mp.c`; mods and CoD2x gates may change allowed ranges.
They are a baseline for controlled testing, not a universal competitive config.

| Cvar | Source default/range | Testing use |
| --- | --- | --- |
| `com_maxfps` | 85; 0–1000 | 250 gives a 4 ms minimum frame interval; 0 removes the user cap |
| `r_swapInterval` | 0; bool | 0 disables requested vsync; renderer/platform must honor it |
| `r_fullscreen`, `r_mode` | 1; display-mode enum | Record actual resolution and fullscreen mode for comparable GPU work |
| `m_filter` | 0; bool | Leave 0 to avoid two-sample mouse smoothing in `CL_MouseMove` |
| `cl_mouseAccel` | 0; 0–100 | Leave 0 for constant engine sensitivity; it does not control macOS acceleration |
| `sensitivity` | 5; 0.01–100 | Keep identical, with mouse DPI and `m_yaw`/`m_pitch` (default 0.022) recorded |
| `cl_maxpackets` | 30; 15–100 | 100 is the stock maximum client packet rate; compare cadence in PCAPs |
| `cl_packetdup` | 1; 0–5 | Redundant usercmd history; keep the same value between clients |
| `rate` | 5000; 1000–25000 | 25000 is the stock upper bandwidth request; server may clamp it |
| `snaps` | 20; 1–30 | Request 20 for the test server's `sv_fps 20`; server cadence limits delivery |
| `cg_drawFPS` | display enum | Use for a visible counter; the timedemo CSV measures frame intervals |
| `logfile` | 0; 0–2 | 2 flushes diagnostic output; keep logging conditions identical |

SDL relative mouse mode appears in `src/unix/linux_input.c`, but the raw-input
acceptance check belongs to WS3: verify that macOS acceleration is bypassed,
relative deltas remain consistent at different movement speeds, cursor capture
works, and focus changes do not create jumps. `m_filter 0` and
`cl_mouseAccel 0` alone do not prove true raw input. No `m_rawinput` cvar is
invented by these scripts.

## Metal performance HUD

```sh
MTL_HUD_ENABLED=1 tools/macos/run.sh '/path/to/CoD2 data'
MTL_HUD_ENABLED=1 tools/macos/bench.sh '/path/to/CoD2 data' example \
  --output output/bench-hud --maxfps 250
```

Apple documents the environment toggle and HUD's FPS, frame interval and GPU
cost in [Discover Metal Performance HUD](https://developer.apple.com/videos/play/tech-talks/110339/).
Read frame interval as presentation pacing, and GPU time as device execution
cost; the two are not interchangeable with CPU frame time. At 250 fps the
budget is 4 ms. A low GPU cost with a high/spiky interval points toward CPU,
presentation or scheduling limits; high GPU cost calls for renderer/resolution
investigation. This is a diagnostic inference, not proof of the bottleneck.
Watch interval spikes and worst values as well as the mean.

The port initially uses legacy OpenGL 2.1 through Apple's driver. HUD support
for that path has **not** been observed; if no overlay appears, do not interpret
it as a successful performance result. The engine CSV remains available, and
Instruments can provide deeper CPU/GPU evidence. Compare HUD-on and HUD-off
runs because the overlay itself can affect timing. Environment changes here
apply only to the launched process; no global defaults are changed.
