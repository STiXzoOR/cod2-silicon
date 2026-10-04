# WS19 — client pacing, presentation experiments and Game Mode

Branch: `port/hitch`, based on `df08d50`. All work and test bundles are in this
worktree. No push, PR, remote change, package installation or installed-app
replacement was performed.

## Outcome and merge decision

**This workstream does not establish constant 333 or complete the requested
nonblocking presentation path.** The client-frame observer and repeatable
validation tools work. The native builds and merge checks pass. The presentation
experiment remains opt-in (`r_presentMode 1`); the default is the original path
(`r_presentMode 0`). A GPU fence did not predict compositor availability and the
final fence placement made pacing worse. Enabling it by default would contradict
the measured result and the requirement to preserve input-to-photon latency.

Spaces requests now avoid SDL's exclusive-display-mode path, which explicitly
prevents Cocoa Spaces fullscreen. However, this locked session did not actually
enter a fullscreen Space. Game Policy recognized the test app, but Game Mode ON,
focus, physical raw mouse and latency remain unverified. Keep exclusive fullscreen
as the app default until unlocked measurements establish a better choice.

The orchestrator can merge the measurement tools, guarded observer hooks and
Spaces-selection fix independently of deciding whether to retain the opt-in
fence experiment. No CMake or shared `src/headers/*` edits are required.

Focused implementation commits, in order:

| Commit | Scope |
| --- | --- |
| `d137b9f` | Native client/cap observation and exact timing-fixture stub. |
| `6aefed6` | Initial selectable fence experiment. |
| `b911fb6` | Use desktop mode for a Spaces request. |
| `38b916d` | State snapshots, owned launches and validation helpers. |
| `86463ed` | Measure GL flush and fence issuance. |
| `44bf32f` | Default mode 0; final fence placement and matching arm64 guard. |
| `e32c054` | Validate default mode; reject locked/unfocused captures. |
| `c9f362d` | Optional AppKit fetch/dispatch timing. |
| `277a1ba` | Create output parents in a fresh worktree. |

Merge the complete branch rather than the initial fence commit alone: later
commits disable its default and remove the extra post-swap flush. Preserve the
ignored evidence before deleting this worktree. The report is committed separately.

## Conditions and measurement meaning

Every WS19 result below was taken in the locked session. The initial unchanged
baseline has a manual `ioreg -n Root -d1` snapshot showing
`CGSSessionScreenIsLocked = true`, plus process/display snapshots in
`output/ws19/baseline`; it has no separately measured post-run lock endpoint.
Every subsequent benchmark records true before and after. **None is final
unlocked-screen acceptance.** Lock evidence and the baseline's missing endpoint
are recorded in each `results.json`. The main display reports 1920×1080 at 60 Hz;
1440p and 4K are render/backing workloads on that display, not physical display
modes. The observer records the actual viewport, CGL backing size, SDL window
flags, swap interval and QoS. Geometry/focus changes are recorded too.

Apart from that manual baseline, each run has `before/after-session.txt`, `before/after-display.txt` and
`before/after-processes.txt`. The process snapshot uses PID, PPID, CPU, memory and
executable name, not arguments that might disclose a key. Existing user applications,
an aerial wallpaper and an unowned `powermetrics` were left running. During the later
experiments another pre-existing/unowned Codex audit process tree ran several
CPU-heavy worker processes. Its ancestry did not match my benchmark or merge-gate
processes. I did not kill it, inspect its files or enter a sibling worktree. This
load and the locked compositor are substantial comparison confounders.

Live combat uses the WS18 Toujane spawn, real shader cache, 30-second cycles of
fire/reload/movement/grenades, 120 seconds per final run, three repeats per
resolution/cap. The 1% low is 1000 divided by the mean of the slowest 1% of
intervals; P99 is the 99th percentile. Timedemo uses WS18's 2,420-frame demo and
the existing integer-millisecond engine CSV. Its P99 is only resolved to 1 ms.

The new live **client interval** is measured just after `Com_Frame`'s existing
cap/event loop and before `Cbuf_Execute`, using `mach_absolute_time`. It also
records the integer `com_frameTime` delta, which is what simulation consumes.
Main-thread CPU comes from `CLOCK_THREAD_CPUTIME_ID`. The separate
`frames.csv.presented.csv` measures intervals between **swap returns**. This is a
submission/presentation proxy, not physical scanout or photon timing; a 60 Hz
display does not physically present 333 images/s. No latency claim is made from
these values.

All raw evidence is ignored under `output/ws19/`, never committed. Preserve this
directory if individual CSVs or process snapshots are needed. The installed
`~/Applications/CoD2x Native.app` was not replaced. Test bundles have unique bundle
IDs, and the scripts unregister only their test bundles on exit. Final cleanup
unregistered the original WS19 and final validation bundles; the preliminary
bundle had already been unregistered before its directory was moved. No owned
observed game process remains. A read-only CoreServices query confirmed the
`cod2x` default handler is `org.opencod2.cod2x.native`, matching the installed
app (`output/ws19/launchservices-final.json`).

## Before and after

First, the unchanged `df08d50` CoD2x executable was preserved as
`output/ws19/baseline/client` and measured at capped 1080p for 120 seconds under
the current locked conditions. That observer necessarily measures swap-boundary
intervals because the old executable has no `Com_Frame` callback.

| Run | Boundary | Average fps | 1% low fps | P99 ms | Lock state |
| --- | --- | ---: | ---: | ---: | --- |
| WS18 1080p capped reference | Swap | 331.2 | 205.2 | 4.261 | Not recorded by WS18 |
| WS19 unchanged baseline, 120 s | Swap | 332.64 | 232.17 | 3.669 | Locked pre-run; no post snapshot |
| WS19 old path with client observer, 60 s | Client | 332.17 | 226.54 | 3.863 | Locked |

The unchanged baseline and short presentation/cap/clear A/B runs launched the
native executable directly. The final matrix launches a fresh signed test app
through LaunchServices, like WS18. App activation and locked-session policy can
differ between these paths; do not interpret this as a clean code-only before/
after comparison.

The unchanged locked baseline's 1% low was 27.0 fps higher than WS18, before any
optimization. That alone prevents attributing a cross-session change to code.
A small 64×64 GL capability probe briefly overlapped the first baseline; subsequent
paired comparisons did not run GL probes or compiler workloads concurrently.

Final evidence: `output/ws19/matrix-final/summary.json`. All 18 live runs
recorded client boundaries, 120 seconds, requested viewport/backing, unchanged
presentation geometry, real-cache manifest verification and clean `quit`.
All 18 timedemos completed with matching 2,419 timing rows and clean scripted
shutdown. Every before/after lock check is true. These are observational
results, **not acceptance**. Mode 0, Spaces 0 was used throughout.

The following tables show median (minimum–maximum), with three repeats per row.
The final matrix ran later, with different background load and app policy from
the direct-executable baseline. There is no measured optimization retained in
the default presentation path.

| Live client intervals, n=3 × 120 s | Cap | Average fps | 1% low fps | P99 ms | Lock |
| --- | ---: | --- | --- | --- | --- |
| 1920x1080 | 333 | 310.8 (307.6–312.9) | 127.0 (121.7–129.0) | 7.215 (7.155–7.370) | Locked |
| 1920x1080 | 0 | 733.0 (679.7–736.5) | 177.2 (170.6–181.6) | 5.066 (5.008–5.217) | Locked |
| 2560x1440 | 333 | 306.3 (305.3–316.4) | 128.2 (126.8–138.9) | 7.160 (6.567–7.251) | Locked |
| 2560x1440 | 0 | 676.2 (626.4–685.5) | 168.9 (163.1–176.4) | 5.418 (5.141–5.581) | Locked |
| 3840x2160 | 333 | 318.4 (318.3–318.8) | 141.1 (139.9–141.2) | 6.407 (6.385–6.411) | Locked |
| 3840x2160 | 0 | 411.7 (398.3–432.7) | 134.6 (134.5–137.4) | 6.910 (6.754–6.935) | Locked |

| Swap-return intervals (presentation proxy), n=3 | Cap | Average submissions/s | P99 ms | Lock |
| --- | ---: | --- | --- | --- |
| 1920x1080 | 333 | 310.8 (307.6–312.9) | 7.739 (7.689–7.907) | Locked |
| 1920x1080 | 0 | 733.0 (679.7–736.5) | 5.066 (5.007–5.213) | Locked |
| 2560x1440 | 333 | 306.3 (305.3–316.4) | 7.703 (7.166–7.850) | Locked |
| 2560x1440 | 0 | 676.2 (626.4–685.5) | 5.423 (5.146–5.582) | Locked |
| 3840x2160 | 333 | 318.4 (318.3–318.8) | 6.980 (6.946–7.006) | Locked |
| 3840x2160 | 0 | 411.7 (398.3–432.7) | 6.909 (6.757–6.934) | Locked |

| Timedemo, n=3; integer-ms timing | Cap | Average fps | 1% low fps | P99 ms | Lock |
| --- | ---: | --- | --- | --- | --- |
| 1920x1080 | 333 | 329.1 (328.7–329.7) | 210.1 (192.3–221.2) | 4 (4–4) | Locked |
| 1920x1080 | 0 | 779.6 (778.6–780.9) | 186.6 (185.2–196.9) | 5 (4–5) | Locked |
| 2560x1440 | 333 | 327.0 (323.1–327.8) | 176.1 (174.8–188.0) | 5 (4–5) | Locked |
| 2560x1440 | 0 | 741.0 (731.1–749.2) | 186.6 (186.6–192.3) | 5 (5–5) | Locked |
| 3840x2160 | 333 | 329.1 (327.8–329.3) | 223.2 (189.4–229.4) | 4 (4–4) | Locked |
| 3840x2160 | 0 | 416.7 (415.7–418.3) | 143.7 (142.0–144.5) | 6 (6–6) | Locked |

For comparison, WS18 had one run per row, swap-boundary live timing and no
recorded lock state. These values are its report, not a paired control.

| WS18 reference, n=1; lock unknown | Cap | Live average / low / P99 ms | Timedemo average / low / P99 ms |
| --- | ---: | --- | --- |
| 1920x1080 | 333 | 331.2 / 205.2 / 4.261 | 330.2 / 219.3 / 4 |
| 1920x1080 | 0 | 798.6 / 354.1 / 2.459 | 843.2 / 324.7 / 2 |
| 2560x1440 | 333 | 332.4 / 233.0 / 3.827 | 333.2 / 308.6 / 3 |
| 2560x1440 | 0 | 663.5 / 326.8 / 2.696 | 788.0 / 304.9 / 3 |
| 3840x2160 | 333 | 332.3 / 230.8 / 3.936 | 331.6 / 238.1 / 4 |
| 3840x2160 | 0 | 417.1 / 256.9 / 3.407 | 413.3 / 219.3 / 4 |

The requested live target (low ≥320, P99 ≤3.3 ms) is missed at both 1080p and
1440p under lock. Uncapped medians of 733/676/412 fps still show substantial
throughput; they do not cure the slow tail. Swap-return P99 can differ from the
client P99 because swap occurs partway through the frame. It is never a
measurement of 60 Hz scanout or latency.

Across the final 18 live runs, 18,698 client intervals exceeded 6 ms: 18,605 had
more than half their time in swap, 8 in poll/pump/peep, 1 in clear, 1 in cap wait,
and 83 were not dominated by those measured phases. Only 38 spent more than 80%
in thread CPU. Phase maxima were swap 12.966 ms, pump 8.156 ms, scene clear
0.061 ms, drawable clear 8.659 ms, and cap wait 6.040 ms. Maximum cap-wait
overshoot was 5.354 ms (sleep overshoot 5.433 ms), so rare scheduling/wait delays
are also present under this later load. The worst client interval was 41.900 ms,
with CPU 3.986 ms, swap 0.229 ms, poll 0.032 ms, clear 0.162 ms and no cap wait;
it remains unclassified. These counts cover three times WS18's capture duration
and different machine/launch conditions; they are not evidence of a code-caused
regression. Initial observer metadata and periodic geometry checks also run on
the measured thread, so unclassified time must not all be assigned to the OS.


## Observer and cap audit

Instrumentation is disabled by default. `COD2_FRAME_CSV` enables a cached `dlsym`
lookup of optional DYLD observer callbacks. There is no per-frame clock read or
CSV write in the normal client. `tools/macos/frame-probe.c` captures client and
swap-return intervals, main-thread CPU, swap, pump/peep/poll, blit, clear,
scene/presentation clear, fence query/issuance, flush, cap wait, cap overshoot,
Mach sleep and sleep overshoot. Phase accounting is aligned with the preceding
client interval. The observer saves after capture, outside the measured next
interval.

The cap remains the previous bounded 500 µs wait with a Mach deadline and an
80 µs final spin. The integer `minMsec`, simulation calls, usercmd generation and
`cl_maxpackets` logic are unchanged. Cap sleep is observed only when the main
thread marks entry into `MacSystem_WaitUntil`; unrelated Mach waits are excluded.
`sleep_overshoot_ms` is lateness relative to the sleep's early deadline, whereas
`wait_overshoot_ms` is lateness relative to the entire cap-wait target.

These individual 30-second experiments used real shaders, 1080p capped 333 and
the then-current presentation prototype. They are exploratory, not three-repeat
acceptance. **Every row is locked.** Temporary environment controls and scheduler
changes were removed.

| Cap experiment | Average fps | 1% low fps | P99 ms | CPU ms/frame | Lock |
| --- | ---: | ---: | ---: | ---: | --- |
| 80 µs spin control A | 333.08 | 285.04 | 3.040 | 1.580 | Locked |
| 500 µs spin | 333.08 | 292.71 | 3.005 | 2.869 | Locked |
| 200 µs spin | 331.43 | 212.26 | 4.173 | 2.416 | Locked |
| Time-constraint policy | 331.35 | 207.95 | 4.191 | 2.022 | Locked |
| 80 µs spin control B | 333.02 | 277.68 | 3.062 | 1.551 | Locked |

500 µs reduced mean wait overshoot from 0.03256 ms to 0.0000395 ms and raised the
1% low 7.7 fps over control A, but used 1.29 ms more CPU per frame and still missed
320. The intervening/control spread prevents treating this single improvement
as robust. The 200 µs run and real-time policy were worse. Keep the existing cap.

`pthread_get_qos_class_np` confirmed USER_INTERACTIVE (`0x21`).
`thread_policy_set(THREAD_TIME_CONSTRAINT_POLICY)` accepted a 3 ms period,
2 ms computation, 3 ms constraint, preemptible policy (return 0), but observed QoS
became `0x0` and pacing degraded. Success of this API does not prove P-core
residency. Public interval workgroups in the installed SDK are audio workgroups;
ordinary workgroups are not a public P-core-pinning API. I did not pretend the
game was an audio real-time thread or use private affinity APIs.

## Presentation: measured failures and retained comparison

The renderer already draws the scene into an FBO. Mode 1 checks a zero-timeout
ARB sync fence before copying the newest scene to the drawable. If the previous
GPU submission is incomplete, it flushes the current scene and returns; it does
not store or replay an older completed image. Mode 0 uses the original path.
The fence is deleted with the owning GL context current, including shutdown.

This only gates GPU completion. It cannot guarantee that
`SDL_GL_SwapWindow`/`NSOpenGLContext.flushBuffer` will not wait for the compositor.
In the capped experiments the query returned no timeouts, even when swap stalled.
Therefore this is **not successful decoupling of the client from presentation**.
A true async present path would require ownership/synchronization changes and
unlocked latency/refresh evidence; those were not safe to declare complete here.

Three matched 60-second 1080p capped comparisons initially looked promising with
a fence issued after swap and an explicit flush:

| Initial presentation A/B, n=3 | Median avg fps (range) | Median 1% low (range) | Median P99 ms (range) | Lock |
| --- | --- | --- | --- | --- |
| Old path, mode 0 | 332.48 (332.12–332.90) | 237.08 (223.68–264.27) | 3.639 (3.161–3.866) | Locked |
| Post-swap fence + flush | 332.97 (332.07–333.02) | 271.45 (233.82–276.42) | 3.244 (3.236–3.547) | Locked |

This 34.4 fps median low improvement did not reach 320; all queries completed
without skipping a swap. The probe gained finer phase accounting between the
second pair's runs, so the table is an exploratory comparison, not proof of a
causal optimization. A later three-repeat flush-only experiment had median low
216.31 (212.69–267.12) and median P99 3.969 (3.169–4.032) ms; it was rejected.

A later probe measured the explicit post-swap `glFlush` at mean 1.347 ms/frame,
maximum 2.173 ms. Fence issuance itself averaged 0.000544 ms. In that locked
60-second run, mode 1 was 328.08/180.96/P99 4.779, versus mode 0 at
328.55/185.64/P99 4.742. Removing the extra flush by issuing the fence before the
swap made swap much slower instead. Timing locates the increased cost inside
swap rather than fence issuance; the driver's internal mechanism is unverified.

| Final fence A/B, n=3 × 60 s | Median avg fps (range) | Median 1% low (range) | Median P99 ms (range) | Lock |
| --- | --- | --- | --- | --- |
| Mode 0, original path | 328.27 (328.25–329.07) | 182.29 (181.63–187.03) | 4.740 (4.588–4.814) | Locked |
| Mode 1, fence before swap | 284.63 (284.18–290.21) | 126.60 (125.95–126.93) | 7.291 (7.228–7.394) | Locked |

The final consistent probe showed a 55.7 fps median low regression and a 2.55 ms
P99 increase. Mode 1's first run spent 2.100 ms/frame in swap versus 0.469 ms in
the matched mode 0 run. All six runs still had zero fence timeouts. This is why
mode 1 is opt-in rather than the requested default-on optimization.

The installed OpenGL reports `2.1 Metal - 91.7`, ARB sync and APPLE fence support.
A standalone ignored probe found `kCGLCESwapLimit = 0` already disabled,
`kCGLCPMPSwapsInFlight = 1` and `kCGLCEMPEngine = 0`.
Setting the existing swap-limit/flight values succeeded
but changed nothing. No system OpenGL setting was changed.

## Events and clear

The existing native raw-mouse implementation already receives GameController
mouse deltas on a dedicated dispatch queue and drains them through the existing
input queue during the cap loop. Adding a second IOHID/raw-mouse thread would
duplicate an existing path, not explain the Cocoa event stall.

SDL3 3.4.16's Cocoa pump drains `NSApp` events with `nextEventMatchingMask` using
`distantPast`, then calls `sendEvent` (and services any modal session). There is
no deliberate
5 ms sleep in that source path. The observer separates pump and peep, but a slow
AppKit call can include driver/OS/preemption time; its root cause is not fully
attributed. It would be inaccurate to label all unaccounted time as scheduling.

The separate optional Cocoa probe (`--cocoa-probe`) reproduced a 4.552 ms pump
in a 120-second app capture. `nextEventMatchingMask:untilDate:inMode:dequeue:`
took 4.476 ms and returned **nil**, accounting for 98.3% of that pump. The other
slow fetch took 0.923 ms and also returned nil. No send, window-update or modal
call exceeded the probe's 0.5 ms threshold. This locates the delay in AppKit's
fetch/run-loop path rather than dispatch of a returned input event; it still
does not distinguish IPC, driver work or descheduling inside that call. The
affected client interval was 7.224 ms with 3.577 ms thread CPU. The profiling
run averaged 304.95 fps, low 123.33 and P99 7.537 ms, **locked and perturbed by
instrumentation**, not acceptance. Evidence is `output/ws19/cocoa-1/cocoa.csv`.

`tools/macos/cocoa-probe.m` wraps the active NSApplication class's public methods
after startup, preserving arguments, return values and event order. It records
only slow calls while the frame observer is recording. It is not compiled or
enabled in the default observer or the game. The committed CLI was also exercised
in `output/ws19/cocoa-tool-check` (10 seconds, real shaders, scripted quit,
exit 0). Repeat this diagnostic separately from the validation matrix:

```sh
SDL_VIDEO_MAC_FULLSCREEN_SPACES=0 python3 tools/macos/live-bench.py \
  "$HOME/Games/CoD2" --app "output/CoD2x WS19 Check.app" \
  --output output/cocoa-unlocked --window-mode fullscreen --seconds 120 \
  --combat --cocoa-probe --set r_presentMode 0
```

| Individual 30 s change | Average fps | 1% low fps | P99 ms | CPU ms/frame | Lock |
| --- | ---: | ---: | ---: | ---: | --- |
| Control A | 333.12 | 288.11 | 3.049 | 1.523 | Locked |
| Skip fully covered drawable color clear | 331.96 | 218.20 | 3.989 | 1.787 | Locked |
| Pump every 3 ms instead of 1 ms | 332.67 | 241.52 | 3.529 | 1.733 | Locked |
| Control B | 332.81 | 252.67 | 3.357 | 1.787 | Locked |

Both were rejected and the original clear/pump behavior restored. Pumping less
often also adds up to 2 ms to keyboard/button delivery, which conflicts with the
latency constraint. Separating scene and drawable clear showed that the costly
clear is usually the drawable's color clear, suggesting drawable acquisition or
driver backpressure; this is an inference, not a verified driver explanation.

The discarded preliminary matrix (`output/ws19/matrix-preliminary`) contained
a 36.68 ms client interval with only 0.29 ms in swap, 0.02 ms in pump, 0.20 ms in
clear and 2.49 ms of thread CPU. That interval remains unclassified. Another
frame spent 3.88 ms in pump and a swap reached 11.32 ms. The matrix was stopped
by sending `quit` through the verified owned FIFO before changing instrumentation;
its incomplete runs are not included in the final matrix.

## Spaces fullscreen and Game Mode

SDL's Cocoa path explicitly excludes `fullscreen_exclusive` from a Spaces
transition. The native display layer now promotes a fullscreen request to
desktop/borderless fullscreen when `SDL_VIDEO_MAC_FULLSCREEN_SPACES=1`, before
setting a physical display mode, and requests synchronous SDL window operations.
The selected render backing size is still handled by the existing CGL/FBO path.
`SDL_VIDEO_MAC_FULLSCREEN_SPACES=0` preserves the default exclusive behavior.

The first locked app comparison at 1080p/333 (15 s) returned average 329.22,
low 151.68 and P99 4.126 ms, but actual window flags were `0x6`: **windowed**, not
a successful Space. It is not a valid fullscreen pacing comparison. Exclusive
controls had fullscreen flags `0x7`. Game Policy's owned-PID lines identified
the unique test app and `LSSupportsGameMode`; system-session states were paused,
then off after exit, not ON. The log is `output/ws19/game-policy-spaces.log`.

The final helper run in `output/ws19/matrix-final/gamemode` returned 0 and
recognized owned PID 69932. It again logged `paused, off`, and actual flags were
`0x6`, with a 1920×972 window and 1920×1080 render backing. Capture/log file times
span 24.4 seconds. Shutdown was scripted `quit`. Lock state was true before and
after. This demonstrates the helper mechanics, not Game Mode activation or a
valid fullscreen pacing comparison.


`tools/macos/check-gamemode.sh` launches only the supplied test bundle, captures
lock/window state and Game Policy logs, prints recognized PID and observed
system-session states, sends scripted `quit`, and unregisters that test bundle.
It bounds launch and log collection with `timeout -k`. System-session state can
also reflect another game, so the helper prints that caveat instead of treating
any ON line as proof this process activated Game Mode.

## Merge gate and unchanged i386 path

`output/ws19/gate-final/results.json` records exit 0 for both Release builds and
all requested suites. The complete ABI run in `output/ws19/gate/abi-final.log`
finished with 636 translation units, 0 errors, 0 mismatches, 0 new mismatches;
218 renderer bindings with all mismatch counts 0; and 514 imports with 0 proven
extra/missing dereferences. Initial 300-second ABI attempts timed out during the
import pass; they are not counted as passes. The full run used 1,200 seconds and
finished successfully. Subsequent production edits changed a default integer,
GL call placement and the matching arm64 guard, not ABI types or prototypes.

Commands actually run (every command returned 0):

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS= \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos -j12
cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos-codx --target cod2_macos -j12
python3 tests/fixes13/run.py --build build-macos-codx
ASAN_OPTIONS=symbolize=0 sh tests/lp64/renderer/run.sh
python3 tests/lp64/game/run.py
python3 tests/lp64/script/run.py
python3 tests/lp64/script/vm_semantics.py
sh tests/cod2x/run.sh
python3 tests/perf/run.py
python3 tests/online/run.py build-macos/compile_commands.json
timeout -k 10 1200 sh tools/abi/check.sh build-macos-codx/compile_commands.json \
  output/ws19/gate/abi-final
python3 tests/fixes13/legacy.py --base df08d50
sh tests/cod2x/run_native.sh
```

The legacy checker found **5 changed source files, 10 legacy configurations,
0 mismatches**. `tests/fixes13` preserves its exact engine fixtures; the native
timing fixture only adds an observer stub asserting the existing engine time.
All production edits are in native-only sources or behind the existing
`COD2_APPLE_SDK`/Apple/X64/arm64 gates. Source preprocessing matches with X64 off;
this is not a fresh linked-i386 byte comparison. An actual `xcrun clang -arch i386`
probe compiled an object, but linking failed with
`ld: linking for i386 is no longer supported`. No substitute system toolchain
was installed. Optional `pkg-config` was unavailable; the already installed
`sdl2-config` supplied the required flags.

A supplemental source-only use of `tools/abi/legacy.py` also found 5 files ×
5 X64-off configurations with 0 mismatches, and 6 actual Release SDK commands
with X64 removed with 0 mismatches/errors (`gate-final/legacy-native-sdk.log`).
Its unfiltered invocation reports 10 expected nonproduction differences in the
changed timing fixture and DYLD observer, which are not i386 game translation
units. The source-only filter was applied in an ignored local copy, without
changing the shared checker. Both Release builds and fixes13 were rerun after
matching the present-mode registration guard to the arm64-only platform setter.

The final CoD2x smoke launched the Release executable with real shaders under
`timeout -k 10 90`, `+devmap mp_toujane`, unique
`output/ws19/gate-final/smoke/home` and `net_port 29519`. After `CS_ACTIVE` it sent
`screenshotJPEG ws19-smoke` and `quit`. Exit was 0 and the JPEG exists at
`output/ws19/gate-final/smoke/home/main/screenshots/ws19-smoke.jpg`. Its exact
argument array and log are saved beside it. `python3 -m py_compile`, `bash -n`
for both helpers and `git diff --check` also pass.

## Exact unlocked validation commands

Run from the merged worktree with the screen unlocked. The first command creates
an isolated test app, runs all 18 live 120-second captures and 18 timedemos
(1080p/1440p/4K × 333/uncapped × three repeats), then checks Game Mode. It leaves
the installed app alone. This takes approximately an hour, not one minute.

```sh
tools/macos/validate-333.sh
```

The summary exits nonzero when capped default-mode 1080p/1440p fail either
low ≥320 or P99 ≤3.3 ms in any repeat, or when captures are locked, lack proper
fullscreen/focus/size, change geometry, end early or fail to quit. `COD2_ALLOW_LOCKED=1`
is only for observational evidence; it is deliberately absent above.

For the under-one-minute Game Mode check, build a separate test app first, then
run the helper. These commands do not invoke the CMake app target, which would
replace the installed app.

```sh
cmake --build build-macos-codx --target cod2_macos -j12
python3 tools/cod2x/make_macos_app.py build-macos-codx/cod2_macos \
  "output/CoD2x WS19 Check.app" --bundle-id org.opencod2.ws19.check \
  --game-dir "$HOME/Games/CoD2" --replace
tools/macos/check-gamemode.sh "output/CoD2x WS19 Check.app"
```

For the full unlocked presentation/Spaces comparison, the same validator accepts
both mode lists. This expands to 72 live captures plus timedemos, so budget
several hours. Do not describe a Spaces run as fullscreen unless the recorded
flags and geometry confirm it.

```sh
COD2_PRESENT_MODES="0 1" COD2_SPACES_MODES="0 1" tools/macos/validate-333.sh
```

The old WS18 demo alias points to the removed ship worktree. The validator leaves
it untouched and makes ignored symlinks to the authorized archived demo and the
read-only game IWDs inside its own output directory. Demo SHA-256 is
`c980acf6c5fe95dc1e54e40e0a0cfac410e21315ff9787b5b6d3108519bba633`.

## Primary references and remaining blockers

The SDL source used for diagnosis is upstream release 3.4.16's
[Cocoa window implementation](https://github.com/libsdl-org/SDL/blob/release-3.4.16/src/video/cocoa/SDL_cocoawindow.m),
[event pump](https://github.com/libsdl-org/SDL/blob/release-3.4.16/src/video/cocoa/SDL_cocoaevents.m)
and [OpenGL implementation](https://github.com/libsdl-org/SDL/blob/release-3.4.16/src/video/cocoa/SDL_cocoaopengl.m).
The [SDL Spaces hint](https://wiki.libsdl.org/SDL2/SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES),
[Apple OpenGL threading guide](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_threading/opengl_threading.html)
and [Apple Silicon performance guide](https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon)
inform the investigation. SDK headers supplied the actual CGL/workgroup/policy
contracts. No code was copied from a decompile or another game reconstruction.

Remaining: a verified nonblocking compositor present path, unlocked 333
acceptance, physical input-to-photon comparison, successful Spaces transition
with focused raw input, confirmed Game Mode ON, and full attribution of the rare
AppKit/clear/unclassified stalls. The locked screen prevents those behavioral
and latency checks. The experiments here do not justify pretending those tasks
are done.
