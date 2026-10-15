# WS23 — Metal presentation and independent client pacing

Branch `port/metal-present`, based on `3ef770b`. All source, builds, homes,
private evidence and test bundles belong to this worktree. No sibling worktree
was read or written, no packages installed, no remotes changed, and nothing was
pushed or published. Process inventories can show sibling executable names;
those processes were left alone.

## Design and integration

`r_present gl|metal` is an archived, renderer-latched enum. Use it at launch or
apply it with `vid_restart`. **GL remains the default.** `r_presentEDR` is an
archived, latched boolean, default 0. `r_presentMode` remains the independent
WS19 GL fence experiment; Metal does not use it. The GL presentation branch is
retained. New dvars are registered in nine native-only lines in `r_dvars.c`.
There are no changes to `src/headers/*`, top-level `CMakeLists.txt`, packaging,
original shaders, the client cap, physics, networking or raw mouse code.

The presenter has four BGRA8 IOSurfaces. Each is a CGL rectangle texture and an
`MTLTexture` over the same storage. The scene FBO's colour attachment rotates
through these textures; the two AUX renderbuffers and depth/stencil remain GL.
The original colour renderbuffer is a scratch target when every shared slot is
busy. Screenshots read the current scene before presentation, as before.
There is no extra GL copy into the drawable in Metal mode. The screen CGL
context is retained for FBO rendering and its drawable is cleared.

Ownership is bounded: FREE → RENDERING → GL_PENDING → READY → METAL → FREE.
The producer issues a fence and explicitly flushes GL, then publishes READY
only after `glClientWaitSync(..., 0, 0)` reports completion. Metal completion
releases a slot only after sampling its texture has finished. No GPU fence wait,
drawable acquisition, or presenter queue wait is performed by the client.
Older READY slots can be reclaimed; the newest completed slot is retained under
occlusion. The presenter acquires its drawable first and then selects the newest
READY image. A submitted sequence watermark prevents showing an older image
later, including when the producer pauses. Failed fences quarantine storage.

Apple's [OpenGL/Metal interop sample](https://developer.apple.com/documentation/metal/mixing-metal-and-opengl-rendering-in-a-view)
requires an explicit GL flush before Metal reads shared pixel storage. The
installed `OpenGL.framework/Headers/CGLIOSurface.h` also documents flush/bind
visibility rules. WS23 adds a zero-timeout completion check and explicit
exclusive storage ownership rather than relying on flush alone. GPU completion
and image orientation are exercised with real CGL/IOSurface/Metal pixels.

On macOS 14+, `CAMetalDisplayLink` runs on a separate pthread/run loop and
supplies drawables to its delegate. Its target and presentation timestamps are
observed independently of the 3 ms client loop. The default frame-rate range
lets the OS choose display cadence; no hard-coded 60 Hz client deadline is
introduced. ProMotion/VRR hardware has not been tested here. macOS 13 has a
worker-only `nextDrawable` path with coalesced wakeups.

The layer uses two drawables, `displaySyncEnabled = NO`,
`presentsWithTransaction = NO`, `framebufferOnly = YES`, and
`allowsNextDrawableTimeout = YES`. **Timeout does not mean nonblocking**: the
SDK explicitly says `nextDrawable` can wait one second. That call exists only
on the worker, never the client. Hidden/locked windows can stop yielding
updates/drawables without stopping scene rendering. This does not guarantee
that rendering or the required `glFlush` always fits 3 ms; those are separately
measured costs.

The layer backing always equals the selected render size. An aspect-preserving
layer frame scales into the window view, with black bars and the actual Retina
backing scale. Input uses the same view-pixel aspect rectangle. Resize rebuilds
resources only during the display-mode lifecycle. Shutdown stops/joins the
worker, invalidates the display link, drains Metal work, finishes outstanding
GL work, removes the layer, then destroys textures/surfaces before the GL
context and SDL window. The blocking drains are lifecycle operations.
Teardown also calls Cocoa's GL context `update`, restoring its existing drawable
so a failed presenter rebuild can actually fall back to GL. A private probe
verified that `CGLClearDrawable` leaves default FBO status `0x8219` after Metal
teardown and even after `SDL_GL_MakeCurrent`; Cocoa `update` restores `0x8cd5`.
The regression test failed before the repair and passes afterward.

The overlay view returns nil from hit testing, preserving SDL focus/input
ownership. Metal requests Spaces by default unless the environment explicitly
sets `SDL_VIDEO_MAC_FULLSCREEN_SPACES=0`; the existing desktop-mode promotion
avoids SDL's exclusive-mode path for Spaces. GL keeps its existing default.

Classic output is BGRA8Unorm with nil layer colorspace: no extra sRGB encoding,
tone mapping or color matching. The original presentation shader is written
for this workstream; it contains no extracted/translated game shaders. Its
integer texture reads flip GL's origin and its gamma lookup follows
`macos_gamma.h`. EDR opts into RGBA16Float, extended-linear-sRGB and
`wantsExtendedDynamicRangeContent`; the shader decodes existing SDR values to
linear light. No HDR scene values, tone mapper or EDR metadata are added.

### Interface for WS24

`src/platform/macos_metal_present.h` is the C boundary. `MacMetal_CreateNative`
creates the same presenter without making any GL calls; use it with the SDL
Cocoa game window. `MacMetal_Device`/`MacMetal_Layer` return borrowed Objective-C
objects. `MacMetal_BeginNativeFrame` returns a borrowed renderable BGRA8 Metal
texture plus a token, or NULL immediately when storage is busy.

For an `MTLCommandBuffer`, call `MacMetal_EndNativeFrame` before committing the
rendering buffer. For Metal 4, call `MacMetal_SealNativeFrame` before submission
and `MacMetal_CompleteNativeFrame` from a GPU shared-event completion listener.
Every successful Seal requires a completion, including cancelled/failed work.
The callback must mean GPU writes have finished; CPU encoding completion is
insufficient. These queue-independent hooks let WS24 reuse presentation with
Metal 4 without pretending its command buffers implement the older protocol.
The native texture origin is top-left; interop uses GL's bottom-left origin.
Create/update/destroy are main-thread lifecycle calls. One producer owns frame
acquisition/sealing. Presentation owns its worker and drawable acquisition.

## Verification and current limitations

Private evidence is under ignored `output/ws23/`; preserve it before removing
this worktree. No screenshots, demos, Apple sample downloads, raw logs or cache
contents are committed. The original game data and shader cache are read-only.

Both arm64 Release builds pass, with assertions enabled separately in fixture
compile databases:

```sh
taskpolicy -b nice -n 19 cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos --parallel 3
taskpolicy -b nice -n 19 cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-macos-codx --target cod2_macos --parallel 3
```

The synthetic Metal test exercises actual shared pixels, GL-to-Metal vertical
orientation, gamma, exact render size, hidden-window rendering, slot exhaustion,
stale-frame rejection, native completion/cancellation and default byte parity.
It also verifies EDR layer/format configuration. EDR appearance/headroom on an
HDR display and the macOS 13 fallback are not runtime-verified on this host.

The complete CoD2x `tests/fixes13` suite passes unchanged, including 1,000 frames
at each cap: 125→8 ms, 250→4 ms, 333→3 ms and limited 333→4 ms. No fixtures in
that suite were edited. Two inherited gate harness failures are retained:

- Stock `tests/fixes13/run.py` cannot extract `Cod2x_LimitedFPS` with CoD2x off.
  `--baseline 3ef770b` reproduces the same failure after the eight preceding
  fixtures pass. The function is feature-gated in the unchanged source.
- CoD2x `tests/online/run.py` stops linking the timeout fixture because it has no
  `Cod2x_Frame`/`Cod2x_DemoClientFrame` stubs. Those fixture and engine files are
  unchanged. An ignored supplementary database with those two stubs gets past
  timeout but then exposes another missing fixture link, `Cod2x_CDKeyHash`.
  The stock online suite passes. This is not counted as a CoD2x online-suite pass.

The first tools-suite run had an EPERM cleanup failure in its synthetic timeout
case; the full immediate rerun passes 14/14. Both outcomes remain in the logs.
The full legacy check gives 4 files × 5 X64-off configurations, 0 mismatches;
two actual SDK preprocessing commands with X64 removed also report 0
mismatches/errors. The observer's extensions are explicitly arm64/macOS native;
its original GL body remains verbatim outside that guard. Both observer variants
compile; only the native variant exports the Metal hooks. This is preprocessing
parity, not a fresh linked-i386 artifact comparison. A real
`xcrun clang -arch i386`
link probe fails with `ld: linking for i386 is no longer supported`. No replacement
toolchain was installed; the orchestrator must run the Linux multilib object/
binary comparison. Typed-data private round trip and scalar/layout checks pass.

### Merge-gate evidence

Every command in CONTRIBUTING's native merge gate was invoked. The CPU-only
runner's command/exit-code list is `output/ws23/gate/results.json`; numbered
logs map to that list. Final builds are `build-stock-final4.log` and
`build-codx-final4.log`. Platform and real-GL fixture logs have descriptive names.
The source databases used `-UNDEBUG` for fixtures and separate untouched Release
`compile_commands.engine.json` copies for ABI checking. Builds/fixture compiles
used background scheduling and nice 19.

Passing groups include tools tests (14/14 on rerun), datagen/checker unit tests,
shader manifest/trace parsing, CoD2x sanitizer/full/native suites, SDK identity,
stock online fixtures, complete CoD2x fixes13, performance source fixtures,
LP64 game/script/renderer suites, all HUD/impact/FX/dedicated fixtures, private
round trip, scalar layouts and the Mac fixes13 reference. Real CGL shader raster,
texture mip and volume uploads pass. Platform CTest passes with sanitizers
28/28 and without sanitizers 30/30; both run the new Metal tests. The lifecycle
repair has fresh final sanitized 28/28 and plain 30/30 runs.

Reproduction commands for the new functionality:

```sh
ctest --test-dir build-macos/platform-tests --output-on-failure
ctest --test-dir build-macos/platform-tests-plain --output-on-failure
ctest --test-dir build-macos/platform-tests-plain -R '^metal_' --output-on-failure
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/ws23/gate/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/ws23/gate/abi-codx
python3 tools/abi/legacy.py --base 3ef770b \
  --compile-commands build-macos/compile_commands.engine.json
```

The supplementary `tests/cod2x/check_inactive_gates.py --base 3ef770b` reports
three differences in its `arm64-codx-off` configuration: display C/header and
observer. This checker is for changes that should vanish when CoD2x is off.
WS23 intentionally adds presentation to the stock native client too, so these
changes must remain with CoD2x off. All X64-off configurations still match.
The checker was not changed or counted as passing.

The first full ABI invocations were under a 1,800-second wrapper limit and
expired with incomplete function reports during heavy concurrent auditing.
They are retained as exit 124 in the initial gate ledger. Recovery uses the
original unmodified `tools/abi/check.sh` without that limit. The full stock
function audit reports 621 TUs, zero errors/mismatches; callback audit reports
218 bindings, zero table/named-cast mismatches or unprototyped floating calls.
Both import checks report 514 symbols, zero proven missing/extra dereferences.
Both complete `check.sh` invocations exit 0, including their import steps.
The full CoD2x function audit reports 638 TUs, zero errors/mismatches, and its
callback audit also reports 218 bindings with zero mismatches. Final logs are
`output/ws23/gate/abi-stock-final.log` and `abi-codx-final.log`.

Recovery cache reuse is documented in private `cache-provenance.json` and
`cache-stock-codx-proof.json`: each old entry matches the exact current source
bytes/compiler arguments and a fresh dependency file excludes the only changed
header; stock-to-CoD2x reuse additionally requires identical actual Clang
preprocessed output and all non-feature compiler arguments. No checker, baseline,
or finding was changed. New/affected platform sources and shim providers also
pass a fresh seven-entry check with zero errors/mismatches. Checking only three
platform entries without the providers produced declaration-only false findings;
that diagnostic is not counted as a full ABI failure.

### GL versus Metal and Game Mode

The reduced comparison held PLAN's benchmark lock across all four live runs,
four timedemos and the owned Game Mode probe. Every before/after snapshot says
**screen locked**. The per-run process snapshots contain 43–46 Python processes
(other agents' audits/builds plus this agent's checks), and no pre-existing
`cod2_macos` game process at each live-run launch. These are exploratory results,
not quiet-machine acceptance. GL remains the default.

| Live 333 cap, 20 s, locked | Client mean FPS | Client 1% low FPS | Client P99 ms | GL swap mean/max ms | GL flush mean/max ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| GL 1920×1080 | 330.3 | 166.0 | 4.011 | 0.266 / 5.541 | 0 / 0 |
| Metal 1920×1080 | 332.2 | 241.8 | 3.031 | 0 / 0 | 0.075 / 0.860 |
| GL 2560×1440 | 328.7 | 175.9 | 4.449 | 0.334 / 31.802 | 0 / 0 |
| Metal 2560×1440 | 329.0 | 206.2 | 4.560 | 0 / 0 | 0.118 / 0.749 |

All are `r_presentMode=0`, Spaces=0, EDR=0, exact selected render backing and
clean `quit`. 1080p improves in this sample; 1440p is mixed and neither backend
meets the unlocked 1% low target. No default change is justified. Timedemo 1%
lows (GL/Metal) are 266.0/77.4 at 1080p and 231.5/238.1 at 1440p; one short run
per configuration under this CPU load is not a reliable ranking.

Presentation clocks remain separate. GL's swap-return proxy reports 330.3 and
328.8 Hz. Metal has **zero positive `CAMetalDrawable.presentedTime` samples**:
there is no measured physical presentation rate on the locked screen. It submits
59/60 frames during the two 20-second runs and records 60 display-link updates
each. Display-link target gaps range from 16.667 to 999.998 ms (mean 333.333 ms),
consistent with occlusion throttling; these gaps do not establish unlocked
60 Hz pacing. Rendering, screenshot capture and shutdown continue without a
drawable deadlock. SDL event-pump maxima remain 6.620/4.593 ms in Metal mode.

`tools/macos/check-gamemode.sh` recognizes the owned Metal client (PID 61820)
through LSSupportsGameMode, but logs session states `paused, off`. Actual Spaces
request flags are `0x6` with a 1920×972 view and exact 1920×1080 render backing:
no fullscreen/input-focus bits while locked. The app quits cleanly. Recognition
and locked mechanics are verified; active Game Mode is not.

Both synthetic Metal fullscreen lifecycle tests pass. With Spaces disabled,
SDL reports flags `0x7`; with Spaces enabled it reports `0x6` in this locked
session (no actual fullscreen/input-focus bits). The tests verify the requested
1920×1080 backing, last-pixel rendering, minimize/restore, 2560×1440 backing,
return to 800×600 windowed mode and physical display-mode restoration. They do
not claim an accepted native fullscreen transition, frontmost raw-mouse
behavior, Cmd-Tab acceptance or active Game Mode. Those need an unlocked session.
The view forwards hit testing to SDL; raw mouse code remains unchanged.

Synthetic output compares GL and Metal byte-for-byte, including a nontrivial
RGB gamma ramp, and verifies orientation and default layer configuration. It
is not a screenshot of the physical composited display. Toujane
`screenshotJPEG ws13-live` succeeds on both backends at both resolutions
and in the Game Mode probe. The 1080p JPEG pair was visually inspected: material,
color, HUD and orientation agree; the combat-driven camera timing differs, so
the captures are not byte-identical. These JPEGs read the scene FBO. Physical
layer color matching still needs on-screen captures on the unlocked Mac.
EDR/ProMotion/VRR hardware
and the macOS 13 worker fallback remain unverified.

Private matrix evidence: `output/ws23/matrix-final-code/summary.json`, individual
`results.json` files, `benchmark-lock.txt` and `gamemode/game-policy.log`.
The app used the final `ed32a5c` presentation code. No game captures/logs are
committed. Built-in `sips` verifies JPEG dimensions; Pillow was unavailable and
no package was installed. The validator released its owned lock and unregistered
its test bundle; no foreign game/lock was stopped.

### Unlocked validation

Run from this worktree on a quiet, unlocked Mac. The validator takes/releases
PLAN's shared lock itself. It creates/unregisters only a private test bundle,
uses separate writable homes and captures `screenshotJPEG` in every live run.
It records client intervals, `CAMetalDrawable.presentedTime` intervals and
CAMetalDisplayLink target cadence independently. GL's presented interval is
explicitly a swap-return proxy, not a physical display timestamp.

```sh
COD2_PRESENT_BACKENDS='gl metal' COD2_SPACES_MODES='0 1' \
  COD2_VALIDATE_OUT="$PWD/output/ws23/unlocked-full" \
  tools/macos/validate-333.sh
```

This runs the original 3 resolutions × 2 caps × 3 repeats for each backend and
Spaces setting, 120-second live combat plus timedemo, then an owned Metal
`tools/macos/check-gamemode.sh` session. The machine-state snapshots must show
unlocked before and after, focus/fullscreen flags and no view changes; document
any remaining background jobs. Do not change the default based on locked data.

The reduced exploratory invocation used here is:

```sh
COD2_BINARY="$PWD/build-macos-codx/cod2_macos" \
  COD2_PRESENT_BACKENDS='gl metal' \
  COD2_VALIDATE_RESOLUTIONS='1920x1080 2560x1440' \
  COD2_VALIDATE_CAPS=333 COD2_VALIDATE_REPEATS=1 COD2_VALIDATE_SECONDS=20 \
  COD2_ALLOW_LOCKED=1 COD2_VALIDATE_OUT="$PWD/output/ws23/matrix-final-code" \
  tools/macos/validate-333.sh
```

For a separate Game Mode repeat (own the lock for the whole session):

```sh
python3 tools/cod2x/make_macos_app.py build-macos-codx/cod2_macos \
  "$PWD/output/ws23/Game Mode Test.app" --bundle-id org.opencod2.ws23.gamemode \
  --game-dir "$HOME/Games/CoD2"
ws23_lock="$HOME/Projects/cod2-native-wt/_agents/bench.lock"
while ! mkdir "$ws23_lock" 2>/dev/null; do sleep 60; done
trap 'rmdir "$ws23_lock"' EXIT
COD2_GAMEMODE_PRESENT=metal tools/macos/check-gamemode.sh \
  "$PWD/output/ws23/Game Mode Test.app" "$PWD/output/ws23/gamemode-unlocked"
```

While frontmost in native fullscreen, verify raw mouse, Cmd-Tab out/in and
clean quit/display restoration. Compare default (EDR=0) output captures at the
same view/gamma/render size; scene JPEGs alone do not prove layer color matching.
Run the Linux multilib `bash tools/ci/compare-x86.sh 410342a HEAD output/x86-reference`
before claiming byte-identical i386 objects/binaries.

### Merge notes

Merge the six implementation commits on `port/metal-present`:
`0176a44` (presenter), `846bcb0` (monotonic sequence), `63049c8` (observer/matrix),
`8719b3a` (native completion/pixel tests), `763ef8c` (legacy observer guards),
`ed32a5c` (GL fallback repair), plus this report commit. Keep the native-only dvar
registration in `r_dvars.c`, four small client-only CMake additions and the
platform test linkage. Nothing in `src/headers/*`, top-level CMake or packaging
changed; the dedicated target acquires no Metal dependency. WS24 should consume
`macos_metal_present.h`, with GPU completion and lifecycle ownership as described
above. Native consumers must provide a ramp when identity is false and complete
every sealed token before destroying the presenter. Preserve both presentation
paths and leave EDR off/GL default until the outstanding unlocked evidence is
collected.

A named `WS23 unused SDK declaration experiment` stash preserves an abandoned
header-isolation diagnostic. It is not part of the branch and must not be applied
for this merge. Automatic approval review rejected deleting the stash because
that operation permanently deletes the backup; the tested source remains clean.
