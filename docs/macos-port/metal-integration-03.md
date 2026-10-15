# Metal 0.3 integration map — U1

2026-10-15. U1 is complete as recovery and documentation work. No renderer code
has been integrated, built, or certified by this unit. U2–U7 remain future work.
The user authorized U1 and requested a Rizin/REA decompilation plan; see the
[analysis plan](../plans/2026-10-15-cod2-binary-analysis-plan.md).

## Source and recovery identity

| Role | Immutable identity |
| --- | --- |
| Current main, refreshed from origin | `b3e6880056812e0951ba7bc6553b062dd2a6774e` |
| Metal donor, formerly `port/metal-cert` | `bf71e8ecd9985a57c69f27edf442516a8dd02074` |
| Verified merge base | `3ef770ba088808f87805ba748839db2ca53e9dad` |
| Pre-cleanup bundle SHA-256 | `db4559c5b684bcdd9b647b8ecbe328ab9755833e7f47f7c17d0dc5a155dae64c` |
| Final preparation bundle SHA-256 | `a194ea051088d4ab93fc2632422fea1d963da5fe87e5930d037ecccc40234377` |
| Metal checkout archive SHA-256 | `d41e58a9c01f148d8ca614d4bdc96295ea7b9772df8a4c29d1bc386b67590e49` |

Both bundle hashes were recomputed. `git bundle verify` reports complete
history; fetching the donor ref from that bundle succeeded. `git fsck --full
--no-dangling` with the donor commit completed successfully. The Metal checkout
archive was streamed without extraction: all 4,287 manifest entries matched,
and both archived patch files are zero bytes. Thus there is no tracked dirty
Metal delta to apply. Ignored build/reference material remains in the archive.
This checks the recovery dependencies used here, not every other removed worktree.

Machine-local recovery root (private, never publish its contents):
`~/Library/Application Support/CoD2-native-current-readiness/phase03-20261013.noindex/recovery/`.
Metal's manifest and payload are `worktree-30/manifest.json` and `files.tar.gz`.

Two app-managed worktrees now exist:

- Exact donor: `/Users/stix/.codex/worktrees/metal-donor-recovered/cod2-native`,
  branch `codex/metal-donor-recovered` at the donor SHA.
- Current-base documentation: `/Users/stix/.codex/worktrees/metal-u1-integration/cod2-native`,
  branch `codex/metal-u1-integration`, based on the current-main SHA.

The primary checkout remains on clean main. These paths are local convenience;
the SHAs and verified bundle are the recovery authority.

## Historical documentation

Seventeen donor documents are restored byte-for-byte: WS23, WS24, WS29–WS35,
WS37, WS39, the WS32 fixed/raster supplements, and the design, interface,
contract-audit and goldens documents. This includes all nine reports required
by U1 and their supporting Metal references. Their instructions, paths, version
floors and passing results describe historical work. Current CONTRIBUTING.md
and the [reviewed plan](../plans/2026-10-13-03-metal-readiness-plan.md) govern
new integration. In particular, an old “merge” instruction is not authorization
to merge the entire donor. Links to absent runtime files describe donor paths.

Use `git show bf71e8ecd9985a57c69f27edf442516a8dd02074:<path>` for exact donor
content. [The inventory](metal-integration-03-inventory.json) enumerates every
one of its 188 changed paths, donor/current blob identities, disposition and
integration owner. Main changed 630 paths from the ancestor. Their intersection
is 38 paths: six identical and 32 divergent. These are path counts, not merge
conflicts. No trial merge was used to infer behavioral compatibility.

## Integration slices and owning verification

Each slice starts from current main plus accepted predecessors. The inventory's
slice is its primary owner; seams can also need later consumers. Never copy a
shared engine file wholesale. Keep additions native-client gated, preserve
current object sizes and method signatures, and keep GL the default.

| Slice | Recoverable donor source and reconciliation | Owning verification after integration |
| --- | --- | --- |
| U2a-build | `cmake/macos-arm64.cmake`, CI additions, SDL Cocoa patch and stamp, `tests/platform/CMakeLists.txt`. Select source lists/framework/ARC additions while retaining current dedicated data filtering, release checks and macOS 26. | Current stock/CoD2x client and dedicated builds; platform CTest with and without sanitizers; package dependency/deployment audit. |
| U2b-dispatch-transport | `metal_{abi.h,backend.*,core.*,dispatch.m,gpu.m,frame.m}`, `shim_dispatch.h`, object index, frame stats, HRESULT include; all donor DirectX_9 hooks. Preserve typed parameter-address dispatch, creating-backend identity, sidecar ownership, single queue/event and completion retirement. Regenerate the method manifest from current signatures. | Donor `tests/metal/run_core.sh`, platform GPU/frame fixtures, `tools/metal/add_dispatch.py --check`, contract audit and no-GL guard; current `tools/abi/check.sh`, `tools/abi/legacy.py`, `tests/platform/directx_escalation.py`, renderer GL fixtures. |
| U2c-resources | `metal_resources.{h,m}`, `resource_formats.{h,m}`, buffer/texture/surface shim consumers. Reconcile descriptor/COM identities before DISCARD generation and NOOVERWRITE range behavior. Do not infer GPU lifetime from current CPU-buffer tests. | Donor `tests/metal/run_resources.sh` (resource/bridge/format/GL-buffer fixtures); current renderer `run_texture_mips.sh`, `run_volume_upload.sh`, `run.sh`; ABI checks in both modes. |
| U2d-display | `macos_metal_present.{h,m}`, display hooks/header, presenter tests. Preserve current backing dimensions, focus/Spaces, window notifications, error handling and GL lifecycle. One pending native drawable submission, completion-owned admission, safe late callbacks. | Donor `tests/platform/metal_present.m`, GPU/frame fixtures; current `tests/platform/fullscreen.c`, `focus_visibility.py`, `app_hide.m`, `window_state.swift`, platform CTest; actual resize/Spaces/quit with pending work. |
| U3-shaders-state | `arb_to_msl.*`, `arb_raster.h`, `shader_*`, `metal_states*`, state cache and `tools/metal` compiler/checkers. Match verified owned extraction manifests; retain ABI-1, loading-only compilation, complete state keys and atomic cache generation. Integrate with current `cod2x_native_shaders.m`. | Donor `tests/metal/shaders.sh`, `raster.sh`, `fixed_run.sh`, `run_states.sh`, `setup.py`; strict `check_semantics.py --all` on private corpus; current `tests/packaging/shaders.py`, `tests/rendering/shader_setup.py`, GL shader raster fixtures. Count translation, compilation, semantics and readiness separately. |
| U4-draws-targets | Target/shim/draw contracts, `metal_draws.mm`, `metal_materials.m`, `metal_integration.mm`, loading enrollment and engine renderer hooks. Keep current water, lighting, saved-screen and tessellation repairs. Adopt target copies, UP-buffer ownership, map-generation reset and typed live-readiness failure. | Donor `run_targets.sh`, `run_target_shim.sh`, `run_draws.sh`, `run_draws_gpu.sh`, `run_map_lifetime.py`, `run_tess_overflow.py`; current `run_water.py`, renderer `run.sh` (saved-screen/viewport cases), fixes13 world-lighting; exact package menu→map→quit. |
| U5-receipts | Donor goldens, observed states, demo/FX clock fixtures, gated capture/scene diagnostics. Add the reviewed plan's common logical receipt before backend encoding. Keep current centralized trace gating and gameplay/server-policy behavior. Existing pixel diagnostics are only a starting point. | Donor `test_goldens.py`, `run_demo_clock.py`, `run_fx_clock.py`; new planned `test_frame_receipts.py` (absent today); current FX cloud/events/primitives/impact-mark runners and camera tests; fresh paired six-map/interior/combat captures with matching receipts. |
| U6-observer | Donor frame-probe/live-bench/validation/summarizer, leak gate and bench-state changes. Reconcile with current process ownership and external exclusive bench wrapper; retain complete timing windows and dependency identity. | Current `tools/tests/test_bench_state.py`; donor leak-gate tests and presenter regression; controlled capped/uncapped physical-observer runs, ten-minute ordinary soak, whole-frame admission checks and lifecycle transitions. |
| U7-package | Current launcher/model/first-run/packaging owners; donor source is evidence, not a launcher replacement. Add requested/effective backend/refusal reporting and explicit Classic retry through current owned-process lifecycle. | Current launcher suites, packaging `release_smoke.py`, `first_run.py`, `launcher_bundle.py`, shader tests; exact final package on macOS 26 and later, early/newer Apple silicon as required by G7. |

Order: U2a establishes build boundaries; U2b/U2c/U2d form one coherent opt-in
object/resource/presentation baseline. U3 then supplies ready shader/state
providers, U4 connects scenes, U5/U6 close independent certification gaps, and
U7 consumes both. Provider stubs and NULL callbacks are not drawable readiness.
Run each slice's focused checks before broader current CONTRIBUTING.md gates.
Use actual engine compile databases, stock and CoD2x, and low-priority bounded
builds. U1 deliberately runs no build/game tests because it changes no runtime.

## Disposition of the 38 shared changed paths

All names below are repository-relative. The inventory supplies exact blob IDs.

| Paths | Decision |
| --- | --- |
| `.github/workflows/ci.yml`; `cmake/macos-arm64.cmake`; `scripts/build-sdl.sh`; `tests/platform/CMakeLists.txt` | U2a. Retain macOS 26 runner/floor, dedicated data view, trace and server tests. Add providers/tests and SDL patch selectively. |
| `src/Mac/DirectX_9/CDirect3D.c` | U2b. Preserve current device allocation and adapter handling while adding typed dispatch. |
| `src/Mac/DirectX_9/CDirect3DCubeTexture.c`; `CDirect3DVertexShader.c` in the same directory | U2b/U2c/U3. Retain native destructor adjustments, texture update and shader access repairs. Sidecars must use current C identities. |
| `src/Mac/DirectX_9/CDirect3DDevice.c` | U2b/U4. Preserve native Reset dimensions, draw diagnostics and saved-screen StretchRect; reconcile every hook against current signature/layout. |
| `src/PC/EffectsCore/FxSystem.c`; `src/PC/game_mp/player_use_mp.c`; `src/PC/qcommon/cm_load_obj.c`; `src/PC/qcommon/cm_world.c`; `tests/lp64/game/use_list.c`; `tests/lp64/game/world_links.c` | All six are identical to donor. Keep main; do not replay FX clock, use-list, temporary-box or broadphase repairs. Keep initializer/link invariants together. |
| `src/PC/EffectsCore/Fxexport.c` | Existing combat receipt already landed; main substitutes centralized trace gating. Keep main and extend only U5 receipt fields if needed. |
| `src/PC/cgame_mp/cg_consolecmds_mp.c`; `cg_event_mp.c`; `cg_view_mp.c` in the same directory | U5. Separate donor golden-pose/event instrumentation from current command policy, obituary/layout/camera changes. Diagnostics must use current trace guards. |
| `src/PC/client_mp/cl_ui_mp.c` | U4. Add loading/preparation seam without losing current UI lifecycle and typed server-info behavior. |
| `src/PC/game_mp/g_active_mp.c` | Donor takedamage repair already present; main adds trace guards. Retain main. |
| `src/PC/gfx_d3d/r_dvars.c` | U4. Add latched renderer selection without reverting current renderer-option ownership/defaults. |
| `src/PC/gfx_d3d/r_init.c` | U4. Reconcile map-lifetime pool reuse with current typed fields and guarded diagnostics. |
| `src/PC/gfx_d3d/r_screenshot.c` | U4/U5. Native readback/receipt hook must preserve current screenshot ordering, trace gating and dimensions. |
| `src/PC/gfx_d3d/rb_backend.c` | U4/U5. Add native fence routing; retain current command processing/text fixes and diagnostics policy. |
| `src/PC/gfx_d3d/rb_shade.c`; `rb_state.c` in the same directory | U3/U4. Retain WS75 water sampler/upload/generated-coordinate behavior; add context, shader state and GL escape guards around those corrected inputs. |
| `src/PC/gfx_d3d/rb_tess.c` | U4. Add bounded overflow behavior without replacing current typed entity handling. |
| `src/PC/universal/dvar.c` | Equivalent donor ownership repair already represented; retain main's live-slot freeing and later bounded domain formatting. Different text is not missing behavior. |
| `src/platform/macos_display.c` | U2d. Preserve current drawable dimensions, focused notification and Spaces fixes; reconcile layer/worker lifetime, no direct replacement. |
| `tests/fixes13/run.py`; `tests/fixes13/timing.c` | Retain current broader suite and stock/CoD2x timing expectations. Adapt donor seam dependencies only if compilation needs them. |
| `tests/lp64/game/run.py` | Retain current registration and expanded tests; identical use-list/world-link tests need no re-import. |
| `tests/online/challenge_resend.c`; `tests/online/run.py`; `tests/online/timeout.c` | Retain current protocol suite. Donor scaffolding/assertions are not evidence that newer network behavior should be rolled back. |
| `tests/platform/fx_call_test.c` | Preserve current trace stubs/guards; reconcile only required U5 diagnostic dependencies. |
| `tools/macos/bench_state.py`; `tools/tests/test_bench_state.py` | U6. Preserve current snapshot test coverage; evaluate donor `ioreg -l -w0` capture improvement against actual host evidence. Missing lock-state evidence must remain unknown. |

## Semantic dependencies beyond the overlap count

- WS94 `src/PC/gfx_d3d/r_bsp.c` and `tests/fixes13/world_lighting.c` are outside
  the 38-path intersection. Preserve named SunLightParseParams reads and all
  seven setters; do not emulate old lighting by modifying shaders.
- WS75 `r_water.c` is outside the intersection. Preserve spectrum zeroing,
  normal predecessor carry, typed dvar access and frame-count caching. Its
  CPU fixture does not close asynchronous buffer lifetime parity.
- Current `src/headers/`, typed-data generation, dedicated filtering and launcher
  files also matter even where the donor did not modify them. Confirm sidecar
  shells and vtable casts against current ABI/layout audits.
- Donor-only changes to `r_image.c`, `cl_cgame_mp.c`, `cod2x_native_setup.h`,
  `macos_display.h`, `carbon_stubs.c` and the other DirectX_9 objects still need
  review. “Donor-only-change” means main's blob stayed at the ancestor, not
  permission to transplant without considering its new callers.
- The donor state runner explicitly compiles for macOS 13.0. Replace that floor
  with 26.0 when importing it. Preserve main's 26.0 SDL stamp/build flags and
  pinned dependency/release checks; fold the Cocoa patch digest into the new
  stamp so an old cached dependency build cannot be reused accidentally.
- Donor Metal4 admission requires both macOS 26 and the used GPU capabilities.
  OS deployment floor alone does not establish device support.
- Donor all-target linking failed for the dedicated executable. Current main
  now uses `tools/server/dedicated_data.py` and a server-specific source graph.
  Retain it and verify the actual dedicated target after integration.

## Private inputs and unresolved evidence

The owned i386 Mac MP 1.3 reference is present and its hash matches WS75:
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The Steam Mac value reference is also present. Neither binary nor decompiler
output belongs in Git.

Machine-local existence checks found `~/Library/Application Support/CoD2 Silicon/shaders`,
`CoD2-native-ws39`, and the current readiness `release-live-20261012.noindex/bench_command.py`.
The historical `CoD2-native-ws35`, `CoD2-native-ws37`, and `CoD2x Native/shaders`
paths were absent. Recover archived evidence where available or rerun; do not
substitute an unrelated capture. Existence is not content/freshness validation.
The archive's 4,287 entries were verified, but no private cache or old runtime
result was promoted to current acceptance.

Keep these historical failures explicit:

1. Physical capped/ordinary-soak p99 was 33.334 ms versus the reviewed 18.333 ms
   target at controlled 60 Hz. The clean packaged-dependency soak lacked a
   physical observer; a filtered steady subset is not certification.
2. Logical draw/pass/resource digests and complete factory counters were absent.
   Six-map pixel comparisons remain `certified=false` diagnostics.
3. Dense visible combat/FX was incomplete: smoke consumption did not establish
   cloud creation, and one verified frag explosion was below the map.
4. The owned corpus has 208 vertex and 209 fragment programs plus 417 constant
   tables (834 files, 417 programs). Seventeen Apple-GL rejections require
   independent semantic oracles; translation/compilation success is insufficient.
5. Historical CoD2x recording rejected a noncontiguous/restarted trace; dedicated
   link and inactive-check failures remain historical limits. The Linux i386
   binary comparison still needs real results under the existing issue #10
   infrastructure waiver, not preprocessing substituted as byte equivalence.

U1 completion is limited to recovered source, verified archive, restored reports
and a complete path/slice map. GL remains default. Default promotion still
requires G1–G7 on the final integrated artifact; modern rendering options remain
sequenced after classic certification.

## U1 verification receipt

- Refreshed main and donor identities match the table above; both source
  checkouts are clean, with the recovered donor on its named branch.
- All 188 inventory rows were checked against the two Git trees. Classification:
  124 donor-only paths, 26 donor-only changes to existing paths, six identical
  shared paths and 32 divergent shared paths.
- All 17 restored historical documents match donor bytes exactly. The three
  new artifacts are this map, its JSON inventory and the binary-analysis plan.
- The resulting 20 additions are documentation only; there are no tracked
  runtime/config/test changes. Relative links in the two new documents resolve.
- Runtime/build tests are not applicable to U1 and were not run. Archive and
  source integrity do not imply renderer readiness or gameplay certification.
