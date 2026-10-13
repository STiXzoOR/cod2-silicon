# WS88 — native player bind translations and rendered body

Date: 2026-10-07. Source baseline: `8f17eaf`. Owned branch:
`codex/player-render-fidelity`. Live overlays used the stable composed stock and
CoD2x object sets at `6e9a75f`, replacing only `xanim.c` and `dobj.c`.

## Result and acceptance boundary

The reproducible collapsed local third-person body has a verified skeleton
cause and a small native-only correction. Native `DObjCalcSkel` omitted each
child bone's model bind translation before composing it with its parent.
Animated child bones with absent or zero translation tracks therefore clustered
around the root. The corrected stock and CoD2x engine captures show a complete
local torso, arms and legs; separate movement frames show changing leg poses.
The first-person hands and weapon also remain visible after firing and reloading.

A matched stock before/after pair uses the same British Africa 03 body, head,
helmet, weapon, outdoor position and camera settings. This removes the random
model variant and wall-adjacent framing ambiguities in WS86. At range 100 the
static actor's feet reach the lower image edge; the range-140 movement captures
include the complete actor and feet.

This closes the observed local body collapse. It does **not** establish complete
animation, material, lighting or remote-player parity with Mac 1.3. Bots are
present in the server/status evidence, but the attempted spatially pinned bot
capture did not identify a rendered bot. Remote/bot full-body visual acceptance
remains open. Event names, bone counts and the existence of test clients are not
used as substitutes for viewed model pixels.

## Diagnosis and reference behavior

The investigation followed the actual production chain:

`CG_Player` → entity submission → renderer pose/bounds evaluation →
`CG_DObjCalcPose` → `DObjCalcAnim` / controllers / `DObjCalcSkel` →
`R_SkinXModelCmd` / `R_SkinXSurfaceSkinned` → backend draw.

The submitted entity scale is initialized to one. First-person hands/weapon
already rendered in WS86, and a clear outdoor camera still reproduced the local
body fragments. Those observations narrowed the investigation but did not alone
identify a skeleton or upload defect.

The private owned Mac 1.3 reference is
`/Users/stix/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`.
Raw `otool -tv` disassembly establishes two coordinated behaviors:

- In `DObjCalcSkel`, `0x75fe1`–`0x7602c` adds the child model's bind translation
  to its animation displacement before the parent transform.
- In `DObjCalcAnim`, the unanimated child fallback at
  `0x3d92f`–`0x3d940` clears the displacement, while keeping the model quaternion.

Native code already composed parent rotation/translation, but lacked the first
operation and placed the bind translation only in the unanimated fallback. A
fix to skeleton composition alone would have added bind translations twice for
unanimated children. Both locations are corrected together under
`#if defined(COD2_X64)`: every child receives its model bind translation once;
the fallback supplies zero animation displacement. Root, quaternion, controller,
duplicate-bone and renderer code are unchanged.

Private pose diagnostics corroborate the source defect with the same pinned
111-bone, four-model local actor. Eighteen sampled completed poses per run have
these aggregate model-space Z ranges:

| Run | Minimum sampled Z | Maximum sampled Z |
| --- | ---: | ---: |
| Stock baseline | -18.3247 | 8.73225 |
| Stock corrected | -0.645166 | 66.3865 |
| CoD2x corrected | -0.224133 | 66.5667 |

The samples record local entity zero. They are supporting evidence for the
viewed before/after captures, not a visual acceptance test. The instrumentation
exists only in private generated overlay sources; it is absent from the patch.

## Regression and compatibility checks

`tests/lp64/renderer/player_skeleton.c` exercises the real
`DObjCalcAnim` → `DObjCalcSkel` → `R_SkinXModelCmd` chain using synthetic model
and animation data. The runner inherits each build's actual engine compiler
flags and enables ASan/UBSan and assertions. It compiles the actual source,
rather than a reimplementation of the transform.

The independent geometric expectation uses a root rotated 90 degrees about Z
and translated to `(10,20,30)`, with child bind translation `(3,4,12)`:

- An animated child without a translation track must reach `(6,23,42)`.
- An additional animation displacement `(1,2,3)` must reach `(4,24,45)`.
- An unanimated child must receive its bind translation once.
- A second call for an already completed skeleton must not add it again.
- Actual CPU skinning must consume the composed transform in both renderer
  formats.
- Multiple roots, a rotated child and a deeper hierarchy retain independent
  geometric results through partial masks and repeated completed updates.
- Attached models and duplicate bones copy the expected parent transforms.
- Local controller displacement and an absent bind-translation array preserve
  their expected transforms. This checks transform mechanics, not tag lookup.

These last three groups were added during independent review and retained by
integration as regression cases.

Reading production sources from `8f17eaf` produces a genuine baseline failure
in both stock and CoD2x: the first child position is `(10,20,30)`, not
`(6,23,42)`. Both corrected builds pass all cases under ASan/UBSan. No licensed
asset is needed by this regression.

Reproduce from the repository root, using fresh private output directories:

```sh
python3 tests/lp64/renderer/run_player_skeleton.py \
  --compile-db <stock-build>/compile_commands.engine.json \
  --baseline 8f17eaf --output <private-output>/baseline-stock
python3 tests/lp64/renderer/run_player_skeleton.py \
  --compile-db <stock-build>/compile_commands.engine.json \
  --output <private-output>/fixed-stock
# Repeat both commands with the CoD2x compile database and separate outputs.
```

Additional focused checks:

| Check | Result |
| --- | --- |
| Existing `model_bounds.c`, ASan/UBSan | PASS, including four nonuniform rotated bone boxes |
| `tools/abi/legacy.py --base 8f17eaf` | Two files, five COD2_X64-off configurations, zero mismatches |
| Legacy comparison with actual stock SDK flags | Four client/dedicated commands with COD2_X64 removed, zero mismatches/errors |
| Legacy comparison with actual CoD2x SDK flags | Four client/dedicated commands with COD2_X64 removed, zero mismatches/errors |
| `tools/abi/raw_offsets.py` with focused actual stock/CODX databases | Each: two sources, 15 reviewed sites, zero unsafe/unreviewed sites |
| `git diff --check` | PASS |

`tests/cod2x/check_inactive_gates.py --base 8f17eaf` was also run, and its failure
is retained: two files, 17 configurations, 14 mismatches. This checker expects
CoD2x-only changes to disappear when CoD2x is inactive; this correction
deliberately changes native stock as well as native CoD2x. It also includes a
`COD2_X64=0` configuration, whereas the existing native guards test whether that
macro is defined. CONTRIBUTING requires CMake OFF to omit the macro. The
applicable legacy-off comparisons above pass; no gate or approval list was
weakened. Full composed builds/ABI gates are the integration owner's checks.
No Linux i386 byte-identical binary comparison was performed by this workstream.

## Private live reproduction and viewed captures

All licensed inputs, extracted disassembly, QA copies, generated sources,
executables, screenshots and logs remain outside git under:

`/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws88-player-render.noindex`

The read-only installed data root is `~/Games/CoD2`. `build_overlay.py` compiles
only the two owned translation units, relinks a private executable against the
stable composed object set, and records source/executable hashes in its
`manifest.json`. It does not rewrite shared engine objects. The private pose
trace is injected only into these generated sources.

`run_live_locked.py` acquires
`/Users/stix/Projects/cod2-native-wt/_agents/bench.lock` by exclusive file creation
and removes it only when the recorded PID is its own. Every recorded run
exited zero, released the lock, and recorded no remaining `cod2_macos` process.
The shared object and GUI slots were released to the integration owner after
the captures.

`pinned_capture.py` creates a fresh private `ws43qa` home. Its QA-only
`mptype/british_africa.gsc` copy selects Africa 03, keeping the full precache
function; installed and repository game scripts are not modified. It launches
windowed 640×480, FPS cap 60, non-pure DM, four client slots, port 29966,
`devmap mp_toujane`. It reads the actual server ID/menu configstrings, submits
the server-info close, British allies and Sten responses, then adds two test
clients. Exact launch commands and receipts are in each run's `results.json`.

The static matched camera uses `setviewpos 2286 2110 140 90 0`, third-person
angle zero and range 100. Twelve outdoor/static views additionally cover three
locations and four orbit angles. Walk/run capture commands use the same
starting position, angle zero and range 140, followed by forward movement
with/without `+speed`. The separate JPEGs are 0.3 seconds apart.

Private replay commands, run sequentially while holding the shared slot:

```sh
evidence_root='/Users/stix/Library/Application Support/CoD2-native-current-readiness/ws88-player-render.noindex'
python3 "$evidence_root/build_overlay.py" --mode stock --label <fresh-baseline-overlay> --baseline
python3 "$evidence_root/build_overlay.py" --mode stock --label <fresh-fixed-overlay>
python3 "$evidence_root/build_overlay.py" --mode codx --label <fresh-fixed-codx-overlay>
python3 "$evidence_root/run_live_locked.py" <fresh-baseline-run> <fresh-baseline-overlay> pinned_capture.py
python3 "$evidence_root/run_live_locked.py" <fresh-fixed-run> <fresh-fixed-overlay> pinned_capture.py
python3 "$evidence_root/run_live_locked.py" <fresh-fixed-codx-run> <fresh-fixed-codx-overlay> pinned_capture.py
```

Every path below is relative to the private evidence root. Its screenshot
suffix is `qa.noindex/movement-trace-spawned/home/ws43qa/screenshots/`.
The selected engine JPEGs were opened and inspected, including the bot attempt.

| Run directory | JPEG | Viewed evidence |
| --- | --- | --- |
| `pinned-baseline-stock` | `ws88-box-approach-0.jpg` | Helmet/weapon fragments at the ground/lower edge; no complete body |
| `pinned-fixed-stock` | `ws88-box-approach-0.jpg` | Same pinned model/camera; torso, arms and both legs visible |
| `pinned-fixed-stock` | `ws88-pinned-run-1.jpg`, `ws88-pinned-run-2.jpg` | Complete actor/feet and distinct moving leg poses |
| `pinned-fixed-stock` | `ws88-pinned-walk-1.jpg`, `ws88-pinned-walk-2.jpg` | Complete body during slower forward movement |
| `pinned-fixed-codx` | `ws88-box-approach-0.jpg` | Complete local body in CoD2x |
| `pinned-fixed-codx` | `ws88-pinned-run-1.jpg`, `ws88-pinned-run-2.jpg` | Complete actor/feet and distinct moving leg poses |
| `pinned-fixed-codx` | `ws88-pinned-walk-1.jpg`, `ws88-pinned-walk-2.jpg` | Complete body during slower forward movement |
| Both corrected directories | `ws88-firstperson-after-reload.jpg` | Hands and Sten visible; ammunition 32/188 after earlier 32/192 |
| Both corrected directories | `ws88-pinned-bot0.jpg` | Empty courtyard; no identified bot body accepted |

The private bot QA thread logged `bot0`, entity one, and repeatedly requested
origin `(2321,2310,140)`. The first-person camera at `(2321,2170,140)`, yaw 90,
with the local gun hidden did not yield a visible bot. Its session/spawn,
snapshot and culling state was not isolated by this bounded run. This is an
unresolved coverage limit, not evidence that the local bind fix solves or
causes remote submission behavior.

`verification-receipt.json` records the selected JPEG SHA-256 values, exact
paths, source hashes, pose ranges and run receipts. `baseline-stock` /
`baseline-codx` retain red fixture compile/run output; `fixed-stock` /
`fixed-codx` retain green output. `compile-stock.json` and `compile-codx.json`
retain the focused actual compiler entries used by the compatibility checks.
The private unpinned exploratory runs are supplementary and are not the
matched acceptance pair.

## Patch scope

Production changes are 18 added lines in `src/PC/xanim/dobj.c` and
`src/PC/xanim/xanim.c`, entirely behind the native guard. Two synthetic test
files and this report are added. Nothing is staged or committed by WS88.
The correction preserves intentional CoD2x behavior and leaves COD2_X64-off
preprocessed source unchanged. The remaining acceptance boundary is identified
remote-player visual coverage and broader animation/material comparison,
followed by the composed integration owner's full build and ABI gates.
