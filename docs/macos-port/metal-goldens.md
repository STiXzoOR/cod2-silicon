# WS34 lossless WS15 camera harness

`tests/metal/goldens.py` captures courtyard, rooftop and sky in three serial
processes: GL, GL repeat, then Metal. It compares every RGB byte without
alignment, crop, exclusion masks, color adjustment or gamma conversion.
It requires the same executable and extracted-shader manifest for all runs.
Licensed screenshots, logs, receipts and comparison output stay outside every
git checkout, including symlinked paths. Nothing derived from the game belongs
in a commit.

This prepares the WS33 gate; it does **not** certify Metal camera parity today.
The WS29 engine has no draws. `COD2_MAC_DRAW_TRACE` executes on the GL path,
lacks complete frame/matrix/state inputs, and cannot be a Metal receipt.
The harness removes that environment variable and never fabricates its output.
The current engine can run the explicitly labeled capture smoke mode. Strict
capture and comparison fail without the receipt hook below.

## Exact scene and commands

The camera recipe comes from WS15's final matched-view and merged-tree sections
and the WS24 design. `setviewpos` accepts XYZ and yaw, not pitch. WS15 obtained
the sky pose by holding `+lookup` for four seconds and releasing `-lookup`.
The reported upward pitch is a positive magnitude; engine signed pitch must be
normalized to that convention in a receipt.

| View | Command | Actual eye | Yaw | Upward pitch |
| --- | --- | --- | ---: | ---: |
| Courtyard | `setviewpos 2569 2274 181 180` | `(2569,2274,182)` | 180 | 0 |
| Rooftop | `setviewpos 3051 2178 220 180` | `(3051,2178,221)` | 180 | 0 |
| Sky | `setviewpos 3051 2178 350 0`, `+lookup`, `-lookup` | `(3051,2178,351)` | 0 | 85 |

All three use Toujane DM, windowed 1280×720, FOV 80, gamma 1,
`r_ignoreHwGamma 0`, fog on, `r_normalMap Unchanged`, one AA sample, manual
picmip with `r_picmip`, `r_picmip_bump` and `r_picmip_spec` all zero, hidden
HUD/gun, noclip and released movement/look buttons. Dynamic entities and FX
remain enabled. This is the full scene recipe, so exact simulation/refdef time
and its dynamic contents must match; disabling entities/FX would weaken it.
No `D3D_PROG` override is used; the licensed ARB path is required by receipts.
Fresh private homes prevent old user configs from changing these settings.

From the repository root, run the original synthetic unit tests:

```sh
python3 -m unittest discover -s tests/metal -p test_goldens.py -v
python3 tests/metal/goldens.py --help
```

For capture smoke on the current clear-only engine:

```sh
python3 tests/metal/goldens.py capture build-macos/cod2_macos \
  --base "$HOME/Games/CoD2" \
  --cache "$HOME/Library/Application Support/CoD2 Silicon/shaders" \
  --output "$HOME/Library/Application Support/CoD2-native-ws34/camera-smoke" \
  --smoke-only
```

This produces nine lossless images and external command/console logs. Metal
images are expected to be black until draws exist. Exit 0 in this mode means
the requested files were captured and each game exited cleanly; it says
nothing about matched state, pitch, scene time or renderer parity. `viewpos`
logs only truncated eye XYZ/yaw, so they are diagnostic evidence only.
Smoke manifests are explicitly rejected by `compare`.

After WS33 wires the common receipt and freezes/replays a deterministic scene
at the chosen refdef time, run the strict gate. The time below is an example
chosen by the operator, **not** a verified Toujane frame:

```sh
python3 tests/metal/goldens.py capture build-macos/cod2_macos \
  --base "$HOME/Games/CoD2" \
  --cache "$HOME/Library/Application Support/CoD2 Silicon/shaders" \
  --output "$HOME/Library/Application Support/CoD2-native-ws34/camera-parity" \
  --scene-time-ms 10000
```

`session-<unique-id>.json` names the three fresh capture directories;
`comparison-<unique-id>.json` holds the result. Re-run an existing strict
session without launching a game:

```sh
python3 tests/metal/goldens.py compare \
  --gl "/external/session/gl-a-<unique-id>" \
  --gl-repeat "/external/session/gl-b-<unique-id>" \
  --metal "/external/session/metal-<unique-id>" \
  --output "$HOME/Library/Application Support/CoD2-native-ws34/recompare.json"
```

Repeat with `build-macos-codx/cod2_macos` and a separate output root. A single
comparison must use one exact binary; it cannot mix stock and CoD2x captures.

Capture checks installed GNU `gtimeout` (or GNU `timeout`) for `--kill-after`
and launches under `timeout -k 5s 240s`. It checks `pgrep -f cod2_macos` for candidate PIDs before
each process and waits up to five minutes if an actual game is running. `ps`
distinguishes game executables from build commands and this Python harness,
which also contain the binary name. It never kills another agent's process.
Failure cleanup terminates only its own newly created process group. Each
home/request ID is unique and every launch chooses a free UDP port. Missing
licensed game data or shader manifest exits 77 with `SKIP`; missing tools
fail explicitly and nothing is installed. No timing metrics or benchmark are
collected, so these commands are correctness captures, not performance runs.

## WS35 frozen replay diagnostics

The `replay` subcommand adds a diagnostic run with the same external `.dm_1`
on GL twice and Metal, several readbacks per view, and observed camera,
server time, weapon/ammo and HUD/gun controls at actual screenshot readback.
The common correctness cap defaults to 30 (`--maxfps`), independently of
the frozen scene clock:

```sh
python3 tests/metal/goldens.py replay build-macos/cod2_macos \
  --demo "/external/owned-recording.dm_1" --scene-time-ms 15941 \
  --fx-view 1946 1993 124.125 -60.3039551 0 \
  --output "$HOME/Library/Application Support/CoD2-native-ws35/replay"
```

Choose the time and FX pose from actual receipts in the recording. This
example describes the WS35 local recording; it is not a bundled fixture.
The optional FX pose uses engine signed pitch. Without it the FX diagnostic
uses the courtyard pose and does not establish that heavy FX are visible.

`COD2_MAC_GOLDEN_DEMO_TIME` advances ordinary demo playback by at most 50 ms
per frame to the exact requested time, then holds it. The native
`goldenpose X Y Z YAW PITCH [GUN]` command, enabled only by
`COD2_MAC_GOLDEN_REQUEST`, applies the engine camera. Optional `GUN=0/1`
sets the two diagnostic view controls internally: replay gamestate otherwise
cheat-protects the HUD/gun cvars. Receipts report their actual values. The
harness does not enable server cheats or alter the recording. The ordinary
`cl_freezeDemo` and timedemo clock paths remain separate, matching the Mac
reference. Since `42d9b4e`, the native ordinary FX clock reads the typed
`fx_freeze.current.enabled` value on both renderers. Ordinary gameplay FX now
advance; replay FX follow the demo frame steps and hold when those steps stop.
There is no separate golden-only FX-clock override.

The harness reapplies the same settings after loading the replay, waits for
the clock receipt and settling, then rejects wrong time, pose, view controls,
equipment, image size, uniform clear-colour images or ambiguous readback receipts. It checks GL repeats
before computing any GL/Metal metrics and returns failure if a pixel gate
fails. Images and recordings stay external. No crop, masks, registration or
color transformations are applied.

These outputs are explicitly `certified=false`. They lack complete logical
per-draw/pass/resource and factory receipts required by the strict schema
below. Near-identical pixels and exact camera/time alone cannot establish
that identical work was submitted, displayed gamma, zero drops, or heavy FX
coverage. The original strict `capture`/`compare` remains fail-closed.

WS37 adds `replay --scene /external/scene.json` for other maps and named views.
The external JSON has only `map` and `views`; each named view has `eye` (three
finite numbers), `yaw_degrees`, `pitch_up_degrees` and integer `draw_gun` (0/1).
It accepts 1–16 safe view names, rejects command injection/nonfinite poses,
checks the actual map-loading receipt against the recipe and binds the recipe
digest into the result. `--scene` and `--fx-view` cannot be combined. No map
coordinates or recordings are bundled. All existing repeatability, readback,
equipment and pixel gates still apply; this extension does not supply the
missing logical-input receipts or change strict certification.

## WS33 common same-frame receipt hook

**This is a proposed private diagnostic contract, not an ABI-1 addition.**
The harness sets `COD2_MAC_GOLDEN_REQUEST` to an external request-file path.
Before queuing `screenshot ws34-<view>`, it atomically publishes JSON containing
`schema=1`, a unique `request_id`, `view`, relative `image` and `receipt` paths,
the requested `scene_time_ms`, `expected_eye`, and
`expected_pitch_up_degrees`. The engine must treat the pose/time values as
expectations to verify, never copy them into observed evidence.

WS33/integration must implement all of the following before strict mode can pass:

1. Freeze or replay the **simulation, animation and refdef time** at the same
   selected `scene_time_ms` in every process, while still allowing scripted
   noclip pose changes and sky lookup to reach the clamp. The current
   `timescale` minimum is 0.001, so setting a tiny timescale is not a deterministic
   freeze. This harness does not pretend wall-clock sleeps establish matching
   scene time. If the requested time cannot be honored, reject the capture.
2. Observe the complete logical engine draw/pass inputs on both GL and Metal,
   before backend-specific depth conversion, batching and pipeline encoding.
   ABI 1 exposes raw shim state but not the engine's refdef camera or scene
   time. The integration owner must provide a narrow native diagnostic hook
   at the common engine frame/readback seam; no frozen ABI/header is changed
   by this harness. GL-only `lp64_draw_trace.h` is insufficient.
3. At `R_CaptureScreenshotNow` / actual pre-present readback, bind the pending
   screenshot's filename to its unique request and the same logical frame's
   inputs. Screenshots currently have a two-frame queue delay; an adjacent
   trace frame is not adequate. Metal readback may seal/drain submissions:
   frame identity must refer to the logical rendered image, rather than a new
   empty submission. Hash the actual RGB readback bytes after its orientation
   conversion and before encoding, with no gamma pass.
4. Atomically write the requested external receipt only after the actual image
   and state are available. Never synthesize shader identities, matrices,
   state hashes, scene time or camera coordinates from the request. Count
   dropped draws and factories invoked after scene readiness; both must be zero.
   The factory counter covers shader/library/compiler, pipeline, depth/stencil
   and sampler creation. Ordinary upload/constant buffers, argument tables and
   texture views are resources, and are not compilation/state factories.

The JSON receipt schema required by `validate_receipt` is:

| Field | Actual required evidence |
| --- | --- |
| `schema`, `abi` | Both 1 |
| `request_id`, `view`, `screenshot` | Exact request identity and image path, including `main/screenshots/` |
| `renderer` | Actual `gl` or `metal` backend |
| `encoding`, `readback_phase`, `width`, `height` | `engine-tga-rgb8-top-left`, `pre_gamma`, 1280, 720 |
| `map`, `settings`, `shader_path` | `mp_toujane`, actual dvars matching `goldens.SETTINGS`, `licensed_arb` |
| `frame_id`, `readback_frame_id`, `scene_time_ms` | Nonnegative actual frame IDs that agree within a capture; exact requested frozen/refdef time |
| `rgb_sha256`, `shader_manifest_sha256` | Hashes of actual RGB bytes and verified original extracted manifest bytes |
| `camera` | Actual `eye[3]`, `yaw_degrees`, `pitch_up_degrees`, `roll_degrees` |
| `matrices` | `view_f32le`, `projection_f32le`: each 16 original engine float32 values encoded as 128 lowercase hex characters |
| `logical_state` | Complete snapshot and ordered logical draw/pass identities, as described below |
| `camera_sha256`, `matrices_sha256`, `logical_state_sha256` | SHA-256 of the canonical JSON for the corresponding observed object |
| `dropped_draws`, `factories_after_scene_ready` | Both zero |

Normalize yaw to `[0,360)`, signed engine pitch to positive upward degrees
(`-AnglesNormalize180(pitch)`), and roll to signed degrees. Eye tolerance against
the recipe is 0.02 units; angle tolerance is 0.1 degrees. All matrix floats
must be finite and each matrix nonempty. Cross-capture camera and matrix hashes
must agree exactly. Preserve original float32 bits before backend wrappers;
do not hash a Metal-adjusted projection against a GL-adjusted one.

`logical_state` contains `viewport=[0,0,1280,720,0,1]`, `sample_count=1`,
`render[256]`, `samplers[16][16]`, `texture_stages[8][33]`,
`passes_sha256` and a nonempty ordered `draws` list. State words are unsigned
32-bit values. Each logical draw has a positive `primitive_count` plus
`geometry_sha256`, `shaders_sha256`, `constants_sha256`,
`resources_sha256` and `state_sha256`.

Those draw digests must cover normalized declaration/producer layout and
submitted index/vertex ranges, verified VS/PS source+metadata identities,
separate stage register bytes including local/state banks, texture/subresource
content identity, and the complete per-draw logical render/sampler/stage/
viewport/attachment state. `passes_sha256` must cover ordered target/depth
selection, dimensions and logical formats, clears with exact argument order/
float bits/masks/rectangles, copies and post operations. Hash stable logical
identities and bytes; exclude addresses, GL texture names, Metal resources,
submission values and backend-internal pass splits. Backend topology batching
must not change this engine-input receipt. The arrays alone describe the
terminal state; per-draw digests supply the changing state throughout the frame.

Use `goldens.canonical`: UTF-8, sorted object keys, compact separators, no NaN/
infinity. Hash raw float32 bits in byte-backed fields rather than normalizing
nonfinite register values numerically. The Python validator recomputes camera,
matrix and logical-state digests, verifies the image hash, and rejects absent
evidence. The receipt is a diagnostic trust boundary, not a signed attestation;
the orchestrator must review the capture hook to ensure its digests cover the
actual engine inputs listed above.

## Pixel acceptance and evidence limits

Every view must pass GL→GL repeatability and GL→Metal gates:
RGB MAE ≤1/255, RMSE ≤2/255, nearest-rank 99th-percentile channel error ≤4/255,
and **strictly fewer than 0.1%** of pixels with any channel error >16/255.
Metrics in JSON are normalized to `[0,1]`; the outlier count/fraction are also
recorded. Pose/matrix/state/scene-time mismatch fails before comparing pixels.
Entirely black Toujane frames and clear-only receipts are rejected. Fix GL
repeatability failures before using a Metal result; do not relax the gates.

The comparator reads the exact current engine lossless screenshot encoding:
`R_CaptureScreenshotNow(type=1)` writes RGB bytes directly into its TGA payload,
with top-left descriptor 32, rather than the usual TGA BGR order. It validates
the precise header and payload length and preserves those bytes. It performs
no pixel channel correction. A future screenshot-writer repair needs an
explicit encoding/schema change and corresponding synthetic tests.

This full-frame gate complements, rather than replaces, the WS34 GPU clear/
copy/depth/target/resize/gamma fixtures and WS33 thin-feature/HUD/alpha checks.
Pre-gamma screenshots cannot certify displayed gamma, MetalFX, MSAA or EDR.
WS15's Windows JPEG numbers remain historical evidence; this lossless gate
does not compare fresh images to JPEGs or claim those old captures were lossless.
