# WS35 — Metal parity, deterministic replay and lifetime investigation

Branch: `port/metal-parity`; starting commit: `aa94c0c` (WS33).
Evidence collected on macOS 27.0.1, Apple M6, 2026-10-05.

**Recommendation: retain GL as the default.** Metal now renders complete
admitted world frames, has genuine GPU/presentation receipts, and passes the
pixel gates on five frozen replay views. It does not satisfy the full WS35
acceptance gate: sustained combat still coalesces frames, the ordinary engine
FX clock has an LP64 bug, an FX-heavy golden and every weapon class remain
unproved, unfiltered `leaks` is not clean, and equal-work performance is not
measured. The replay deliberately reports `certified: false` until complete
logical draw/pass/resource and factory receipts exist.

Demos, captures, per-pass differences, isolated soak caches and raw evidence
remain outside git under:

```
~/Library/Application Support/CoD2-native-ws35/
```

The licensed base is read-only `~/Games/CoD2`; source shaders are read from
`~/Library/Application Support/CoD2 Silicon/shaders`. Replay uses the existing
external `CoD2 Silicon/metal-draws` runtime cache. No asset/cache is bundled.

No sibling worktree was accessed, no package was installed, no remote was
changed, and nothing was pushed. ABI 1 and `src/headers/*` remain untouched.

## Null slots: the measured cause

The WS33 `dropped` counter counts a NULL **producer submission**, not a missing
texture or buffer. The frame-tail fence committed a producer command buffer;
Present then created and committed another producer buffer. That consumed both
ABI 1 producer slots. Subsequent draw calls retried slot admission separately:
the BSP prefix could disappear while models/HUD encoded after a slot retired.
This explains the partial-world symptom without inventing a missing lightmap
provider.

`COD2_METAL_FAILURE_TRACE=/external/path/failures.tsv` now records each failed
or dropped draw: exact failure stage and sampler/stream index, offending shim
object, HRESULT, indexed/UP flags, primitive count, engine material, vertex
producer declaration, lightmap index, shader/declaration/index/stream/texture
identities, and a symbolized engine stack. Slot failures distinguish PSO,
depth state, constant banks, resources, argument tables, targets and encoders.
The summary groups identical material/call-site failures. These diagnostics are
opt-in and stay local.

Normal tail fences now share Present's producer commit. Query fences still
commit immediately; testing a pending fence flushes once without waiting.
BeginScene admits a frame once. If no producer slot exists, the whole frame is
skipped through Present; it cannot resume halfway through world geometry.
Screenshot readback uses the same commit path.

| Failure slot | WS35 baseline Toujane smoke | Fence correction smoke | Final frozen replay | Final ordinary 10-minute soak |
| --- | ---: | ---: | ---: | ---: |
| Producer submission NULL | 13,152 | 70 | 105 | 30,941 |
| Texture/sampler provider | 0 observed | 0 observed | 0 | 0 |
| Vertex/index provider or declaration | 0 observed | 0 observed | 0 | 0 |
| Constant/fetch bank | 0 observed | 0 observed | 0 | 0 |
| PSO/depth state unready | 0 | 0 | 0 | 0 |
| Target/encoder/argument table | 0 observed | 0 observed | 0 | 0 |
| Encoded draws | 135,232 | 161,165 | 70,988 | 3,165,526 |

Provider checks cannot prove readiness for a frame rejected at admission;
“0 observed” means no failure in draws that reached that provider. Unbound,
shader-unused stages are legal; a deliberately unbound sampled texture uses
the existing typed black/null texture provider rather than dropping the draw.
World textures, lightmaps, static/dynamic vertex and index buffers, declarations,
DrawPrimitiveUP and indexed-UP paths already exist in WS33. This branch found
no missing provider requiring a replacement implementation.

The initial baseline trace predates material/declaration context. Its stacks
identify 11,581 generic batched surface draws, 1,359 rigid-model draws, 209
HUD/text/loading draws and three other loading draws. It cannot disambiguate
all batched world/FX materials retrospectively. The final trace can:

| Final ordinary soak producer context | Submission drops | Evidence |
| --- | ---: | --- |
| World declaration (`VERTDECL_WORLD=1`) | 16,868 | Material/lightmap and `RB_RenderDrawSurfList` stack recorded |
| Static-model cache declaration (`=3`) | 4,204 | Static mesh materials and vertex/index identities recorded |
| Generic declaration (`=0`) | 9,869 | Includes rigid/skinned models, FX, decals and 2D |
| Rigid models (stack classification; overlaps declaration rows) | 1,933 | `RB_TessXModelRigid` |
| Skinned models (overlapping) | 695 | `RB_TessXModelSkinned` |
| FX/decal materials (overlapping) | 2,404 | `gfx_*` materials; not all FX use that prefix |
| HUD text (overlapping) | 1,466 | Text/character engine stack |
| HUD/loading pictures (overlapping) | 2,653 | Stretch-picture engine stack |

These are **not accepted intentional GL-equivalent skips**. The zero-drop
requirement is unmet. During the settled 30-fps replay all 105 drops are generic
loading/2D draws; no world/static-model declaration drops occur there.

A separate real regression was caught while fixing map lifetime: returning
from reused-pool initialization before `RB_SetInitialState` left cached
`samplerImage` identities stale. The next map reused an image identity while
Metal still bound its retired texture. The intermediate five-map run recorded
27,998 `fragment-texture-provider[6]` rejections. Keeping initial-state reset
while skipping only allocations eliminated them: the repeated five-map run
encoded 449,858 draws, with **0 rejected and 0 unready**. Stage 6 was a stale
lightmap-weight/procedural-image binding, not absent BSP support.

## Deterministic replay and pixel gates

`tests/metal/goldens.py replay` uses one external `.dm_1` recording for serial
GL-A, GL-B and Metal launches, unique homes/ports, `pgrep` admission checks,
GNU `timeout -k`, identical settings and a common correctness cap (30).
It applies settings again after the replay gamestate and queries actual dvars.
It waits for map initialization, exact target-time receipt and settling, then
captures three lossless frames per view. Uniform clear-colour frames are
rejected. No masks, crops, gamma corrections or threshold relaxations are used.

The native, environment-gated `goldenpose` applies exact floating-point eye and
angles before the final camera axis/viewmodel calculation. The screenshot
readback logs the **actual** pose, refdef time, weapon/class/ammo/clip/state,
player mode and gun/HUD flags. Ambiguous, missing, nonfinite or unequal receipts
fail the harness. Camera movement/eye-height physics no longer choose the pose.

The original `CL_SetCGameTime` reconstruction attached its ordinary-time branch
to the wrong freeze conditional. Verified Mac 1.3 disassembly independently
checks timedemo after the freeze branch. The native correction holds ordinary
`cl_freezeDemo` and keeps 50-ms timedemo stepping independent. The opt-in
`COD2_MAC_GOLDEN_DEMO_TIME` steps by at most 50 ms to an exact positive target,
then holds it. The production-function ASan/UBSan fixture fails before the fix
and passes after it.

FX exposed a second clock problem: `FxHelper_AdjustTime` and `WarpTime` read
byte 8 of `fx_freeze`. On LP64 that byte contains the `0x1080` flags; the boolean
is in `current.enabled`. An owned-process debugger receipt confirmed
`time=20828`, `mTime=0`, `mFrameTime=0`, `mSeed=20828`. A casing therefore remained
in an initial, frame-dependent state despite frozen refdef time. **Only the
opt-in golden replay** now reads the typed freeze value. Ordinary GL/Metal
behavior is preserved, as required. The production FX clock fixture tests
forward/frozen/backward stepping and explicitly checks unchanged ordinary-mode
behavior. A general engine FX clock repair is a follow-up proposal, not silently
included in this renderer workstream.

Final reproducible capture command (external demo already recorded with the
player grounded, rifle firing and grenade button attempts):

```sh
python3 tests/metal/goldens.py replay build-macos/cod2_macos \
  --demo "$HOME/Library/Application Support/CoD2-native-ws35/record-heavy-fx/main/demos/ws35_reference.dm_1" \
  --scene-time-ms 20828 --fx-view 1234 2433 128.561996 149.699707 0 \
  --output "$HOME/Library/Application Support/CoD2-native-ws35/replay-fx-clock"
```

Result: exit 0, `replay-fx-clock/replay-5f016c602aa5.json`. A second full
replay after the uniform-clear guard also exits 0:
`replay-final/replay-c478768edcc3.json`; all GL/Metal repeats remain byte-identical
and all five pixel rows pass.
Binary SHA-256: `e8e25e2c41b846126a108f259f53221854e69ba9aba28def2f65e601c1b46454`.
Demo SHA-256: `c7242339bff38a70993ba60b616b04b09efe5bc9d28027c66b5fa57ec0df529b`.

**GL determinism proof:** all three captures in both GL launches are byte-identical
for every view (six identical RGB payloads per view). All three Metal repeats
are also byte-identical per view. Actual time is 20,828 ms for every shot; actual
poses match each requested view within 0.001; equipment and settings agree
across launches. The weapon view receipts show `enfield_mp`, clip 8, ammo 60,
class 0, gun 1; other views show gun 0. HUD is 0 throughout.

The following values use **8-bit channel units** (divide MAE/RMSE/p99 by 255
for normalized values). Gates are MAE ≤ 1, RMSE ≤ 2, p99 ≤ 4 and strictly less
than 0.1% of pixels with any channel error > 16. Each image is 1280×720;
three identical Metal repeats give the same row.

| View | MAE | RMSE | p99 | Pixels >16 | Percent >16 | Pixel gate |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| WS15 courtyard | 0.007105 | 0.178810 | 0 | 18 | 0.001953% | PASS |
| WS15 rooftop | 0.010658 | 0.134250 | 0 | 2 | 0.000217% | PASS |
| WS15 sky | 0.001417 | 0.053135 | 0 | 0 | 0% | PASS |
| Weapon in hand | 0.006154 | 0.157531 | 0 | 11 | 0.001194% | PASS |
| FX enabled, tank/casing view | 0.024351 | 0.748851 | 0 | 633 | 0.068685% | PASS; sparse FX |

The fifth row is **not an FX-heavy acceptance claim**. Visual inspection shows
sparse effects. A second pose at a map-authored distant smoke plume was outside
the rendered world and produced only clear colour; it is discarded. The harness
now explicitly rejects such images. Grenade commands alone are not evidence of
a dense smoke scene.

Before the FX clock correction, the same sparse view failed: GL-A vs GL-B had
734 >16 pixels (0.079644%, nearly-repeatable gate passed); GL vs Metal had 2,067
(0.224284%, FAIL), MAE 0.096038, RMSE 1.822882, p99 0. Local amplified difference
images isolated the casing drift and tank highlights. An owned debug process
with `fx_draw=0`, queried afterwards, made all GL repeats exact and reduced the
tank view to 633 outliers (PASS). That per-pass diagnostic was not substituted
for the final FX-enabled result. The clock correction then removed casing drift
without changing sampler, sRGB/gamma, depth, culling, alpha-test or fog policy.

Full certification remains blocked by absent complete logical input/pass/resource
hashes and frame-specific shader-factory receipts. The harness does not invent
these or certify pixels alone. See `metal-goldens.md` for the receipt boundary.

## Presentation receipts

Native presentation installs `addPresentedHandler` and obtains
`CAMetalDrawable.presentedTime`. Native presenter completion uses actual Metal
queue feedback. Producer completion has a separate kind so it cannot be mistaken
for display presentation. Observer kinds are: 0 enqueued, 1 presented, 2 display
link target, 3/4 presenter completion success/failure, 5/6 producer success/failure.
The existing frame probe rejects nonpositive stamps.

An external observer dylib was injected only into the owned native game for a
five-map correctness run (`map-state-check`). The benchmark lock was acquired
with `mkdir`, held for this observer check, and released. Raw events:

| Receipt | Count |
| --- | ---: |
| Display-link targets | 8,050 |
| Producer completions, success | 4,377 |
| Presenter enqueued/completed, success | 3,420 each |
| Presented callbacks | 3,420 |
| Positive actual presented timestamps | 3,342 |
| Presented callbacks with zero timestamp | 78 (not pacing samples) |
| Presenter/producer failure completions | 0 |

This establishes native callbacks, not an FPS result. The local observer state
file's ad-hoc screen detector recorded false without retaining sufficient lock
property evidence; do not use that field to certify unlocked pacing. Builds,
Clang and Python audits were running. Earlier session inspection found a locked
screen. Consequently this observer run is exploratory only.

The production `bench_state.py` previously requested no IORegistry properties
and could return unknown. It now requests `-l -w0` and reads the explicit session
lock key, falling back to `IOConsoleLocked`; missing evidence remains unknown.
A final actual registry receipt records `IOConsoleLocked=No` (unlocked). Parser
regressions cover both states, precedence and unknown. When no presented samples
exist, `live-bench.py` reports screen-lock suppression if observed, otherwise
reports missing/occluded/unknown presentation rather than fabricating timing.

## Lifetime, validation and combat

`CFBridgingRelease` in the sidecar destruction callback placed retired payloads
in the engine's long-lived autorelease pool. A retained resource could therefore
survive map changes despite the registry releasing it. Balancing the original
`__bridge_retained` with `CFRelease` destroys it immediately. A 16-object
production resource regression checks stable live count without draining the
caller pool; it fails before the correction and passes after it.

Renderer registration also recreated dynamic IB/VB and both skin buffers and
lost the previous 10-MB scratch allocation whenever a map loaded. Verified Mac
registration skips creating these pools when the device already exists. The
Metal-only path now reuses complete pools, while still resetting frame/sampler
state. Procedural texture replacement releases the previous Metal texture.
An ASan production-function fixture checks repeated registration, reset,
replacement lifetime and the unchanged GL allocation/replacement path.

The final ordinary-mode soak used the debug native client, `MTL_DEBUG_LAYER=1`,
`MallocStackLogging=1`, GNU `timeout -k 10 900`, and:

```
leaks --noContent --fullStacks --atExit -- <owned debug cod2_macos> ...
```

The external reproducible driver is `/tmp/ws35-soak-accepted.py`; its full launch
and issued commands are recorded in `soak-accepted/soak.json` and `commands.json`.
The directory name does not mean all acceptance gates passed. It ran 604.873 s
of combat control, 51 firing/reload/grenade cycles, `map_restart`, several Toujane
reloads, a Dawnville map change and return to Toujane. Engine exit reached CRT
normally; the wrapper exits 1 because leaks remain.

| Ordinary-mode soak result | Evidence |
| --- | --- |
| Duration / native crash | 604.873 s / no crash |
| Encoded / submission-dropped | 3,165,526 / 30,941 |
| Unready / rejected provider draws | 0 / 0 |
| Metal validation errors | 0 observed (`MTL_DEBUG_LAYER=1`) |
| `leaks` at exit | **440 leaks, 30,256 bytes; FAIL** |

Actual weapon receipts prove rifle (Enfield), SMG (Sten), spread/shotgun and
pistols (Webley/Colt), with depleted and replenished clips. Bren fires, but its
licensed MP definition reports class **rifle**, not MG. Scoped Enfield is seen
with clip 10; scoped firing is not independently proven. Frag and smoke buttons
are issued, but primary-weapon receipts do not prove grenade ammo consumption
or a smoke cloud. Rockets, mounted turrets, item/binocular classes and every
weapon class are not proven. No authored MP class-1 MG definition was found in
the owned metadata; this is not grounds to relabel Bren. A first `take weapons`
recipe left `weapon=none`; it was rejected as combat evidence.

Leak results before and after ownership fixes:

| Run | Leaks | Leaked bytes | Meaning |
| --- | ---: | ---: | --- |
| `soak-class` before retirement correction | 455 | 94,549,136 | Repeated scratch allocation plus residual roots |
| `soak-final` after retirement, before pool reuse | 431 | 102,573,904 | Engine pool leak remained |
| `soak-accepted` after pool/sampler corrections | 440 | 30,256 | Large renderer leak removed; unfiltered exit still fails |

Final roots: 411 nodes/26,112 bytes in three AppIntents/LinkServices XPC cycles;
20 nodes/3,968 bytes from SDL3 `GetDisplayMode` NSMutableArray/CGDisplayMode
ownership; nine engine `CopyStringInternal` dvar strings totaling 176 bytes.
There are no Metal resource payload roots in this final report. This attributes
the residual report; it does **not** waive the clean-`leaks` requirement or hide
framework allocations. Fixing installed SDL or unrelated dvar ownership is a
follow-up; no system packages or global library configuration were changed.

Live inventory counts actual surviving textures, buffer generations/bytes and
unique resident PSO identities, not cumulative factory calls:

| Settled map boundary | Textures | Buffers | Buffer bytes | Resident PSOs |
| --- | ---: | ---: | ---: | ---: |
| Initial Toujane | 580 | 796 | 88,077,160 | 1,344 |
| Five repeated Toujane registrations | 580 | 796 | 88,077,160 | 1,344 |
| Dawnville | 719 | 874 | 92,510,960 | 1,262 |
| Return to Toujane | 580 | 796 | 88,077,160 | 1,346 |
| Next Toujane registration | 580 | 796 | 88,077,160 | 1,346 |

Texture/buffer counts and bytes return exactly. Two new warmed PSOs appear on
return, then remain stable; therefore strict “no PSO growth at all” is not proved.
Transient loading/menu boundaries have 55 additional textures and a smaller
loading PSO cache; those are not compared to settled map inventories.

The final FX-clock-enabled diagnostic soak (`soak-fx-clock`, driver
`/tmp/ws35-soak-fx-clock.py`) ran 604.054 s and 51 cycles, with map restart,
Dawnville and return. It encoded 3,178,264 draws, dropped 4,185 producer
submissions, and had **0 unready, 0 rejected and 0 Metal validation errors**.
All failure records are submission admission. Startup explicitly logs
`Metal API Validation Enabled`; no validation error is observed. Unfiltered exit reports
**506 leaks / 43,200 bytes**, so it still fails clean leaks. Inventory returns
to exactly 580 textures / 796 buffers / 88,077,160 bytes; resident PSOs return
to 1,345 and remain there on the next registration (one warm-up addition).
Actual unlocked state is recorded by the corrected registry detector;
background Clang/Python audits were present. This is correctness evidence,
not a performance comparison.

This run sets `COD2_MAC_GOLDEN_DEMO_TIME=2147483647` while playing a live local
server. The demo-only server-time hook does not freeze live gameplay; the
opt-in FX hook reads the correct boolean. It cannot certify the ordinary
renderer FX clock or every weapon class. Rifle/SMG/Bren/shotgun/pistol clip
changes are confirmed again; scoped firing and actual grenade consumption
remain unproved.

## Performance boundary and optimization evidence

No 1080p/1440p capped/uncapped GL-versus-Metal FPS comparison is claimed.
Equal-work prerequisites fail: ordinary combat skips whole Metal frames and
complete logical input receipts are absent. Pixel correctness captures at
1280×720/30 fps and debug soaks at cap 125 are not benchmarks. Competing builds
and audits further prevent a clean CPU encode comparison.

Measured structural changes are a normal frame tail's **two producer commits →
one**, immediate retirement of GPU payloads, and stable map buffer inventories
instead of leaked scratch/dynamic pools. Source inspection also shows expensive
per-draw sidecar lookups, index scans, three constant/fetch `newBufferWithBytes`
uploads, two argument-table allocations and all-stage sampler-key lookups.
These are candidates, not profiled attribution. No arena/cache optimization was
added without an equal-work measured baseline. Before/after CPU encode and FPS
tables are therefore **not measured**, not invented from frame counts.

## Merge gate and commands

Both arm64 Release targets and the external Debug target build. Assertions are
enabled in fixture compile databases; the ABI databases retain actual Release
`-DNDEBUG` flags. All CONTRIBUTING suites were executed. Initially the CoD2x
online timeout/digest fixtures lacked extension dependencies and the stock
fixes13 timing fixture extracted gated-out extension functions. Focused fixture
commits fix those assumptions; both full online and fixes13 variants now pass.

| Gate | Result / command |
| --- | --- |
| Stock + CoD2x native build | PASS: `cmake --build build-macos --target cod2_macos --parallel 3`; same for `build-macos-codx` |
| Tool/datagen/ABI checker tests, shader setup, Wine trace parser | PASS: all CONTRIBUTING commands; tools harness now 17 tests |
| CoD2x full sanitizer/native suites and SDK identity | PASS: `COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh`; `sh tests/cod2x/run_native.sh`; `sh tests/online/sdk_identity.sh` |
| Online + fixes13, both compile configurations | PASS: `python3 tests/online/run.py build-macos{,-codx}/compile_commands.json`; `python3 tests/fixes13/run.py --build build-macos{,-codx}` |
| Perf source fixtures, LP64 game/script/renderer, real GL shader/texture/volume fixtures | PASS: every listed CONTRIBUTING runner |
| HUD, impact, FX event/primitive/cloud and dedicated FX fixtures | PASS: every listed CONTRIBUTING runner; FX primitive suite repeated after clock investigation |
| Platform CTest | PASS: sanitizer 34/34; plain 36/36 |
| Private datagen roundtrip/layout/reference checks | PASS: all three CONTRIBUTING commands |
| Metal core/resources/states/targets/target-shim/draws/GPU draws | PASS: all `tests/metal/run_*.sh` |
| Metal shader/raster/fixed/setup/observed states/tess overflow/golden unit tests | PASS: all corresponding runners; 14 golden tests |
| New production clock/map lifetime fixtures | PASS: `python3 tests/metal/run_demo_clock.py`; `run_fx_clock.py`; `run_map_lifetime.py` |
| Full owned-shader translation/compile/validation | PASS: `sh tools/metal/run.sh "$HOME/Library/Application Support/CoD2 Silicon/shaders" "$HOME/Library/Application Support/CoD2-native-ws35/gates/translated"` |
| ABI function/callback/import audit | PASS at 0 for both actual engine databases; final replay-clock rerun recorded below |
| Toujane smoke, GL + Metal | PASS: both `tools/metal/smoke.py` renderer runs, exit 0, menu/map lossless screenshots |
| OFF preprocessor supplement | PASS: fixes13 legacy 16 source files/10 configurations; ABI legacy 11 C files/5 configurations, 0 mismatches |
| x86 artifact byte-identity | UNAVAILABLE: `bash tools/ci/compare-x86.sh 410342a HEAD output/ws35-x86-reference` exits 2, requires x86_64 Linux multilib |
| `git diff --check` | PASS |

A repeat plain CTest run while the combat window was active failed the two
fixed fullscreen display-width assertions. After combat ended, a full rerun
passed 36/36 in 41.15 s; sanitizer CTest passed 34/34 in 44.28 s. The failed
interactive attempt is retained as evidence, not relabeled a pass. Do not
schedule fullscreen/display fixtures alongside a live game.

The full ABI commands are:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json \
  "$HOME/Library/Application Support/CoD2-native-ws35/gates/abi-stock"
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json \
  "$HOME/Library/Application Support/CoD2-native-ws35/gates/abi-codx"
```

Final actual-flag checks after the FX replay change both exit 0
(`/tmp/ws35-abi-stock-fx-real.log`, `/tmp/ws35-abi-codx-fx-real.log`). They report
635/652 TUs, zero errors/mismatches/new;
218 renderer bindings, zero table/cast/floating-prototype mismatches; 515 import
symbols, zero proven extra/missing dereferences. Initial 600-second wrapper
attempts timed out; completed full reruns replace those results.

`tests/cod2x/check_inactive_gates.py --base aa94c0c` reports 177 mismatches
across 20 files/17 configurations: it compares
Mac Metal Objective-C files as inactive CoD2x C even though this work is native
renderer functionality independent of CoD2x. The OFF guards above cover engine
compatibility; they do not replace Linux artifact identity. No claim of i386
binary identity is made on a Mac that cannot run the comparison. The orchestrator
must execute the Linux multilib gate without installing packages on this host.

Smoke commands used unique owned homes and ports after process checks:

```sh
timeout -k 10 300 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer gl --home "$HOME/Library/Application Support/CoD2-native-ws35/smoke-gl" --port 29937
timeout -k 10 300 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer metal --home "$HOME/Library/Application Support/CoD2-native-ws35/smoke-metal" --port 29938
```

The external evidence index is `gates/results.json`; later fixed fixture runs
are `/tmp/ws35-online-{stock,codx}-fixed*.log` and
`/tmp/ws35-fixes-{stock,codx}-fixed.log`. Raw game logs must be redacted before
sharing; they were not committed.

## Orchestrator handoff and proposals

Merge this branch in order after WS33. There are no CMake, frozen interface or
shared-header changes. Integration seams are small guarded additions in
`rb_shade.c` (failure context), `r_init.c` (Metal pool reuse), `r_image.c` (Metal
procedural replacement), `r_screenshot.c` and cgame/client clocks (opt-in replay),
and the native presenter. Replay pose/time controls are env-gated; GL is still
the selected default. Tests and report contain no derived assets.

Remaining acceptance work, with the contract boundary stated explicitly:

1. ABI 1 admits at most two producer allocator slots and `Begin` must return
   immediately. The whole-frame latch fixes partial-world corruption but cannot
   guarantee zero rejected frame work when both slots are outstanding. Propose
   an admission/pacing or retained-frame scheduling contract with explicit
   bounded memory and latency semantics; do not silently enlarge frozen slots,
   block the compositor/client or relabel skips as GL-equivalent. Then repeat
   menu/Toujane/combat traces and certify zero dropped work.
2. Repair the ordinary LP64 FX freeze read in a separately reviewed engine
   change. Doing so here would alter ordinary GL output, forbidden by this
   workstream. Obtain a verified dense smoke/explosion replay with grenade ammo,
   FX population and logical input receipts; then repeat all-pixel gates.
3. Add complete per-frame logical draw/pass/resource/shader-factory hashes
   shared by GL and Metal. Current camera/time/equipment proof is strong but
   insufficient to change `certified:false`.
4. Finish mounted turret, rocket, binocular/item and scoped-fire coverage with
   actual equipment/action receipts. Do not equate issued `give` commands to a
   held weapon or grenade throw.
5. Resolve residual SDL display-mode, framework XPC and engine dvar leak roots
   without hiding them; repeat unfiltered exit leak checks. Extend PSO map-cycle
   testing past warm-up and prove no continuing growth.
6. After zero drops and equal logical work, take the benchmark lock, record
   reliable unlocked state and competing processes, measure all four resolution/
   cap combinations, profile CPU encode, optimize measured hotspots, then rerun.

Retain GL until these gates pass. The world, lifetime, timing and harness fixes
are useful mergeable progress; this report does not call the workstream accepted.
