# WS33 — Metal draws and integration

Branch: `port/metal-draws`. Integration base:
`94ee40d7d460d7ccb06947157fa87dbdbf654d8d`.

## Status

The native Metal provider renders the original programmable menu, HUD and
textured/lightmapped Toujane, including sky. Cold-cache setup prepares the
verified original shader libraries and material pipelines before drawing.
The original GL renderer remains the default.

**This branch is not certified for the full WS33 acceptance gate.** Matched
lossless camera receipts and deterministic scene replay are absent; the
WS34 strict comparator correctly refuses the exploratory captures. Combat
coverage and leak results are recorded below without treating commands sent
as proof of effects observed. Two inherited source-fixture failures and the
unavailable Linux multilib artifact comparison also remain.

No frozen ABI 1 header changed. No original shader, compiled shader archive,
capture, demo, game asset or reference dump is committed. All licensed runtime
evidence is under `~/Library/Application Support/CoD2-native-ws33/`; original
shader inputs remain under `~/Library/Application Support/CoD2 Silicon/shaders/`.

## Provider composition and lifetime

`MacMetalIntegration_Bootstrap`, called before shim objects are created,
installs WS30 ResourceOps through WS34 `MacMetalTargets_Install`, with a local
borrowed attachment resolver for WS30 render-target textures and their level
surfaces. It then installs WS31's default states provider and WS33 DrawOps.
The build includes `metal_states_gpu.mm`, the draw contract, draw provider,
material enrollment and integration sources only in the native arm64 target.

`MacMetalIntegration_DeviceCreated` prepares WS32's shader provider using
`COD2_MAC_SHADER_CACHE`, verifies and loads its 417 original programs, derives
the state cache namespace from the verified translated manifest, and appends
`-ws33-fetch1` for the vertex-fetch interface. Shader associations use the
actual original `#COD2CTAB:` names (`.vc`/`.pc` to `.vsa`/`.pse`), with basename
validation. A local `AssociateReady` API attaches an already verified library
after the shader factory window closes. ABI 1 `Load` still rejects loading
while sealed; association itself cannot compile.

Native hooks at the end of `CL_InitUI` and `R_EndRegistration` reopen the
explicit loading window, enroll the registered material techniques, publish
ready states, then seal both shader and state factories. Enrollment runs the
engine's actual 11-rule state-map decoder and `RB_ChangeState_0/1`, iterates
DX9 declaration variants and producer strides 20/24/32/36/64/68, and prepares
BGRA/RGBA targets with and without scene depth. It restores both shim state
and `dxState`. All 128 engine sampler-flag combinations are prepared through
the actual sampler decoder. The blend prewarm transition explicitly crosses
the zero/nonzero operation boundary required by the engine's blend-enable
cache; complementing the bits alone was insufficient.

Draw-time recipes are retained as bounded, validated local JSON under the
external Metal cache root, never compiled during drawing. A subsequent load
renormalizes stored state and checks shader identities before enrollment.
`COD2_METAL_CACHE_ROOT` selects a private external root; the default is in
Application Support. Up to 1,024 observed recipes and a 64 MiB input limit are
enforced. Pointer bindings are not replayed from disk.

Submission transfers coalesced COM references and native GPU resources to
the existing retirement path. After the core drains submissions and releases
the last shim object, the local release hook saves recipes, cancels remaining
pending references, uninstalls/destroys states and shuts down resources.
Weak default hook definitions preserve standalone provider fixtures; the
engine links the strong integration definitions.

Native object and backend-ownership lookups now use raw-pointer CoreFoundation
indexes, with the existing linked lists retaining ownership and teardown.
A loading-process sample attributed much of the repeated material cost to
linear `MacMetal_FindSidecar` and shim dispatch searches. Removed keys are
unregistered before objects can reuse an address; the indexes release when
the last object disappears. Allocation failure keeps the list fallback for
that object lifetime. All index code is native guarded. Synthetic fixtures
exercise 2,048 pinned backends, selection changes, unregister/reuse and 2,048
released sidecars. SDK dictionary calls are isolated behind a local opaque
helper, so reconstructed engine headers do not acquire SDK declarations.

The states provider also shares an identical prepared PSO across buffer-fetch
layout keys. ABI 1 keys still include the full declaration/stride/color layout;
only the internal factory cache removes layout fields when hardware vertex
fetch is disabled. Those fields are consumed by the per-draw fetch descriptor,
not by the actual Metal pipeline descriptor. Hardware fetch, shaders, strict
math, target formats, topology, blending and specialization remain in the
factory identity. The added GPU assertion fails with the prior provider (12
layout PSOs) and passes with four target-signature PSOs for all three packed
color layouts; pixel, archive and factory-prohibition fixtures still pass.

## Draw path

All four ABI descriptors are implemented: primitive, indexed, UP and
indexed-UP. The contract validates topology, primitive-to-element count,
first index/vertex, u16/u32 format, signed base vertex, minimum/range, overflow,
declaration types, per-stream offsets/strides and the exact consumed bounds.
Sparse indexed draws reserve contiguous runs of actually referenced vertices,
including signed base conversion, rather than occupying holes between minimum
and maximum indices. Full declaration/range validation remains enforced.
It rejects malformed descriptors with `INVALIDCALL`, and patch/unsupported
topologies with `NOTAVAILABLE`. Triangle fans are converted to retained u32
indices. Indexed draws add the signed base once. Null submission slots drop
immediately without waiting.

Vertex fetch uses the original generic register mapping and the WS32 fetch
wrapper, with producer-specific packed RGBA/BGRA/ARGB color interpretation.
Each draw copies the 736-byte fetch descriptor and separate immutable
9,216-byte VS/PS register snapshots. Argument tables bind fetch at buffer 0,
constants at 1 and streams at 2–17, with 16 texture/sampler slots. UP vertices
and indices are copied before return. Only shader-used texture slots receive
typed null textures. Sampler LOD bias stays in the shader descriptor; ready
sampler lookup uses zero bias to avoid clamping the engine's full float bias.

PSO, depth/stencil and sampler lookup is ready-only. The draw provider has no
compiler/factory fallback. Draw diagnostics separately count encoded,
null-slot dropped, unready and otherwise rejected descriptors. Original
shader metadata and actual attachment signatures determine the pipeline key.
WS34 `BeginDraw`/`EndDraw` owns pass encoding and barriers. Active-target
sampling is rejected; parent texture aliases are checked as well. Sampling
a CPU-modified render target first flushes its WS30 shadow upload.

Culling follows the GL shim's actual CW/CCW convention. Viewport, scissor,
wireframe, stencil reference, blend constant and raster depth bias are set
per draw. D3D's normalized D24 constant bias is converted by 2^24 as in GL;
the remaining D24-versus-float depth/decal parity requires matched captures.

Two resource fixes were necessary for the engine's real command stream:

* Nonoverlapping nonzero DISCARD append locks reuse the current backing
  buffer; offset-zero and overlapping writes still rename. The engine uses
  DISCARD for sequential appends with `r_gpuSync=0`; renaming each append
  exhausted WS30's three generations on the fourth write. GPU-read ranges
  still prevent overwriting live bytes.
* Fresh GPU render-target textures begin without a dirty CPU shadow, so
  sampling them cannot upload zeroes over previously rendered contents.

A combat run exposed a separate native engine overflow bug in `rb_tess.c`:
quad/line producers saved the 32-bit `declType` through `void **`, restoring
the adjacent stale `indexCount` on arm64 after flushing 5,450 vertices. The
original retail quad producer loads/stores four bytes at `declType`; the next
four bytes are the separate count. Native save/restore now uses the enum.
The inactive macro expansion preserves the original source exactly. The
ASan/UBSan overflow fixture extracts all four actual DX9/DX7 quad/line
producers: it fails with the prior code and passes with four vertices/six
indices after each flush. This fixes malformed grenade/smoke FX descriptors
without relaxing Metal's index validation.

## Reproduction and runtime evidence

Always check `pgrep -fl cod2_macos`, use a fresh external home and free unique
port, and run under `timeout -k`. The following stock smoke reached
`CS_ACTIVE`, captured the menu and Toujane and exited 0 with a fresh state
cache. It had zero unready draw messages:

```sh
python3 tools/metal/no_gl_guard.py build-macos/cod2_macos output/ws33/no-gl
DYLD_INSERT_LIBRARIES="$PWD/output/ws33/no-gl/no_gl_guard.dylib" \
COD2_METAL_CACHE_ROOT="$HOME/Library/Application Support/CoD2-native-ws33/cache-cold-fixed" \
timeout -k 10 300 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer metal \
  --home "$HOME/Library/Application Support/CoD2-native-ws33/metal-cold-fixed" \
  --port 29840
```

The generated GL/CGL interposer covered 1,342 SDK symbols and all 126 engine
GL imports. No trapped GL/CGL call occurred on Metal. This is a fresh-cache
run: UI prepared 3,226 recipes; Toujane prepared 35,812; each loading phase
prepared 128 sampler flags, rejected zero state maps and published successfully.
`gl-cull` and `metal-cull` earlier stock smoke homes also exited 0 on Toujane
(ports 29838/29837 respectively). The final stock smokes on the latest binary
also exited 0 at ports 29880/29881, under `final-smoke-metal` / `final-smoke-gl`. The Metal run used
the same GL/CGL trap and logged 106,100 encoded / 59,072 null-slot dropped /
0 unready / 0 rejected draws. Both final courtyard images visibly contain the
textured world; they remain uncontrolled JPEG diagnostics. No DX7 material
path is claimed.

## Golden gate

```sh
COD2_METAL_CACHE_ROOT="$HOME/Library/Application Support/CoD2-native-ws33/cache-cold-fixed" \
timeout -k 10 750 python3 tests/metal/goldens.py capture build-macos/cod2_macos \
  --base "$HOME/Games/CoD2" \
  --cache "$HOME/Library/Application Support/CoD2 Silicon/shaders" \
  --output "$HOME/Library/Application Support/CoD2-native-ws33/camera-smoke" \
  --smoke-only
```

All nine lossless images were written, all three processes exited 0 and
Metal had zero unready messages. This is capture smoke, **not a golden pass**.
The actual logged eye Z positions differed from the recipe and between runs:

| View | Requested eye Z | GL A | GL B | Metal | Strict gate |
| --- | ---: | ---: | ---: | ---: | --- |
| Courtyard | 182 | 122 | 55 | -51 | FAIL: unmatched pose/time, no receipt |
| Rooftop | 221 | 158 | 96 | 2 | FAIL: unmatched pose/time, no receipt |
| Sky | 351 | 170 | -28 | -401 | FAIL: unmatched pose/time, no receipt |

Raw exploratory RGB metrics below are in byte units (255 times the normalized
JSON metrics). They compare different scenes and do not measure backend parity.

| View | Pair | MAE | RMSE | p99 | Pixels >16 | Pixel gate |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| Courtyard | GL repeat | 120.84 | 145.19 | 233 | 84.513% | FAIL |
| Courtyard | GL/Metal | 134.20 | 152.21 | 233 | 93.853% | FAIL |
| Rooftop | GL repeat | 104.18 | 126.82 | 216 | 86.193% | FAIL |
| Rooftop | GL/Metal | 112.99 | 131.84 | 217 | 91.919% | FAIL |
| Sky | GL repeat | 63.44 | 87.47 | 194 | 68.657% | FAIL |
| Sky | GL/Metal | 48.85 | 73.19 | 169 | 57.916% | FAIL |

The strict comparator rejects these manifests with “Missing strict capture
manifest; smoke captures are not parity evidence.” No requested values were
copied into an observed receipt. An earlier uncontrolled courtyard JPEG pair
had MAE 0.118, RMSE 0.703, p99 2 and 0.0215% outliers; JPEG and missing matched
state make that merely a visual diagnostic, not acceptance. Menu JPEG comparison
failed RMSE/outlier gates, with different cursor positions. No exclusion masks
or weakened thresholds were used.

Still needed: the common engine screenshot/frame receipt seam described in
`metal-goldens.md`, deterministic simulation/animation/refdef replay, exact
noclip pose verification, then strict GL repeatability and GL/Metal comparisons
and per-pass diffs. HUD, weapon, FX, decals and lighting have no certified
full-scene pixel result yet.

## Validation soak

The stock Debug build ran under `MTL_DEBUG_LAYER=1`,
`MallocStackLogging=1`, unique external homes/ports and `timeout -k 10`.
The console explicitly confirmed “Metal API Validation Enabled.” The screen
was locked throughout.

* `soak-debug-v3` completed 615.096 seconds, 23 cycles and a Toujane →
  Dawnville map change, exited 0, and logged zero unready draws or Metal API
  validation errors. Its six requested weapon names were enfield, Garand,
  Sten, Bren, scoped Enfield and Webley. The `take weapons; give <name>`
  sequence did not reliably equip fresh primary weapons: the actual stock
  give path uses touched-item restrictions. Captures lacked weapon/ammo
  visuals. This proves duration/map-transition stability for the exercised
  world/UI path, **not** all-class combat/FX coverage.
* `soak-combat-v4` used stock class menu choices and respawn attempts, adding
  shotgun and switching to American choices on Dawnville. It reached eight
  cycles / 308 seconds on Toujane, then exited 255 after the Dawnville join
  with “DirectX failed a call to lock a vertex buffer or an index buffer.”
  There was no preceding Metal API validation error or draw rejection.
  Weapon/ammo visuals were still absent in post-respawn captures, so class,
  grenade and reload coverage is unverified. This is a failed soak attempt.
* `soak-map-diagnostic` reproduced the failed lock without a map change:
  HRESULT `8876021c`, a 4 MiB static-model VB, offset 2,277,376, length 4,608,
  flags `0x1001` (`NOOVERWRITE`), usage `0x208`, submissions 577/575.
  The new GPU regression reproduced a false hazard on a gap between sparse
  indices. Exact referenced-vertex runs fix that case; generic WS30's bounded
  busy contract remains intact. `soak-map-sections` then completed 152.911
  seconds, two cycles and Toujane → Dawnville, exit 0, with an observed rifle
  and ammo HUD and zero unready/rejected draws or Metal validation errors.
* `soak-combat-v5` exposed the quad overflow described above. An external
  diagnostic run logged `vertcol_simple.vsa/.pse`, invalid index 12 against
  declared `[0,12)`, and a stack through `RB_EndSurface`/`RB_DrawIndexedPrim`.
  v5 was stopped after observing the failures. `soak-combat-v6` used the
  corrected producers and had no logged rejected draws, but post-suicide
  captures still lacked weapon/ammo visuals. It was stopped to replace the
  coverage driver rather than count invalid combat coverage.
* `soak-combat-v7` exited 0 after 1,101.726 wall seconds, 26 cycles and
  619.226 seconds of intervals after class-join commands. It restarted the
  stock map for fresh initial choices, then changed from Toujane to Dawnville.
  API validation was enabled; unready/rejected draw messages, failed locks,
  crashes and Metal validation errors were all zero. Enfield and BAR captures
  visibly show the gun and ammo HUD. Other requested classes include blank
  captures or a different rifle from the requested scoped class. The driver's
  `active_seconds` is an interval counter, not proof of actual all-class combat.
  The retained equip/reload/frag/smoke captures therefore fail full coverage.
  The two-map leak scan reported 428 allocations / 21,030,960 bytes, including
  two roughly 10 MiB `VirtualAlloc` roots, with restricted attribution.

Live leak scans were not clean: v3 reported 427 allocations / 10,528,816 bytes;
v4 reported 426 / 10,528,720 bytes. Both warned that the process was not
fully debuggable and readonly memory inspection was restricted. The largest
reported root was about 10 MiB from `VirtualAlloc`; other roots included
engine strings/dvars and Foundation/XPC. No Metal root was identified in
those scans, but restricted attribution cannot establish absence of Metal
leaks. The provider teardown fixtures pass separately. A debuggable signed
run, restart/respawn capture diagnosis, leak attribution and a verified
weapon/grenade/reload driver remain required. The full no-crash/no-validation-error/no-leak combat gate fails.

## Exploratory performance

The shared `~/Projects/cod2-native-wt/_agents/bench.lock` was held throughout.
The screen was locked before and after each measurement. No competing game
or test/build process from this workstream ran during measurement; the tools'
external machine snapshots record the other system processes. These results
are exploratory only. The live benchmark now allows a configurable 180-second
load window and accepts its native common client-frame observer marker on
Metal (the GL swap marker is absent when Metal bypasses that hook). The initial
60-second window expired during Metal material preparation and produced no measured frames. Failed attempts are retained
separately from successful retries.

Final successful results are in external `performance-indexed/` and
`performance-retry/`. All eight timedemos, all four GL live cases and all four
Metal live cases completed after the runner correction. Three failed Metal
live marker attempts are preserved; they had CSV frames but never reached
result publication. Earlier pre-index/pre-PSO-sharing load-timeout attempts
are also retained in `performance/`; they are not performance measurements.
The loading-process sample, rather than those incomparable FPS attempts,
justified the native lookup indexes.

These are **client-frame clock** results with the screen **locked**, stock
Release, windowed, `r_gpuSync=adaptive`, ten-second static live samples. The
requested live eye is `(-299,1001,121)`, yaw 90; all successful live runs logged
`(-299 1001 88) : 90`, so they do not establish the requested camera recipe.
The demo contains 220 replay frames; its frame timing has integer-millisecond
precision and only 219 interval samples. No long timedemo confidence claim is
made.

### Live client-frame clock

| Resolution | Cap | GL FPS | Metal FPS | GL 1% low | Metal 1% low | GL P99 ms | Metal P99 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 333 | 332.97 | 330.03 | 268.29 | 228.94 | 3.041 | 4.165 |
| 1920x1080 | uncapped | 894.80 | 710.46 | 404.55 | 223.93 | 2.163 | 4.301 |
| 2560x1440 | 333 | 333.23 | 327.53 | 302.89 | 229.71 | 3.032 | 4.166 |
| 2560x1440 | uncapped | 824.42 | 702.99 | 365.93 | 221.28 | 2.446 | 4.351 |

### Timedemo client-frame clock

| Resolution | Cap | GL FPS | Metal FPS | GL 1% low | Metal 1% low | GL P99 ms | Metal P99 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 333 | 333.80 | 327.90 | 272.73 | 187.50 | 3.000 | 4.000 |
| 1920x1080 | uncapped | 969.20 | 712.00 | 333.33 | 187.50 | 2.000 | 4.000 |
| 2560x1440 | 333 | 334.30 | 325.90 | 300.00 | 187.50 | 3.000 | 4.000 |
| 2560x1440 | uncapped | 940.20 | 733.30 | 333.33 | 187.50 | 2.000 | 4.000 |

Metal's successful live sessions reported 0 presented/completed callback
samples and only sparse display-link samples, with no native actual-display
marker. Their whole-session draw totals (including menu/load/capture) were:

| Resolution/cap | Encoded | Null-slot dropped | Unready | Rejected |
| --- | ---: | ---: | ---: | ---: |
| 1920x1080/333 | 180129 | 381233 | 0 | 0 |
| 1920x1080/uncapped | 188478 | 1041068 | 0 | 0 |
| 2560x1440/333 | 153551 | 389207 | 0 | 0 |
| 2560x1440/uncapped | 168891 | 1056911 | 0 | 0 |

The many dropped draws and absent presentation receipts prevent treating
these loop rates as equal-work GPU rendering throughput or physical display
FPS. GL swap-return timing is itself a proxy; Metal's drawable-present timing
provided no samples while locked. The observed loop numbers favor GL, but a
meaningful unlocked, matched-work comparison remains required before making
any rendering-performance recommendation.

Reproduce each live case with a new external output, a unique unused port,
`--resolution 1920x1080` or `2560x1440`, `--maxfps 333` or `0`, `--seconds 10`,
and `--set r_renderer gl` or `metal`:

```sh
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2 Silicon/shaders" \
COD2_METAL_CACHE_ROOT="$HOME/Library/Application Support/CoD2-native-ws33/cache-cold-fixed" \
timeout -k 10 240 python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --binary "$PWD/build-macos/cod2_macos" --output /external/fresh-output \
  --resolution 1920x1080 --maxfps 333 --seconds 10 \
  --set r_renderer metal --set net_port 29882
```

The external `matrix.json` files retain every exact command, including
`tools/macos/benchmark.py` with `--timeout 180`, the same four resolution/cap
pairs and the privately recorded `ws33_bench` demo. Licensed demos and the
symlink-only benchmark base remain outside git.

## Build and suite gates

Stock and CoD2x Release builds and the stock Debug build passed using the
`CONTRIBUTING.md` native flags. Engine compile databases retain `-DNDEBUG`;
source-fixture copies replace it with `-UNDEBUG` as instructed. Actual engine
flags, including `-ffp-contract=off`, were not changed.

```sh
taskpolicy -b nice -n 19 cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos --parallel 3
taskpolicy -b nice -n 19 cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
taskpolicy -b nice -n 19 cmake --build build-macos-codx --target cod2_macos --parallel 3
```

Every suite listed in `CONTRIBUTING.md` was invoked. Local logs and command
lists are in ignored `output/ws33/{units,sources,gpu}.json` and corresponding
logs. The final legacy reruns supersede the intermediate header-guard failures.

| Gate | Result |
| --- | --- |
| Tools/datagen/ABI checker unit suites, shader setup, Wine trace parser | PASS |
| `COD2X_SANITIZERS=1 tests/cod2x/run_full.sh`, `run_native.sh`, SDK identity | PASS |
| Online stock | PASS |
| Online CoD2x | FAIL: inherited timeout fixture misses `Cod2x_Frame`/`Cod2x_DemoClientFrame` definitions |
| Fixes stock | FAIL: inherited extractor requires gated-out `Cod2x_LimitedFPS` |
| Fixes CoD2x; perf source fixtures; LP64 game/script/renderer | PASS |
| GL raster, mip and volume GPU runners | PASS |
| HUD matrices, impact marks, FX events/primitives/cloud, dedicated FX | PASS |
| Platform CTest with ASan/UBSan | PASS: 34/34 |
| Platform CTest without sanitizers | PASS: 36/36 |
| Private datagen i386 round trip/determinism, layouts, fixes reference | PASS |
| Full stock and CoD2x ABI check | PASS: 0 mismatches, 0 new errors |
| Frozen ABI headers; `git diff --check` | PASS: no frozen-header diff; whitespace clean |
| Legacy fixes and ABI preprocessing vs integration base | PASS: 0 mismatches in all OFF configurations |
| CoD2x inactive preprocessing | Ten stock-native `arm64-codx-off` differences from intentional native Metal hooks; all legacy/inactive configurations unchanged |
| x86 object/binary artifact gate | UNAVAILABLE: script requires x86_64 Linux multilib; this host is macOS arm64 |

The full ABI checks covered 635 stock / 652 CoD2x translation units, all
218 rendering callback bindings and 515 imports, with zero function/callback
mismatches or proven missing imports/extra dereferences:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/abi-codx
python3 tests/fixes13/legacy.py --base 94ee40d7d460d7ccb06947157fa87dbdbf654d8d
python3 tools/abi/legacy.py --base 94ee40d7d460d7ccb06947157fa87dbdbf654d8d
python3 tests/cod2x/check_inactive_gates.py --base 94ee40d7d460d7ccb06947157fa87dbdbf654d8d
```

All Metal runners passed: `run_core.sh`, `run_resources.sh`, `run_states.sh`,
`run_targets.sh`, `run_target_shim.sh`, `raster.sh`, `fixed_run.sh`,
`shaders.sh`, `setup.py`, `run_draws.sh`, `run_draws_gpu.sh`,
`run_tess_overflow.py`, golden unit tests, and `tools/metal/run.sh` with the real local shader cache and external output.
The new GPU fixture independently authors synthetic shader inputs for all
417 expected names and exercises all four shim entry points, signed base and
first-index ranges, immediate UP mutation, immutable constant snapshots,
packed colors, culling, released resources surviving GPU readback, null slots,
patch rejection and ready-only associations after sealing. The resource GPU
test verifies eight disjoint DISCARD append locks with exact readbacks while
preserving the bounded-ring busy case. No fixture embeds original shader text.

An additional exploratory all-target `cmake --build` invocation failed to link
`cod2_macos_ded` in both configurations because the generated literal/import
units reference omitted client/GL symbols (for example `CColorArray_Enable`,
`CG_DObjCalcPose` and `CIN_PlayCinematic`). The required explicit `cod2_macos`
client targets pass. Dedicated FX source fixtures pass; a linked native
headless executable is not established by this branch.

The two source-fixture failures are also documented by the merged WS32/WS34
reports. They were not changed to hide the failures. The OFF preprocessing
checks support guard preservation; they do **not** establish byte-identical
i386 artifacts. The orchestrator must run the Linux multilib gate. No package
was installed. The locally generated Debug build tree was moved to external
Application Support after runtime checks, leaving only tracked changes in
the worktree. `clang-format` and Python PIL were unavailable; style was
checked directly and existing `sips` plus the native screenshot decoder were
used for local images.

## Merge notes and default recommendation

Merge this branch after the already integrated WS29/30/31/32/34 providers;
do not re-merge sibling worktrees. Ten implementation commits plus focused
exit-diagnostics and observer-marker commits follow the integration base.
The shared changes are four native source entries in `cmake/macos-arm64.cmake`, seven-line native hooks
in each of `cl_ui_mp.c` and `r_init.c`, and the native enum fix in `rb_tess.c`.
`CMakeLists.txt`, `src/headers/*` and all frozen ABI 1 declarations are untouched.
The provider-local changes in resources, shader associations and PSO factory
sharing are necessary for real engine integration; preserve their regression
fixtures when resolving conflicts.

Use `COD2_MAC_SHADER_CACHE` for the verified original cache and optionally an
absolute `COD2_METAL_CACHE_ROOT` under Application Support. These external
roots are runtime inputs/cache locations, not material to import into git.
`r_renderer metal` is an explicit experimental selection. Reports and fixture
passes establish the implemented provider path; they do not establish full
scene parity or reliable all-class combat. Review the capture/respawn/restart
failures before treating the renderer as production ready. Run the Linux
multilib artifact comparison before certifying OFF byte identity.

Remaining acceptance work is the common exact-frame receipt/replay hook,
per-view/per-pass lossless comparisons, reliable all-class activity under
validation, restart capture diagnosis and attributable clean leaks. Two
inherited source fixtures and the native dedicated link also need their
respective owners. No remote, push, PR, issue or system package operation was
performed.

Keep `r_renderer gl` as the default. Do not recommend changing it until every
strict golden passes and a verified all-class combat run has a clean leak
result. MetalFX, MSAA, EDR, ProMotion and modern quality/options remain later
work; this branch neither enables them nor claims their parity.
