# WS104 — Preserve the launcher snapshot canvas on macOS 27

2026-10-09. Branch `codex/snapshot-harness`, based on release lineage
`port/main` at `664f1a1`. Fix commit: `e3e81a5`.

The dimension regression is fixed in the review harness. The full launcher gate
is **still red** at its later dark/light comparison: this locked login session
returns transparent WindowServer images. No assertion was relaxed. No launcher
product, engine, typed-data, CMake or build-script source changed.

Evidence is retained at:

```text
~/Library/Application Support/CoD2-native-current-readiness/WS104.noindex/
```

## Measurements and cause

Read `CONTRIBUTING.md` and `docs/macos-port/PLAN.md` before editing. Added
opt-in `COD2_SNAPSHOT_GEOMETRY=1` diagnostics before making the capture fix.
The diagnostics log the window frame, content layout rectangle, content and
frame-view bounds, backing scale, screen rectangles, own-window WindowServer
bounds and image dimensions. The `constrainFrameRect` override only logs the
result from `super`; it does not change constraints.

On macOS 27.0.1 / 26A434, SDK 27.0, the display is 3008×1692 points with a
1606-point-high visible frame and backing scale 2. For the home screen:

| Step | AppKit frame size | Hosting-view / frame-view size | Content layout size | Image size |
| --- | --- | --- | --- | --- |
| Window initialization | 1440×900 | 1440×900 | 1440×868 | — |
| Attach hosting controller | 1×1, transient | 1×1 | 1×0 | — |
| First explicit `setFrame` | 1440×900 | 1440×900 | 1440×848 | — |
| Order front and settle | 1440×900 | 1440×900 | 1440×848 | — |
| Second `setFrame` and settle | 1440×900 | 1440×900 | 1440×848 | — |
| Original WindowServer capture | 1440×900 | 1440×900 | 1440×848 | 2824×1764 |
| Existing `cacheDisplay` 2× render | 1440×900 | 1440×900 | 1440×848 | 2880×1800 |
| Fixed WindowServer capture | 1440×900 | 1440×900 | 1440×848 | 2880×1800 |

`constrainFrameRect` initially moves y=40 to y=56 above the Dock; it never
reduces width or height. The toolbar accounts for the content layout height,
while the full-size hosting view and frame view remain 1440×900.

The loss first appears in WindowServer's `kCGWindowBounds`: at the final
AppKit frame `(40,40,1440,900)`, its rectangle is `(54,761,1412,882)` in Quartz
coordinates. The actual AppKit frame in those coordinates is
`(40,752,1440,900)`. Thus the auto capture rectangle is inset by 14 points per
horizontal edge and 9 points per vertical edge, exactly the missing 28×18.

An independent AppKit-only window reproduced this without SwiftUI, a hosting
controller or a toolbar. With a null capture rectangle, ignore-framing plus
best-resolution returned 2824×1764; best-resolution alone included shadow and
returned 2916×1856. `frameRect(forContentRect:)` did not change the canvas or
fix auto capture. Explicit capture of the AppKit frame returned 2880×1800
with either image-option combination. See `window-probe.log`,
`window-probe-explicit.log` and `baseline-geometry.log`.

This proves the harness's assumption that automatic WindowServer framing
bounds equal the AppKit canvas is false on this OS. It does not establish
which macOS release first introduced that behavior.

## Fix and coordinates

`WindowCapture.image(of:)` now supplies `NSWindow.frame` explicitly to
`CGWindowListCreateImage`, converting the vertical origin using the primary
screen's height. It still captures only its own window at best resolution.
There is no resampling, padding operation, hardcoded inset or OS-specific
dimension exception. The window size and accepted PNG dimensions remain
unchanged. The primary display is `NSScreen.screens.first`, rather than the
display of the currently active window, as documented by
[Apple's NSScreen reference](https://developer.apple.com/documentation/appkit/nsscreen/screens).
The capture API accepts an explicit screen rectangle;
[Apple's capture reference](https://developer.apple.com/documentation/coregraphics/cgwindowlistcreateimage).

The full-size SwiftUI canvas remains 1440×900, so `ShapeCheck.Pixels` can still
use `image.width / 1440` with a zero-origin image. The original auto capture
had both a cropped origin and the wrong effective scale for those coordinates.
Diagnostics remain opt-in for future OS investigations.

For bounded pixel evidence, the material `cacheDisplay` home render is
byte-identical before and after the fix. A separate pixel reader locates the
primary capsule at the audited frame `[300,368.5171923828125,132,56]` using
scale 2. Its fill RGB is `[222,186,117]`, adjacent background `[95,74,44]`, and
top-leading corner `[74,61,39]`; the corner matches background more closely
than fill, as the capsule audit requires. See `baseline-pixel-alignment.json`
and `fixed-pixel-alignment.json`. The material image was inspected at original
resolution. This supports the unchanged AppKit coordinate canvas and material
button sampling, **not live Liquid Glass pixel alignment**: transparent
WindowServer output prevents that last measurement in this session.

`run.sh` also accepts `COD2_LAUNCHER_TEST_OUTPUT` and
`COD2_LAUNCHER_SNAPSHOT_OUTPUT`. The snapshot driver places its build beside
the requested capture directory. These allow private evidence and disposable
builds without capturing into the repository. Its docstring now records that
macOS 27 can return transparent captures while locked.

## Verification

The full suite was executed through a lease-owning Python wrapper with the
following command and environment, from this worktree:

```sh
evidence="$HOME/Library/Application Support/CoD2-native-current-readiness/WS104.noindex"
COD2_LAUNCHER_TEST_OUTPUT="$evidence/scratch/unit" \
COD2_LAUNCHER_SNAPSHOT_OUTPUT="$evidence/launcher-regression.noindex/snapshots" \
COD2_SNAPSHOT_GEOMETRY=1 \
taskpolicy -b nice -n 19 timeout -k 5 900 sh tests/launcher/run.sh
```

**Exit 1**, recorded in `launcher-regression-exit.txt` and
`launcher-regression.log`. Core/config/parser/key fixtures, actual local UDP
transport, synthetic artwork/cache fixtures, font verification, button-style
check and macOS 13–25 product-view typecheck all passed. The harness wrote 26
images and reported 200 shapes with zero findings. The first two image-size
and byte-length checks passed; the next check failed:

```python
assert shots["dark"] != shots["light"], screen  # AssertionError: home
```

An independent scan verified **all 26 PNGs are 2880×1800**, not just the first
pair; see `capture-summary.json`. All 26 also have an all-zero decompressed
RGBA stream. The baseline images had all-zero RGBA too, so the blank capture
predates this fix. `ioreg` reported `CGSSessionScreenIsLocked = True` and the
session on console, recorded in `session-state.txt`. Identical transparent
images explain the later appearance failure. File length exceeds 60 KB due to
PNG ancillary data, so byte length alone is not proof of rendered content.
The zero shape findings support reported layout geometry; their pixel-shape
portion cannot validate transparent captures. The full suite did not reach
the material-fallback assertions after the `home` failure.

Strict harness typechecking passed in **both** styling branches:

```sh
sdk=$(/usr/bin/xcrun --show-sdk-path)
for styling in material glass; do
    flags=()
    if test "$styling" = glass; then flags=(-DCOD2_LIQUID_GLASS); fi
    taskpolicy -b nice -n 19 /usr/bin/xcrun swiftc -typecheck \
        -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
        -target arm64-apple-macos13 -sdk "$sdk" \
        -import-objc-header launcher/NativeSetup.h "${flags[@]}" \
        launcher/Core.swift launcher/Presentation.swift launcher/Network.swift \
        launcher/Model.swift launcher/MapArtwork.swift launcher/Theme.swift \
        launcher/Artwork.swift launcher/RootView.swift launcher/HomeView.swift \
        launcher/ServersView.swift launcher/SettingsView.swift launcher/SetupView.swift \
        launcher/LibraryView.swift tests/launcher/Snapshots.swift
done
```

See `harness-typecheck.log`; both invocations exited zero. The optimized
snapshot build also passed. Supplemental bounded runs used:

```sh
taskpolicy -b nice -n 19 scripts/build-launcher.sh "$evidence/scratch/fixed" --snapshots
COD2_SNAPSHOT_GEOMETRY=1 taskpolicy -b nice -n 19 timeout -k 5 90 \
    "$evidence/scratch/fixed/LauncherSnapshots" "$evidence/fixed" --only home --scale2
COD2_SNAPSHOT_GEOMETRY=1 taskpolicy -b nice -n 19 timeout -k 5 90 \
    "$evidence/scratch/fixed/LauncherSnapshots" "$evidence/fallback" --only servers --fallback --scale2
```

Both runs exited zero: home reported 26 shapes, servers fallback 48, with no
findings. All eight PNGs, including both `cacheDisplay` appearances, are
2880×1800; see `supplemental-dimensions.json`, `fixed-geometry.log` and
`fallback-geometry.log`. Their WindowServer captures remain transparent, so
these exits do not replace the failed appearance/fallback pixel checks.
Python syntax, `sh -n tests/launcher/run.sh` and `git diff --check` passed.

## Preservation, cleanup and integration

```sh
git diff --quiet 664f1a1 -- src build/lp64_gen launcher scripts CMakeLists.txt
```

Exit zero, recorded in `source-preservation.txt`. There are **zero touched
engine files**, so the per-engine-file `unifdef -UCOD2_X64` plus `cmp` set is
empty. Native-OFF definitions and `COD2X_HOST_REVISION=0` are untouched.
The engine merge gate was not rerun for this test-only change. The shared
known-red gates remain visible and unchanged: legacy ABI has 10 differences
(`build/lp64_gen/data_native.c` in five configurations, excluded from OFF
builds, and test-only `tests/platform/server_control.c` in five configurations);
inactive gates has 436 differences across
intentional repairs and synthetic defined-zero configurations; the Linux
x86 multilib artifact comparison is unavailable on this Mac. These counts
come from the wave brief, not a new engine-gate run in this unit.

All compiles and heavy tests used `taskpolicy -b nice -n 19`, with no more than
two simultaneous compilation jobs from WS104. GUI runs were bounded by
`timeout -k`; the shared lease used exclusive file creation with WS104's PID
and timestamp, and was removed only after comparing the owned receipt.
No stale-lease takeover was needed. The screen remained locked; another
agent's `LauncherSnapshots` process was observed during the full-suite work.
No game or performance run was performed. No real player data, game assets,
TCC setting, global configuration, remote or other worktree was changed.
No `.app` bundle was created or registered.

External scratch build directories were removed after verification; the
evidence, captures, logs and standalone probe sources remain under WS104's
evidence directory. Nothing is intentionally left uncommitted in this
worktree. No push, tag, PR, merge or history rewrite occurred.

Cherry-pick the fix commit `e3e81a5` and this report commit into either
readiness or Config Studio lineage. The change is confined to the common
launcher harness. The orchestrator must carry the later locked-session
appearance failure as red: this branch establishes correct dimensions and
strict compilation, but does not establish an end-to-end green launcher
snapshot gate or live Liquid Glass pixel fidelity.
