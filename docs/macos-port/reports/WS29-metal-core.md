# WS29 — Metal backend core and interface freeze

Branch: `port/metal-core`; starting commit: `fe779a2`. The clear-only native
backend is implemented behind the existing D3D9 shim. GL remains the archived,
latched default. No game resources, shaders or geometry are rendered by this
slice. The exact integration contract is
[`metal-interfaces.md`](../metal-interfaces.md); its native C headers are ABI 1.

## Ownership and timeline as built

WS23 creates the device. Native presentation retains that exact device through
`MacMetalGPU_Create` and owns one Metal4 command queue, one shared event and one
preparation compiler. The producer and drawable worker borrow that transport.
There is no second native device or queue. GL interop presentation retains its
existing legacy queue and GL implementation.

`metal_gpu.m` has four allocator/command slots: two producer slots and two
presenter slots. Begin checks availability without waiting. Commit ends
encoding, commits residency, serializes queue operations under the transport
mutex, assigns a monotonic 64-bit value and signals the event. Drawable commits
schedule wait-for-drawable, commit, signal-drawable and present in SDK order.
GPU feedback releases allocations/retained objects and frees the allocator
slot exactly once after execution, including failed execution. Errors quarantine
the queue. The shared event is the fence and resource-retirement clock.

The core uses Metal4 only with the macOS 26 availability check, Metal4 GPU
family and successful object creation. The documented fallback selects GL
before scene creation; it does not build a second Metal3 game backend. Actual
older-OS execution was unavailable; SDK deployment/availability checks pass.

`metal_frame.m` owns a persistent, shared BGRA8Unorm scene target. Engine ARGB
clear values reach a Metal4 render clear. Present copies that scene into the
WS23 native mailbox after a conservative render/copy barrier, seals before
commit, retains both source/destination allocations even on no-clear frames,
and publishes only after completion. The worker independently obtains
drawables and presents the newest completed image. Neither slot acquisition nor
drawable acquisition waits in the client frame loop. Screenshots commit pending
encoding, drain and read the actual pre-gamma Metal texture; RGB and RGBA readback
are covered. A fresh producer slot preserves existing scene content after a
screenshot/fence commit; only target initialization or an engine color clear
requests another clear. Clear requests remain pending when all slots are busy.
Screenshot/lifecycle drains are intentionally blocking.

Resize cancels unsubmitted producer work, stops and joins the worker, drains
sealed/present work, replaces the layer/mailbox/scene target, and retains the
same device, queue and event through `MacMetalGPU_Retain` and
`MacMetal_CreateNativeWithGPU`. The backbuffer C identity survives; its dimensions
are refreshed. Fullscreen geometry-only changes preserve the target. Shutdown
stops native presentation before changing fullscreen state or destroying the
SDL window, drains callbacks, collects retired payloads and releases the last
transport owner. Only full renderer shutdown starts a new timeline epoch.

Native sidecars cover device, swapchain, surface, 2D/cube/volume texture,
vertex/index buffer, declaration, both shader stages, query and fence. C object
storage and original vtables remain the engine-facing identity. Payload release
callbacks retire by last-use value; producer notifications expose the exact
commit value to resource/draw providers. Reserved value 0 notifies cancellation
of an unsubmitted slot: providers release pending CPU references/ring
reservations without assigning a GPU last-use. A surface view holds its parent texture
while externally referenced. Event queries and APPLE fences use this timeline.
The registry/state/retirement queue are producer-thread APIs; GPU callbacks do
not mutate them.

## Dispatch and GL preservation

470 nonempty exported shim entries pass typed argument **addresses** through
the owning backend table. `shim_methods.json` records names/arguments/return
types, and `add_dispatch.py --check` verifies coverage. Object ownership is
pinned at construction independently of later selection. Coalesced weak hooks
let isolated, extracted GL fixtures link without the native backend. The strong
engine hooks select the GL continuation or Metal handler. Unsupported HRESULT
entries return NOTAVAILABLE; unsupported scalar/pointer getters return typed
zero. The generated HRESULT list prevents writing error bits into floats or
pointers. Empty compiler/container stubs have no operations to dispatch.

GL continues into its original typed method body. Native continuations into
original code are limited to audited CPU-only buffer/declaration/surface and
capability operations. Original GL draw/state/texture bodies were not rewritten.
Renderer fixtures and actual GL raster/mip/volume tests pass. An original-build
menu and controlled static Toujane screenshot match the changed GL build with
zero decoded RGBA bytes different. Dynamic FX/entities were disabled for the
golden: an earlier live comparison correctly showed animation differences and
was not used as exact-match evidence.

## Complete GL escape inventory

This inventory combines source searches across `src` with the actual native
compile database, rather than assuming that shim dispatch covers the engine.
All active engine/platform escapes are listed below. The runtime interposer
also covers dependency calls: 1,342 SDK/imported GL/CGL symbols, including all
126 engine imports. It aborts and records a backtrace on the first call, and
refuses an empty symbol set.

* `src/PC/gfx_d3d/r_screenshot.c`: `glActiveTextureARB`, `glColorMask`,
  `glDepthMask`, `glDisable`, `glGetError`, `glGetIntegerv`, `glPopAttrib`,
  `glPopClientAttrib`, `glPushAttrib`, `glPushClientAttrib`, `glReadBuffer`,
  `glReadPixels`. `R_GetFrontBufferData` returns native scene readback before
  any GL state work; `R_ClearGLErrors` returns immediately in Metal mode.
* `src/PC/gfx_d3d/rb_backend.c`: `glGenFencesAPPLE`, `glSetFenceAPPLE`,
  `glTestFenceAPPLE`, `glDeleteFencesAPPLE`. Every occurrence routes to native
  timeline hooks in Metal mode, including backend execution/frame completion.
* `src/PC/gfx_d3d/rb_shade.c`: `glBindTexture`, `glDisable`, `glEnable` in the
  hard-offset texture-name prebind at `RB_EndSurface`. The complete block is
  guarded in native mode; the D3D SetTexture operation still reaches dispatch.
* `src/PC/gfx_d3d/rb_state.c`: `glDrawBuffer`, `glReadBuffer` for back/AUX
  targets. Native guards bypass them; shim render-target state is retained.
* `src/platform/macos_display.c`: `CGLClearDrawable`, `CGLDisable`, `CGLEnable`,
  `CGLErrorString`, `CGLGetCurrentContext`, `CGLGetParameter`, `CGLSetParameter`;
  `glBindFramebufferEXT`, `glBindRenderbufferEXT`, `glBlitFramebufferEXT`,
  `glCheckFramebufferStatusEXT`, `glClear`, `glClearColor`, `glClientWaitSync`,
  `glColorMask`, `glDeleteFramebuffersEXT`, `glDeleteRenderbuffersEXT`,
  `glDeleteSync`, `glDisable`, `glDrawBuffer`, `glFenceSync`, `glFlush`,
  `glFramebufferRenderbufferEXT`, `glFramebufferTexture2DEXT`,
  `glGenFramebuffersEXT`, `glGenRenderbuffersEXT`, `glGetFloatv`,
  `glGetIntegerv`, `glGetString`, `glPopAttrib`, `glPushAttrib`, `glReadBuffer`,
  `glRenderbufferStorageEXT`, `glViewport`. `glVertexArrayParameteriAPPLE` is
  a local no-op definition. Native context creation bypasses all GL context,
  FBO, gamma and capability calls; present, size/mode/resize and shutdown take
  native branches. MacGL read/draw-buffer wrappers and AGL entry points are
  guarded. SDL GL escape APIs are `SDL_GL_ResetAttributes`, `SetAttribute`,
  `CreateContext`, `MakeCurrent`, `DeleteContext`, `GetDrawableSize`,
  `SetSwapInterval`, `GetSwapInterval`, `SwapWindow`: native code bypasses them
  and uses pixel-size/window APIs. The function named `SDL_GL_SwapWindowDirect`
  is our dispatch wrapper, not an SDL GL call in native mode.
* `src/platform/macos_gamma.h`: the GL-only shader/ramp/quad helpers call
  `glActiveTexture`, `glAttachShader`, `glBegin`, `glBindBuffer`, `glBindTexture`,
  `glCompileShader`, `glCopyTexSubImage2D`, `glCreateProgram`, `glCreateShader`,
  `glDeleteProgram`, `glDeleteShader`, `glDeleteTextures`, `glDisable`, `glEnd`,
  `glGenTextures`, `glGetIntegerv`, `glGetProgramInfoLog`, `glGetProgramiv`,
  `glGetShaderInfoLog`, `glGetShaderiv`, `glGetUniformLocation`, `glLinkProgram`,
  `glMultiTexCoord2f`, `glPixelStorei`, `glPolygonMode`, `glPopClientAttrib`,
  `glPushClientAttrib`, `glShaderSource`, `glTexImage1D`, `glTexImage2D`,
  `glTexParameteri`, `glUniform1i`, `glUseProgram`, `glVertex2f`, `glViewport`.
  This header stays unchanged; its create/present/release helpers are reachable
  only from the GL context branch.
* `src/platform/macos_metal_present.m`: WS23 GL interop calls
  `CGLTexImageIOSurface2D`, `CGLGetCurrentContext`, `CGLErrorString`,
  `glGenTextures`, `glBindTexture`, `glDeleteTextures`, `glGetIntegerv`,
  `glFenceSync`, `glClientWaitSync`, `glDeleteSync`, `glFlush`, `glFinish`.
  The existing `usesGL` split keeps these out of native creation, mailbox,
  worker and destruction; native presentation uses the shared Metal4 transport.
* `src/stubs/carbon_stubs.c`: `gluOrtho2D` calls `glOrtho`; native mode returns
  before that call.
* **SDL3/AppKit dependency:** SDL3 Cocoa `ScheduleContextUpdates`, called even
  with an empty SDL GL-context array, queries `+[NSOpenGLContext currentContext]`
  and triggers `CGLGetCurrentContext`. The first correctly populated interposer
  caught this during native window/layer updates. `MacMetalBackend_PrepareWindow`
  installs a public runtime block IMP once: native mode returns nil; GL mode
  calls the original IMP with its actual signature. No SDL private structures
  are accessed. The source was verified against
  [SDL3 release-3.4.16 Cocoa code](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.16/src/video/cocoa/SDL_cocoawindow.m).

Other search hits are accounted for, rather than silently ignored:

* The shim itself contains GL in `CDirect3DCubeTexture.c`, `CDirect3DDevice.c`,
  `CDirect3DPixelShader.c`, `CDirect3DSurface.c`, `CDirect3DTexture.c`,
  `CDirect3DVolumeTexture.c`, `COpenGL.c`, `COpenGLVAO.c`, `CVAOPacket.c`, and
  included `lp64_shader_draw.h` / `lp64_shader_state.h`. All nonempty exported
  callers are dispatched; static GL helpers are reached only from their GL
  continuation. `CVertexArrays.c` has GL declarations, not active GL work.
* `src/stubs/link_stubs.c` defines zero CGL compatibility functions; these are
  not calls into OpenGL. Its `CGLGetCurrentContext` stub is excluded on native
  Apple X64. `src/imports/libc.h` and `opengl.h` are declarations.
* The GL/window/readback/fog code in `src/stubs/macos_compat.c` is under its
  outer non-Apple-X64 guard; Linux/X11 alternatives are inactive here.
* `src/stubs/agl_stubs.c`, `src/web/webgl2_compat.c/.h`,
  `src/win32/gl_loader.c`, `win32_stubs.c`, and vendored Windows SDL GL/GLES
  headers are excluded from the native target. `Mac/Tools/MacDisplay.c` is
  excluded too and has no active GL call. These platform paths are untouched.

The no-GL smoke covers menu, Toujane initialization/play state, two JPEGs and
quit; it is not a proof that every future target/post operation is implemented.

## Interface freeze and next workstreams

The authority is `metal_abi.h`, `metal_core.h`, `metal_backend.h` and
`shim_dispatch.h`, with exact signatures/ownership in `metal-interfaces.md`.
Providers install copied ABI-1 tables before scene objects exist. This is the
only integration-owned seam; follow-up implementations belong in new Metal
files rather than shared engine headers. Method arguments are typed addresses.

* **Resources** start with sidecar creation/find/payload/bytes/view access,
  `MacMetalResourceOps`, explicit upload/lock/unlock, submission notification,
  retained-residency API and completion retirement. CPU mip/face/volume staging,
  buffer locks and declaration storage work. Implement native formats,
  compression/conversion, bounded partial locks/uploads, DISCARD/NOOVERWRITE
  rings, typed null resources and shared/private residency. No texture upload
  is claimed. Fresh surface-view binding transfers its reference to the parent.
* **States/PSOs** start with raw render/sampler/texture-stage/transform/viewport/
  target/stream/shader state, separate VS/PS banks, typed update callback and
  explicit prepare/ready-pipeline lookup. Normalize state and serialize full
  versioned key bytes; implement equality, caches, datasets and ready-only draw
  lookup. Only the original native present PSO is compiled during startup.
* **Shaders** start with source-retaining stage sidecars and the explicit
  prepare/load contract carrying name, source/metadata SHA-256, translator
  revision, stage, library path/entry and bounded reflection. WS24 translator
  output is local MSL source, not a metallib. Validate it and compile/load during
  loading. Implement declaration/interpolation/derivative/local/state/fixed-
  function details without compiling while drawing or bundling original content.
* **Draws** start with all four primitive/indexed/UP descriptors, primitive
  count/base/min/range/format fields, state, borrowed submission, ready-PSO
  lookup and submitted(value). Validate/copy UP bytes synchronously; byte sizes
  are zero until the provider calculates them. Build immutable tables/constants,
  retain referenced resources and encode passes/barriers. A null slot means
  drop/coalesce, never wait. Patch draws return NOTAVAILABLE.
* **Targets/post** start with the persistent scene, color clear, render/copy
  barrier, resize continuity and real pre-gamma readback. Implement depth/
  stencil/offscreen/masked/partial clears, attachments, copy/stretch, target
  changes and post/gamma hazards. Core clear is full scene color only. MetalFX,
  MSAA and EDR remain after classic parity.

The register ABI is exactly env[256], local[256], state[64], each aligned float4,
separate VS/PS banks. Constant bytes total 9,216, input 448. Raster buffer slots
are input/fetch 0, constants 1, streams 2..17; texture/sampler slots retain their
original numbers (16 each). The design needed one explicit correction: output
fields through depth occupy 208 bytes, but killed at 208 plus float4 alignment
requires **224-byte stride**, not a 208-byte struct. The design now records that
and the executed feedback/resize contracts. The translator's semantic and
raster libraries compile with its original layout; no original shader output
is committed. This is not shader numerical or raster parity proof.

## Evidence and reproduction

All builds used the worktree's own Release targets and existing installed tools;
builds/fixture compiles ran with `taskpolicy -b nice -n 19`. Full logs/build
outputs are ignored under `output/ws29`. Game screenshots and full game logs
are external under `~/Library/Application Support/CoD2-native-ws29/`. The
untouched original-build executable was copied locally before editing for the
GL golden; no reference/game binary was added to git.

The benchmark lock was held for captures/smokes. The screen was **locked**;
background system/app processes and low-priority compiler/Python audits were
present. These runs prove correctness, not FPS, compositor latency or the
3 ms timing budget. WS19 pacing acceptance still needs an unlocked, isolated
measurement. Structurally, the producer never obtains or waits for a drawable.

```sh
taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos --parallel 3
taskpolicy -b nice -n 19 cmake --build build-macos-codx --target cod2_macos --parallel 3
python3 tools/metal/add_dispatch.py --check
sh tests/metal/run_core.sh
ctest --test-dir build-macos/platform-tests --output-on-failure
ctest --test-dir build-macos/platform-tests-plain --output-on-failure
```

Both builds pass. Dispatch/core tests pass ASan/UBSan; platform tests pass
31/31 sanitized and 33/33 plain. Actual Metal4 GPU tests prove allocator
exhaustion/cancel/reuse and monotonic completion; exact NPOT clear bytes are
BGRA (151,73,29,255). Frame tests prove ARGB 0xff1d4997 → RGB (29,73,151),
resize 19×13 → 27×15 with identical transport/queue/event, then
ARGB 0xff258744 → RGBA (37,135,68,255). Invalid readback extents fail.
The frame fixture also writes synthetic GPU scene content (79,107,211,255)
independent of the engine clear, commits it through readback, then verifies
that Present and a no-clear draw submission preserve it. Resize and shutdown
each notify cancellation of an active unsubmitted slot exactly once.

With the shared lock held, the public smoke is reproducible as follows; all
output paths must remain external. Use the same command with `--renderer gl`
without the interposer, and repeat for `build-macos-codx/cod2_macos`.

```sh
python3 tools/metal/no_gl_guard.py build-macos/cod2_macos output/ws29/guard-all
DYLD_INSERT_LIBRARIES="$PWD/output/ws29/guard-all/no_gl_guard.dylib" \
  python3 tools/metal/smoke.py build-macos/cod2_macos --renderer metal \
  --home "$HOME/Library/Application Support/CoD2-native-ws29/frozen-stock-metal"
```

The final stock and CoD2x runs each pass on **both renderers**, reaching the main
menu and `CS_ACTIVE` in `devmap mp_toujane`, writing both JPEGs, and exiting 0
after scripted quit. Both Metal runs have the 1,342-symbol interposer active:
zero trapped GL/CGL calls. Their scene JPEGs are uniformly black, the current
engine clear; the nonblack color conversions are independently proved by the
actual GPU/frame tests above. Local result/log files are `frozen-stock-gl.log`,
`frozen-stock-metal.log`, `frozen-codx-gl.log` and `frozen-codx-metal.log`.
The final scene-preservation/cancellation changes repeat both trapped Metal
runs as `preserve-stock-metal.log` and `preserve-codx-metal.log`; no GL method
body changed in those fixes.

The untouched baseline versus final stock GL menu and static courtyard images
are each 1280×720 and have **0 / 3,686,400 decoded RGBA bytes different**,
maximum difference 0 and mean difference 0. The camera is fixed at
`setviewpos 2569 2274 181 180`, FOV 80, gamma 1, with HUD/gun/bob, FX and dynamic
entities disabled as in `smoke.py`. Comparison decodes both JPEGs with the same
CoreGraphics RGB context, then compares every byte. The original and final
captures remain external; the comparator/logs remain local and ignored.

Final OFF checks report 27 changed files across five include-free OFF
configurations with zero mismatches; all 27 actual SDK commands with X64 removed
also match with zero errors. The fixes13 legacy checker reports 28 changed
source files across ten configurations with zero mismatches. These counts refer
to existing changed sources, not newly added native-only translation units.

The complete ABI gates pass on the **final source and actual Release flags**:

```sh
taskpolicy -b nice -n 19 sh tools/abi/check.sh \
  build-macos/compile_commands.engine.json output/ws29/abi-stock-verified
taskpolicy -b nice -n 19 sh tools/abi/check.sh \
  build-macos-codx/compile_commands.engine.json output/ws29/abi-codx-verified
```

Both commands exit 0. Stock audits 626 translation units and CoD2x 643, each
with zero compiler errors, function mismatches and new findings. Each checks
218 renderer bindings with zero table/named-cast/unprototyped-floating-call
mismatches. Private import checks cover 515 import symbols and 185
placeholders: 1,777 native stock sites / 1,779 CoD2x sites, with zero proven
missing or extra dereferences. No ABI baseline was changed. Final logs are
`abi-stock-complete.log` and `abi-codx-complete.log`.

The engine databases were regenerated from CMake and checked to contain
`-DNDEBUG`, matching Release `flags.make`, before these final audits. Fixture
databases separately use `-UNDEBUG`. Earlier saved copies had inherited the
fixture assertion flag; those results were superseded rather than represented
as the Release merge gate. Normal source/header-digest cache reuse avoids
recompiling unchanged audit inputs; final changed Metal sources were re-audited.

## Full merge gate and remaining limits

Every command in CONTRIBUTING was invoked, including both platform builds,
private roundtrip/layout/fixes references, real GL raster/mip/volume fixtures,
and `tools/metal/run.sh`. After fixes and relevant reruns, the tools/datagen/
synthetic ABI/import/Wine-parser, sanitized CoD2x/full/native, SDK identity,
stock online, CoD2x fixes13, perf/game/script, 23 renderer fixtures, HUD/marks/
FX/dedicated fixtures, private data/reference and platform gates pass.
`tools/metal/run.sh` has the local cache: **417/417** translated semantic and
raster libraries compile, with 417 metadata files and zero failures. Output was
explicitly directed to this workstream's external translated cache.

The full gate is not all green; these unresolved items must survive the report:

1. Stock `tests/fixes13/run.py` reaches its timing extraction then cannot find
   `Cod2x_LimitedFPS`, which is preprocessed out without COD2_CODX. Its earlier
   receive/snapshot/ABI/switch/storage/table/hotpath/trajectory fixtures pass.
   CoD2x's complete fixes13 suite passes. No fixes13 file was changed.
2. CoD2x `tests/online/run.py` fails to link its timeout fixture, which lacks
   mocks for `Cod2x_Frame` and `Cod2x_DemoClientFrame`; preceding tests pass.
   This is the same fixture limitation recorded by WS24, not a Metal runtime
   failure. No online fixture was changed.
3. `check_inactive_gates.py` reports **6 mismatches** across 29 files / 17
   configurations: five production renderer/presenter files and the native
   mailbox fixture, each only in Apple arm64 / CoD2x-OFF. Native Metal must work
   in the stock build. All other 16 inactive configurations, including the
   OFF configurations, match. The checker is a CoD2x-only change gate; it is not
   applicable as an all-green requirement for this explicitly stock-native
   workstream. It was run, not silently omitted or altered.
4. `tools/ci/compare-x86.sh 410342a HEAD ...` returns its prerequisite error on
   this arm64 Mac: an x86_64 Linux host with multilib is required. No packages
   were installed. OFF preprocessor/SDK checks are supplemental evidence,
   **not** a claim that byte-identical i386 objects/binaries were measured here.
   The orchestrator must run that gate on the required host.

Availability syntax checks use the deployment target macOS 13 with both
unguarded-availability warnings as errors. The installed SDL compatibility
dylib reports a newer deployment target at link time; these local builds do
not establish older-OS runtime support. No shared `src/headers/*`, top-level
`CMakeLists.txt`, launcher, packaging, fixes13 sources, remotes or sibling
worktrees were changed. `clang-format` is not installed; no package was added.

## Merge handoff

Merge this branch's focused signed-off commits in order. Client-only CMake
sources live in `cmake/macos-arm64.cmake`; dedicated sources exclude Metal.
The integration-owned seams are confined to this module, shim preambles,
native display/presentation and the listed engine GL escapes. Parallel providers
should add their own files and install tables against ABI 1; coordinate only
their small CMake source additions with this branch. Keep GL default until
visual/resource/state/shader/target parity and the design's full-frame gates
pass. The clear-only slice does not satisfy those future parity gates.

No pushes, PRs/issues, remote changes, system packages, proprietary content or
translated/extracted shaders were committed. Two unrelated untracked probe files
appeared during the session and were left untouched and excluded from commits.
