# WS37 — Metal finish: transport lifetime, allocation cost and acceptance gaps

Date: 2026-10-05. Branch: `port/metal-finish`. Base: WS35 `5cf411c`.

## Decision

**Keep GL as the default. WS37 acceptance is not achieved.** The branch fixes a
transport destruction deadlock and removes several measured CPU allocation
costs. It does not establish zero drops, zero process leaks, complete combat/FX
certification or equal-work presentation parity. The unmet gates are listed
below rather than converted into passes.

All source work stayed in the assigned worktree. No push, PR, issue, remote change,
package installation, game data, proprietary binary, translated shader, archive,
demo or capture was added to git. ABI 1 integration headers, `src/headers/*`,
CMake and the GL implementation remain unchanged. The new
`src/Mac/Metal/metal_encode_pool.h` is a private transport/encoder helper, not an
addition to ABI 1.

External evidence root throughout this report:

```sh
ws37_evidence="$HOME/Library/Application Support/CoD2-native-ws37"
```

Logs and captures remain there; the licensed read-only base is `~/Games/CoD2`.
Every game launch checked `pgrep -fl cod2_macos`, waited for other games, used a
private home and free local port, and ran under `timeout -k` (the replay harness uses a five-second kill grace). The model switch
interrupted the first final-soak controller. Its orphaned owned process was
identified by PID and exact home, terminated, and that incomplete evidence was
excluded. The replacement uses `soak-final2`.

## Changes to merge

| Commit | Change and verification |
| --- | --- |
| `0e37c7a` | Opt-in `COD2_METAL_ADMISSION_TRACE` records begin/commit/feedback ages and both occupied slots on failed admission. `COD2_METAL_PROFILE` times eight encoder phases. Both are disabled by default. |
| `5b05d65` | State-cache ready lookups borrow validated key bytes for synchronous full-byte equality; inserted keys still own their bytes. A forced-collision fixture and allocation counter prove ready lookups allocate zero times. The allocation assertion failed before the fix. |
| `df1a136` | Dense indexed batches use a bounded bitmap instead of sorting repeated triangle corners. Sparse 32-bit indices retain the old sorting path. ASan fixtures compare exact referenced vertices, including gaps, duplicates and negative base vertex. |
| `e007f30` | Retired submission slots reuse constant pages and independent argument tables. Constants remain immutable through feedback; pages are 2 MiB, at most four per slot, aligned to 256 bytes. Tables are capped at 512 per slot. Oversized work uses completion-retained fallback allocations. GPU tests verify different red/green constants in the same submission across three retirements/reuses. |
| `af48088` | Strict two-nibble shader digest decoding replaces 32 `sscanf` calls per digest. Loading/identity and actual GPU fixtures pass. |
| `8209105` | Replay diagnostics accept external validated map/pose recipes. Map identity, finite coordinates, view count/names, exact fields and gun control are checked. Existing Toujane defaults and strict pixel gates remain intact; diagnostics still report `certified: false`. Fifteen golden-harness tests pass. |
| `22f23a2` | Retained-object destruction runs outside the transport mutex. A slot stays unavailable until destruction completes. Completion and cancellation tests use a destructor that calls `MacMetalGPU_Submitted`; the original completion path deadlocked and failed its drain assertion after 10.53 seconds, then passed in 0.33 seconds after the fix. |

The fix removes a real reentrant destruction deadlock. Slots remain unavailable
until feedback and retained-object destruction complete. Arbitrary Objective-C `dealloc` must not
execute with the transport mutex held. The cancellation path similarly keeps
`encoding` set until retained objects are destroyed outside that mutex.

Command allocators were already bounded: four are created with the transport,
then reset on retirement. No per-draw allocator factory was found. Sidecar
lookups already use the WS33 pointer dictionary. The changes target repeated
key copies, dense index sorting, constant/table factories and digest parsing.

## Ordinary soak and NULL submission diagnosis

The final-head repeat, `soak-final2`, completes **604.856 combat seconds / 51
cycles**, with **7,170,849 encoded draws, 390,569 dropped, zero unready and zero
rejected**. There is no crash, Metal submission error or validation assertion.
The game exits normally; the outer `leaks --atExit` status is 1 because leaks
remain. **Zero drops fails.**

Its final admission trace has 13,928 occupied producer-slot records (6,964
failed begins), zero uncommitted busy slots, 13,354 unsignaled records and 574
signaled-but-unretired records. Pending-slot age is median 8.676 ms, p99
23.375 ms, maximum 51.746 ms. Of those pending records, the earliest unsignaled
queue value belongs to the producer in 11,854 and presenter in 1,500. Both GPU
work and presentation can occupy the shared queue; the final trace does not
support attributing all drops to presentation. The derived summary is saved in
`soak-final2/admission-summary.json`.

The ordinary recipe uses Metal, 1280×720, cap 125, validation and allocation
stack logging. It exercises menu/load, Toujane firing/reloading, weapon changes,
`map_restart`, Dawnville and return to Toujane for at least 600 seconds after
joining. `COD2_MAC_GOLDEN_DEMO_TIME` is **absent**: this is the ordinary FX path.
The controller and commands are retained externally; reproduce with
`python3 "$ws37_evidence/drivers/ws37-soak-final2.py"` from this worktree
with a fresh home/output name when repeating. The original local driver is
retained under `drivers/`. Its `soak.json` and `commands.json` record the actual
launch/interaction arguments; observed scene receipts verify honored actions. The underlying
exit-leak invocation is:

```sh
MTL_DEBUG_LAYER=1 MallocStackLogging=1 \
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2 Silicon/shaders" \
COD2_METAL_FAILURE_TRACE="$ws37_evidence/soak-final2/failures.tsv" \
COD2_METAL_ADMISSION_TRACE="$ws37_evidence/soak-final2/admission.csv" \
timeout -k 10 900 leaks --noContent --fullStacks --atExit -- \
  "$PWD/build-macos/cod2_macos" \
  +set fs_basepath "$HOME/Games/CoD2" \
  +set fs_homepath "$ws37_evidence/soak-final2" +set net_port '<free-port>' \
  +set r_renderer metal +set r_mode 1280x720 +set r_fullscreen 0 \
  +set com_introPlayed 1 +set developer 1 +set com_maxfps 125 \
  +set sv_pure 0 +set sv_maxclients 1 +set g_gametype dm
```

An earlier completed run, `soak-ordinary`, encoded 6,883,627 draws and dropped
492,087, with zero unready/rejected draws, over 606.636 combat seconds. These
are Release/validation measurements with a different admitted workload from
WS35's Debug soak; they are not a before/after drop-rate comparison.

Its admission trace has 18,322 occupied producer-slot records, two per failed
begin: all have a nonzero committed value; none identifies an abandoned
uncommitted producer encoder. In 13,224 records the slot's value exceeds the
event completion value; 5,098 are signaled but still awaiting feedback retirement.
The oldest unsignaled queue frontier is a presenter submission in 12,208 records
and a producer in 6,114. This is evidence of queue/admission pressure, including
presentation backpressure, rather than missing resources or PSOs. The maximum
age of an unsignaled producer slot is 50.231 ms. Ages use the measured local
`mach_timebase_info` ratio 125/3 ns per tick; event signaling is not feedback
retirement.

ABI 1 explicitly requires two slots per role, immediate NULL on exhaustion,
one shared queue, feedback before allocator reuse, and no client-loop waits.
WS35's whole-frame admission latch correctly prevents partial frames, but it
cannot create a free slot while both producer submissions are outstanding.
`waitForDrawable` is serialized on that same transport. A queue stall longer
than the two-slot budget can therefore produce NULL even with correct lifetime.
The branch does not enlarge the ring, reuse signaled-but-unretired allocators,
block the client, or relabel dropped draws as successful.

**Scheduling proposal, requiring orchestrator ownership:** reconcile the zero
drop requirement with ABI 1's drop/coalesce contract. Evaluate presentation
admission before enqueueing a drawable wait, then a bounded producer backlog or
explicit admission policy. Define memory/latency bounds and which work must be
preserved before changing slots or completion semantics. Re-run ordinary menu,
loading, combat, map/renderer restart and presentation-stall cases. `vid_restart`
is not certified by this workstream; synthetic resize/timeline continuity is
covered by platform fixtures.

## Leaks and object counts

The final normal exit reports **420 leaks / 26,288 bytes**: three framework
XPC cycles account for 411 objects / 26,112 bytes, and the same nine dvar
strings account for 176 bytes. No SDL display-mode array is reported in this
particular exit; the earlier run below proves that category can occur. No
leak-owning source/dependency repair was made, so the difference between exits
is not a leak-fix claim. **Zero leaks fails.**

Final warmed inventories reproduce 580 textures / 796 buffers / 88,077,160
provider buffer bytes on Toujane after repeated registrations. Dawnville uses
719 / 874 / 92,510,960, and returning restores the Toujane baseline. Ready
Toujane PSOs warm from 1,344 to 1,346 and then remain there. Pool bounds and
real GPU reuse are verified; complete all-object flatness is not measured.

The earlier completed exit check reports 446 leaks / 31,232 bytes, with no Metal
provider object among the reported roots: 417 objects / 26,496 bytes belong to
three AppIntents/LinkServices XPC cycles; five SDL display-mode arrays account
for 20 objects / 4,560 bytes; nine dvar strings account for 176 bytes. This does
**not** satisfy zero process leaks, and these categories are not suppressed.

The ownership evidence gives three separate follow-ups:

1. `Dvar_MakeExplicitType` in `src/PC/universal/dvar.c` saves all three old
   aliases and compares every free against those saved aliases. When current,
   latched and reset initially share one string, all frees are skipped and the
   owners are overwritten. The Mac i386 reference instead compares later frees
   against the already-updated live owners. A sequential live-owner fix belongs
   in a separately verified shared-engine repair; changing GL/OFF behavior here
   violates this brief.
2. Installed SDL 3.4.16's Cocoa mode data owns a CF array. Generic desktop-mode
   replacement frees the outer internal allocation without releasing that
   array; Cocoa shutdown can only release still-reachable arrays. Source and
   installed-library call inspection agree. See the primary
   [Cocoa mode implementation](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.16/src/video/cocoa/SDL_cocoamodes.m)
   and [desktop-mode replacement](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.16/src/video/SDL_video.c).
   Repair the pinned dependency or adopt a verified upstream correction; no
   installed library was modified and no private SDL-layout shim was added.
3. Framework stacks show `NSXPCConnection → xpc_connection → strong block →
   NSXPCConnection` cycles. The renderer has no owned connection handle to
   invalidate. An autorelease-pool change cannot break that strong cycle.
   Reproduce with a minimal SDL/Cocoa app and pursue an OS/dependency fix; do not
   invalidate framework-private objects or waive the zero-leak gate.

In `soak-ordinary`, warmed Toujane repeatedly returns to 580 textures, 796
buffers and 88,077,160 provider buffer bytes through more than three map
registrations. Dawnville uses 719 / 874 / 92,510,960; returning restores the
Toujane resource counts. Toujane PSOs are 1,344 initially, then 1,346 after
additional weapon/map preparation, and remain 1,346 on the next registration.
Loading inventories are transitional and were not compared to ready scenes.
This establishes return-to-baseline provider resources, not flat counts for
every Metal object: the newly bounded encode pools are not included in the
existing live inventory, and full all-object flatness remains unproved.

## Goldens and combat certification

Final-head Toujane (five WS35 views) and Carentan, Burgundy, Matmata,
Leningrad and Dawnville (three views each) all pass the unchanged full-image
WS24 diagnostic gates: MAE ≤1/255, RMSE ≤2/255, p99 ≤4/255, and fraction of
pixels with any channel error >16 strictly below 0.001. Each view has three
Metal captures; there is no masking, resampling or threshold change. All Metal
repeats are byte-identical. GL repeats are byte-identical except Leningrad's
spawn/weapon views: two differing pixels at x=458..487, y=370..371, worst GL
mean error 0.00001736 byte levels and no pixel error >16. Its repeatability
still passes the original gates; exact GL determinism is not established for
those two views. The cause is not attributed to a renderer change without
further evidence.

Carentan's spawn camera is a dim enclosed room (visually inspected from the
actual capture), providing interior coverage. These are quiet-map/held-rifle
diagnostics, not combat or full certification.

| Map/scene | Views × Metal repeats | GL repeats byte-identical | Worst MAE (byte levels) | Worst RMSE (byte levels) | Worst p99 (byte levels) | Maximum fraction >16 | Pixel gates |
| --- | ---: | --- | ---: | ---: | ---: | ---: | --- |
| mp_toujane (five WS35 views) | 5 × 3 | yes | 0.024351 | 0.748851 | 0.0 | 0.00068685 | pass |
| mp_carentan | 3 × 3 | yes | 0.001130 | 0.039225 | 0.0 | 0.00000000 | pass |
| mp_burgundy | 3 × 3 | yes | 0.002211 | 0.353147 | 0.0 | 0.00002387 | pass |
| mp_matmata | 3 × 3 | yes | 0.003920 | 0.171700 | 0.0 | 0.00002821 | pass |
| mp_leningrad | 3 × 3 | no (two pixels) | 0.003356 | 0.314418 | 0.0 | 0.00004015 | pass |
| mp_dawnville | 3 × 3 | yes | 0.012559 | 0.201747 | 1.0 | 0.00000543 | pass |

Normalized MAE/RMSE/p99 values are the table's byte levels divided by 255.
External `replay-*.json` files preserve binary/demo/scene/shader identity,
observed pose/time/equipment/settings and all repeat metrics. Commands:

```sh
python3 tests/metal/goldens.py replay build-macos/cod2_macos \
  --demo "$HOME/Library/Application Support/CoD2-native-ws35/record-heavy-fx/main/demos/ws35_reference.dm_1" \
  --scene-time-ms 20828 --fx-view 1234 2433 128.561996 149.699707 0 \
  --output "$ws37_evidence/replay-toujane"
# For each mp_carentan, mp_burgundy, mp_matmata, mp_leningrad, mp_dawnville:
python3 tests/metal/goldens.py replay build-macos/cod2_macos \
  --demo "$ws37_evidence/record-<map>/main/demos/ws37_<map>.dm_1" \
  --scene-time-ms '<target from record.json>' \
  --scene "$ws37_evidence/record-<map>/scene.json" \
  --output "$ws37_evidence/replay-<map>"
```

An additional `record-dense-fx` / `replay-dense-fx` attempt records actual
Sten firing (clip 32 →16), offhand preparation states after smoke/frag buttons,
and three final poses with three captures each. Its GL/Metal pixels pass
(maximum MAE 0.002063/255, RMSE 0.045441/255, p99 0, no >16 pixels), while GL
repeat mean differences reach 0.000912/255. Inspection shows an interior held
weapon scene; it does **not** prove a dense cloud, explosion or muzzle
flash. Live predicted clip 28 versus frozen replay clip 30 also demonstrates
why a recording request alone is not a complete combat-state receipt. These
files are retained as an unsuccessful density-certification attempt, not
renamed as a passing dense-FX golden. The command uses the same external-scene
replay path with the attempt's `record.json` target and `scene.json`.

The external-scene extension allows the same frozen-time/camera/settings,
GL-A/GL-B determinism and full-image comparison on more maps. It does not change
WS24 gates or declare diagnostic receipts to be certification. The full logical
draw/pass/resource/factory input hashes common to both renderers are still
missing, so strict certification remains blocked.

Dense ordinary FX is also blocked by an existing engine issue:
`FxHelper_AdjustTime` reads byte 8 of the dvar (flags) instead of
`fx_freeze->current.enabled` on LP64. With flags `0x1080`, the ordinary fixture
proves `mTime` remains zero. The WS35 opt-in replay clock corrects the boolean
only with `COD2_MAC_GOLDEN_DEMO_TIME`; it does not repair ordinary gameplay.
`python3 tests/metal/run_fx_clock.py` passes both the preserved ordinary path
and the corrected diagnostic path. Do not call replay smoke/explosions proof of
ordinary FX. A shared native engine-clock repair requires separate authorization
and GL/OFF verification.

The ordinary combat receipts establish actual rifle, SMG, spread and pistol
weapons, with clip reductions/reloads. The driver also sends frag and smoke
buttons, but button requests are not proof of grenade consumption, explosion,
smoke-cloud density or decals. The scoped rifle has only a held-weapon receipt
in the first soak. Rockets, mounted turrets and the item class remain unproved.
The licensed MP definition audit found rifle, SMG, pistol, spread, grenade,
rocketlauncher, turret and item classes. Bren/BAR are authored as rifles: they
must not be renamed to a separate MG-class acceptance row. No authored MP MG or
nonplayer-class definition was found. Runtime `give` of unregistered weapons is
not a reliable matrix driver: `G_FindItem` reaches weapon registration before
the give path sets `level.initializing`. A follow-up must prepare appropriate
stock-map weapon/turret contexts and verify actual weapon/class/ammo state.

Carentan provides a visually inspected dim interior view within the diagnostic
pixel gates. Dense smoke/explosions/muzzle flash/decals and the complete
class/frag/smoke matrix remain acceptance gaps. No default-renderer
recommendation follows from passing quiet-map pixels.

## Performance: exploratory before and after

The benchmark lock was acquired with PLAN rule 8's `mkdir` and released after
each measurement group. The display was an unlocked 6K 60 Hz panel; before and
after screen-state and process/display receipts are saved in each run. The Mac
was in use, with Python/Clang/build work and normal user/system processes active.
Every run is marked **contended, exploratory, not certified equal work**.

The 19-run matrix uses the same Toujane requested pose/settings, 15-second
measurement windows, render dimensions 1920×1080, 2560×1440 and 3840×2160,
caps 250 and 0, and isolated homes/ports. Metal console receipts confirm exact
dimensions; GL observer receipts confirm viewport/drawable dimensions. The
before binary is from `0e37c7a`, after CPU optimization from `af48088`. The later
destruction fix `22f23a2` is **not** benchmarked by this matrix. Commands and
lock/contention flags are in `performance.json`; representative invocation:

```sh
python3 tools/macos/live-bench.py "$HOME/Games/CoD2" \
  --binary "$ws37_evidence/after/cod2_macos" \
  --output "$ws37_evidence/after-metal-1920x1080-250" \
  --resolution 1920x1080 --maxfps 250 --seconds 15 \
  --set r_renderer metal --set net_port '<free-port>' --profile sample
```

| Renderer/version | Render size | Cap | Client fps | Client p99 ms | Mean main CPU ms | Presented fps | Presented p99 ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Metal before | 1920x1080 | 250 | 241.27 | 7.00 | 2.14 | 59.53 | 16.67 |
| Metal before | 1920x1080 | 0 | 779.40 | 4.48 | 1.28 | 58.73 | 33.33 |
| Metal before | 2560x1440 | 250 | 249.80 | 4.53 | 1.81 | 59.60 | 16.67 |
| Metal before | 2560x1440 | 0 | 794.83 | 4.51 | 1.26 | 59.40 | 33.33 |
| Metal before | 3840x2160 | 250 | 227.66 | 5.27 | 3.99 | 52.38 | 47.50 |
| Metal before | 3840x2160 | 0 | 261.16 | 4.91 | 3.83 | 53.86 | 46.74 |
| Metal after | 1920x1080 | 250 | 250.00 | 4.01 | 1.73 | 59.40 | 16.67 |
| Metal after | 1920x1080 | 0 | 906.31 | 2.83 | 1.10 | 59.60 | 16.67 |
| Metal after | 2560x1440 | 250 | 250.00 | 4.00 | 1.82 | 59.53 | 16.67 |
| Metal after | 2560x1440 | 0 | 854.87 | 3.01 | 1.17 | 59.47 | 16.67 |
| Metal after | 3840x2160 | 250 | 250.00 | 4.01 | 2.59 | 52.32 | 47.34 |
| Metal after | 3840x2160 | 0 | 470.90 | 3.36 | 2.12 | 53.33 | 50.00 |
| GL default | 1920x1080 | 250 | 239.45 | 6.64 | 0.99 | unmeasured | unmeasured |
| GL default | 1920x1080 | 0 | 617.31 | 6.39 | 1.06 | unmeasured | unmeasured |
| GL default | 2560x1440 | 250 | 237.93 | 6.66 | 1.07 | unmeasured | unmeasured |
| GL default | 2560x1440 | 0 | 459.88 | 7.67 | 1.53 | unmeasured | unmeasured |
| GL default | 3840x2160 | 250 | 237.90 | 6.57 | 1.41 | unmeasured | unmeasured |
| GL default | 3840x2160 | 0 | 402.10 | 6.97 | 1.53 | unmeasured | unmeasured |

Client-frame timing and physical presentation are distinct. Metal's presented
columns come from WS35 `CAMetalDrawable.presentedTime` receipts. Default GL
exposes a **swap-return proxy**, not physical display timestamps, so those cells
are left unmeasured rather than compared as if the panel displayed 250+ Hz.
An extra 1080p/cap-250 GL-renderer run using opt-in `r_present metal` provides
real display receipts: client 250.00 fps, p99 4.07 ms, mean main-thread CPU
2.05 ms; presented 55.43 fps, p99 45.99 ms, maximum 54.38 ms. It is not the
unchanged default GL presentation path and does not complete all-resolution GL
pacing certification.

Metal reaches the requested client cap in the after runs; 4K physical pacing
still has p99 intervals around 47–50 ms. The client improvements are useful
exploratory observations, not equal-work speedups: for example the after
1080p/cap-250 process encodes 175,787 draws and drops 439,437 over its whole
lifetime. Dropping work can lower client costs while degrading presentation.

The baseline sample attributes substantial encoder work to dense
`VertexSections` sorting (234 sorting samples within 331 section samples out
of 897 encoder samples), normalization (65), per-draw constant buffers/tables,
and digest `sscanf`. Optional phase timers show geometry/targets at 15.640 →
6.322 µs per call, uploads/tables 4.572 → 1.288, identity/declaration 6.182 →
4.847. Those phase means include loading and different admitted draw mixes
(539,072 vs 175,787 encoded draws); they are attribution evidence, not a
controlled per-draw before/after claim. State lookup averages remain 2.898 µs
in these aggregate logs despite the fixture proving removal of lookup heap
allocations. No shader/PSO compilation optimization or per-draw allocator churn
claim is made.

## Modern options: extension points only

Classic behavior remains unchanged; no new option is enabled or implemented.

| Option | Attachment point and contract |
| --- | --- |
| Render scale | Split scene dimensions in `metal_frame.m:StartWithGPU` from presenter/drawable dimensions. Refresh target descriptors/viewport/readback through `MacMetal_TargetResized`. Current scene and output are deliberately 1:1. |
| MetalFX spatial | Insert between the sealed pre-gamma scene mailbox and gamma/output in `macos_metal_present.m:EncodeNativePresentation`. SDK 26's spatial descriptor can create an `MTL4FXSpatialScaler` with the existing device/compiler. Prepare on loading/resize, retain resources through feedback, and encode on the existing transport. The current integer-read presentation shader cannot simply stretch by changing drawable size. |
| EDR | Existing opt-in `r_presentEDR`, float output layer, extended-range content and color-space controls already own output conversion. Add measured headroom/tone policy there after spatial scaling; keep pre-gamma BGRA8 scene/readback semantics. Screenshot parity alone cannot certify display EDR. |
| CAMetalDisplayLink / ProMotion | Already attached to the drawable worker. `preferredFrameRateRange(0,0,0)`, `preferredFrameLatency=1`, update drawable/target timestamps and `presentedTime` receipts are the extension points for display-aware pacing. Keep client cap independent. This 60 Hz host cannot certify 120 Hz/VRR. |

Resize preserves the same device, queue and event via `MacMetalGPU_Retain` /
`MacMetal_CreateNativeWithGPU`. Future scalers/output changes should use that
lifecycle rather than create another producer queue or reset resource epochs.
Any public ABI change is a proposal for the integration owner, not part of
this branch.

## Build and merge gate

Final-head stock/CoD2x builds and the full ABI checks pass. The sanitized
platform run passes **34/34** in 45.78 seconds. The first final plain run passes
34/36 and fails `metal_fullscreen` / `metal_spaces` at the desktop-mode-width
restoration assertion (`tests/platform/metal_present.m:240`). Its log is
retained. After waiting for the other game to exit, a complete plain-suite
repeat passes **36/36** in 40.14 seconds, without source/test changes. This
repeat establishes a passing run, not the cause of the intermittent display
restoration failure; that lifecycle issue remains a handoff risk.

Both Toujane renderer smokes exit 0 (`smokes.json`), including the native Metal
GL-escape trap. Commands:

```sh
cmake -S tests/platform -B build-macos/platform-tests \
  -DCMAKE_BUILD_TYPE=Debug -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build build-macos/platform-tests --parallel 3
ctest --test-dir build-macos/platform-tests --output-on-failure
cmake -S tests/platform -B build-macos/platform-tests-plain \
  -DCMAKE_BUILD_TYPE=Debug -DCOD2_PLATFORM_SANITIZERS=OFF
cmake --build build-macos/platform-tests-plain --parallel 3
ctest --test-dir build-macos/platform-tests-plain --output-on-failure
timeout -k 10 300 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer gl --home "$ws37_evidence/smoke-gl" --port 55696
timeout -k 10 300 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer metal --home "$ws37_evidence/smoke-metal" --port 62883
```

Both Release builds regenerate/verify the committed typed snapshot using the
available private inputs. Actual engine compile databases are saved before
fixture assertions are enabled with `-UNDEBUG`.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-macos --target cod2_macos --parallel 3
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
taskpolicy -b nice -n 19 cmake --build build-macos-codx --target cod2_macos --parallel 3
sh tools/abi/check.sh build-macos/compile_commands.engine.json "$ws37_evidence/gates/abi-stock"
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json "$ws37_evidence/gates/abi-codx"
git diff --check
```

Full ABI checks finish at zero: stock 635 TUs, CoD2x 652 TUs; 218 renderer
bindings each; 515 import symbols each, zero proven extra/missing dereferences.
The all-suite command ledger is `gates/results.json`: 49 successes and two
failures. GPU-dependent resource/state/target/shim/draw checks were repeated
after the destruction fix, all passing (`gates/final-gpu-results.json`).

Passed suite commands (logs named by suite under `gates/`):

```sh
python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/datagen -p test_datagen.py -v
python3 tools/abi/test_audit.py
python3 tools/abi/test_imports.py
python3 tests/rendering/shader_setup.py
python3 -m unittest discover -s tests/lp64/renderer -p test_wine_draw_trace.py -v
COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh
sh tests/cod2x/run_native.sh
sh tests/online/sdk_identity.sh
python3 tests/online/run.py build-macos/compile_commands.json
python3 tests/online/run.py build-macos-codx/compile_commands.json
python3 tests/fixes13/run.py --build build-macos
python3 tests/fixes13/run.py --build build-macos-codx
python3 tests/perf/run.py
python3 tests/lp64/game/run.py --build build-macos
python3 tests/lp64/script/run.py
sh tests/lp64/renderer/run.sh
sh tests/lp64/renderer/run_shader_raster.sh
sh tests/lp64/renderer/run_texture_mips.sh
sh tests/lp64/renderer/run_volume_upload.sh
python3 tests/platform/run_hud_matrices.py
python3 tests/platform/run_impact_marks.py
python3 tests/platform/run_fx_events.py
python3 tests/platform/run_fx_primitives.py
python3 tests/platform/run_fx_cloud.py
sh tests/platform/run_dedicated_fx.sh
sh tools/datagen/roundtrip_test.sh
python3 tests/lp64/game/check_layouts.py --build build-macos
python3 tests/fixes13/reference.py
python3 tests/fixes13/legacy.py --base 5cf411c
python3 tools/abi/legacy.py --base 5cf411c
sh tests/metal/run_core.sh
sh tests/metal/run_resources.sh
sh tests/metal/run_states.sh
sh tests/metal/run_targets.sh
sh tests/metal/run_target_shim.sh
sh tests/metal/run_draws.sh
sh tests/metal/run_draws_gpu.sh
sh tests/metal/shaders.sh "$HOME/Library/Application Support/CoD2 Silicon/shaders" "$HOME/Library/Application Support/CoD2-native-ws37/gates/provider"
sh tests/metal/raster.sh
sh tests/metal/fixed_run.sh
python3 tests/metal/setup.py
python3 tests/metal/observed_states.py
python3 tests/metal/run_tess_overflow.py
python3 tests/metal/run_demo_clock.py
python3 tests/metal/run_fx_clock.py
python3 tests/metal/run_map_lifetime.py
python3 -m unittest discover -s tests/metal -p test_goldens.py -v
sh tools/metal/run.sh "$HOME/Library/Application Support/CoD2 Silicon/shaders" "$HOME/Library/Application Support/CoD2-native-ws37/gates/translated"
```

Further final-head checks:

- All eleven remaining Metal runner/harness checks were repeated successfully
  after the final transport fix (`gates/final-extra-results.json`), in addition
  to the five real-GPU provider checks already listed.
- The CONTRIBUTING 100-frame parity recorder was also attempted with the CoD2x
  binary. An external `exec` wrapper adds a unique port and bounded window;
  the production recorder/parser remain unchanged. The engine exits and writes
  trace/entity files, but the recorder exits 1 at `statehash.txt:9` for
  noncontiguous frames or a map restart. It is a failed recording gate, not a
  valid 100-frame trace. Evidence: `parity-codx.driver.log`,
  `parity-codx-wrapper.json`, `parity-codx-100/`.

```sh
timeout -k 10 240 python3 tools/parity/record.py \
  --binary "$ws37_evidence/parity-codx-wrapper" --data "$HOME/Games/CoD2" \
  --output "$ws37_evidence/parity-codx-100" --frames 100
```

The two source/artifact failures in the original all-suite ledger are material
handoff notes:

- `tests/cod2x/check_inactive_gates.py --base 5cf411c` reports 17 mismatches in
  `metal_gpu.m`. It compares every modified C/header/ObjC body even in native
  **CoD2x-off**, so a CoD2x-independent Metal transport improvement cannot pass
  its current scope. Neither checker nor native behavior was altered to hide
  this failure. The fixes/ABI legacy checks pass. The orchestrator should scope
  the CoD2x-only checker explicitly, while retaining the actual OFF artifact
  gate.
- `tools/ci/compare-x86.sh 410342a HEAD ...` exits 2: it requires x86_64 Linux
  multilib. No such toolchain is available on this Mac and no package was
  installed. Native Metal sources are selected only by the existing
  `APPLE AND COD2_X64` CMake path; no OFF source/flag/blob was changed. That is
  source/build-selection evidence, **not byte-for-byte i386 artifact proof**.
  Run the Linux comparison before treating OFF identity as verified.

An independent code-review agent could not start on two attempts because model
capacity was unavailable. The implementer reviewed the diff and relied on the
red/green lifetime/allocation tests and real GPU checks; this is not presented
as an independent review pass.

## Orchestrator handoff

Merge the focused WS37 commits after WS35; there are no top-level CMake,
`src/headers/*`, ABI 1 integration header, GL or shared-engine source conflicts
introduced here. The main overlap is `metal_gpu.m` / `metal_draws.mm` with other
renderer work. Preserve the feedback/destruction ordering and independent
immutable constant/table snapshots when resolving any conflict.

Retain GL as default. Outstanding acceptance requires a transport/admission
decision, engine FX-clock and dvar ownership repairs, an SDL/framework leak
resolution, common strict receipts, a verified every-class/grenade combat
matrix, dense FX certification, strict interior receipts, uncontended equal-work
pacing at all three sizes, final-head performance repetition, resolution of the recording and
intermittent display-restoration gates, and the Linux i386 artifact
gate. None of these is waived by this report.
