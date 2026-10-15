# WS24 — Metal backend design and local ARB translation

2026-10-04; worktree `~/Projects/cod2-native-wt/metal-design`, branch
`port/metal-design`, base `3ef770b`.

The design, contract inventory, native translator and real GPU differential
harness are implemented. **834/834 cache payloads verify; 417/417 executable
programs translate and compile in both render and compute forms.** The cache
contains 208 vertex programs, 209 fragment programs, and 417 constant tables;
metadata files are not executable shaders.

**400/417 programs pass GL-versus-Metal arithmetic tests.** The other 17
originals are rejected by this Mac's GL compiler, so their semantic equivalence
is blocked. This branch does not enable a Metal game renderer, implement PSO
archives, or establish full-frame visual/performance parity. The final merge
gate status is recorded below; it must not be inferred from shader coverage.

No existing engine, shim, CMake or shared-header file changed. New `.cpp`/`.mm`
files are excluded from the existing engine/legacy `.c` source globs. No sibling
worktree or WS23 branch was read. No packages, global configuration, remotes,
pushes, issues or PRs were touched. Extracted/translated shader payloads,
compiled derivatives and GPU results remain outside the repository.

## Delivered files and decisions

- [Contract audit](../metal-contract-audit.md): every shim file, method/state/
  resource semantics, exact native callsite inventory, GL omissions and trace
  limits; reproducible `tools/metal/audit_contract.py`.
- [Metal design](../metal-design.md): objects, PSO keys/loading, Metal 4
  pipeline datasets/archives, arguments/residency, dynamic rings, formats,
  passes, depth/coordinates/gamma, fixed function, GL fallback, goldens and
  staged workstreams. API claims cite official Apple docs and installed SDK
  headers. MetalFX spatial is feasible after parity; temporal needs motion
  vectors/history. EDR, display link, allocators/queues and GPU capture have
  explicit availability/ownership limits.
- `src/Mac/Metal/arb_to_msl.{h,cpp}`: separate parser, register IR and MSL
  emitter; no bundled program text. `shader_setup.{h,mm}` exposes the native
  C-callable local setup API. `tools/metal/{translate.cpp,prepare.mm,run.sh}`
  are standalone drivers; [usage](../../../tools/metal/README.md).
- `tests/metal/{parser.cpp,setup.py,semantic.mm}` and
  `tools/metal/check_semantics.py`: synthetic parser/setup checks and real
  headless CGL/Metal execution. Explicitly requested original GPU probes work
  without a licensed cache; licensed comparisons print a clear skip if absent.

The selected design keeps the shim contracts and GL parity reference rather
than introducing a second Metal 3 renderer or replacing material/caller APIs.
Metal 4 is the eventual supported-device default; ship GL until parity gates
pass. Loading creates/validates every actual PSO before readiness; a disk archive
alone cannot promise zero compilation. Prefer immutable draw snapshots and
completion-based resource retirement over frame-count heuristics.

Setup validates manifest SHA-256 values, program/metadata pairing and CTAB
bounds, emits MSL locally and compiles validation libraries. It releases those
libraries afterward and caches **source**, not GPU binaries. The next shader/
backend workstream must retain functions and create/link/cache all PSOs during
loading, and integrate this API after WS21's extraction. No launcher or game
first-run flow was changed here; WS25/WS23 are independent.

## Audit evidence and implications

All 33 shim files (12,161 lines), including the full `D3DXShader.c` and every
native shader header, were read. The native compile database audit processes
22 renderer translation units with **zero clang failures**. It finds 164
indexed indirect calls: 93 automatically recognized calls to 45 shim method
names, plus 71 manually categorized expressions whose types were erased to
`void *` or belong to query/CTAB/other dispatch. The current native configuration
has **90 device callsites and 41 distinct device methods**, including one
dynamic VS/PS creation slot. The PLAN's approximate 277 is not a verified count
for this preprocessed configuration; excluded Windows/legacy branches and
repeated helper invocations are separate.

Important seams to freeze before concurrent renderer implementation:

- Engine code directly performs APPLE fence operations, target draw/read-buffer
  changes, a texture prebind through object offset `0x54`, and screenshot GL
  readback. Replacing shim vtables alone leaves these on GL; use narrow native
  backend hooks while preserving the literal OFF path.
- Requested stencil draw state, clip planes, fill mode and anisotropy are
  incomplete/ignored by GL. The zero fixed-function stage-enum tables also
  prevent faithful combiner state. Test intended semantics with independent
  fixtures; GL screenshots cannot prove behavior it omits.
- Vertex layouts, packed producer color order, 16-bit triangle draws, four
  lightmap planes and 3D lighting volumes are real contracts. Palette/BC4–7/
  instancing declarations do not establish active use.
- Existing buffer locks and fence stubs do not protect GPU memory reuse. Metal
  needs range/generation retirement tied to queue completion.

Commands:

```sh
taskpolicy -b nice -n 19 python3 tools/metal/audit_contract.py \
  --compile-commands build-macos/compile_commands.engine.json \
  > "$HOME/Library/Application Support/CoD2-native-ws24/evidence/contract-final.json"
python3 tools/metal/audit_contract.py --trace-only \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-trace-courtyard.jsonl" \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-trace-rooftop.jsonl" \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-trace-sky.jsonl" \
  > "$HOME/Library/Application Support/CoD2-native-ws24/evidence/toujane-trace-summary.json"
```

Those actual older Toujane captures have **143/164/77 draws**,
**105,028/108,528/53,655 triangles**, and **15/15/14 recorded state sets**.
The parser explicitly normalizes their 286/328/154 nonfinite numeric values.
They are not WS15's final 117/118/4-draw matched captures and do not include 2D
draws or every resource/state path. No fresh live capture was taken while the
shared benchmark lock was held by another workstream.

The trace lacks geometry, declaration layouts, target/viewport transitions,
stencil/depth bias and separate-alpha factors. Extend its private capture before
claiming complete PSO prewarming or replaying a Toujane batch. The optional
captured lit-batch slice was therefore not implemented; numeric draw counts
cannot reconstruct its inputs. No performance measurements were made. Screen
lock state was not measured; other autonomous workstreams were active and the
shared benchmark lock remained present during checks. Numerical headless test
results are correctness evidence, not frame-rate results.

## Translator coverage and semantics

Coverage includes all **28 observed opcode spellings** (including saturation
forms), all observed signed/scalar/vector sources, swizzles, partial masks,
parameter arrays/ranges, environment registers, fog state, attributes and
outputs. Original programs declare an unused address register; synthetic GPU
tests additionally exercise ARL and indirect parameter lookup, local constants,
state bindings and fragment position. Used texture targets are 2D, cube and 3D;
TEX/TXP are tested with identical textures. General ARB features absent from
this cache are not a completeness claim.

Native setup compiles both the render entry and `ARB_SEMANTIC_TEST` compute
entry for each program: **834 successful library compilations, zero failures**.
All 417 CTAB files independently pass validation. Render-library compilation
does not establish linked VS/PS raster PSO coverage; that count is zero here.

`xcrun --find metal` locates the executable, but an actual invocation fails:
`cannot execute tool 'metal' due to missing Metal Toolchain`. No component was
installed. The authorized alternative, `MTLDevice.newLibraryWithSource`, works
on this Mac. See [Apple's runtime compilation documentation](https://developer.apple.com/documentation/metal/mtldevice/makelibrary(source:options:))
and the SDK `Metal.framework/Headers/MTLDevice.h` / `MTLLibrary.h`.
Xcode is 27.0 (`27A266a`), SDK 27.0; a runtime device query reports Apple M6,
`supportsBCTextureCompression=1`, `supportsFamily(Metal4)=1`. This does not test
Metal 4 queue submission, EDR or MetalFX output.

```sh
sh tools/metal/run.sh
python3 tests/metal/setup.py
python3 tools/metal/check_semantics.py --self-test --limit 0
python3 tools/metal/check_semantics.py --all --self-test \
  --evidence "$HOME/Library/Application Support/CoD2-native-ws24/evidence/root-final-semantics"
python3 tools/metal/check_semantics.py --self-test --cache /nonexistent
```

Parser tests pass ASan/UBSan. Native setup tests use an independently written
834-payload synthetic cache and prove accepted compilation, digest rejection,
CTAB-bound rejection, repository-output refusal and missing-cache exit 77.
Ten original GPU probes and three independent arithmetic oracles pass;
intentional GPU output corruption is detected.
The licensed comparison command correctly exits **1** for unresolved GL
references, rather than turning them into expected passes.

There are **76 raw opcode sets, 72 after stripping `_SAT`, and 77 distinct
stage/opcode sets**. The representative run passes **74/77**, with three GL
rejections and 6,968 finite component comparisons. Its largest absolute error
is `4.7683716e-7`; largest relative error `1.3439827e-7`.

The complete run passes **400/417**, with **51,040 finite components, zero
numerical failures and zero nonfinite outputs**. Acceptance is
`abs_error <= 2e-6 + 2e-6 * max(abs(GL), abs(Metal))`. All ordinary outputs
remain within `4.7683716e-7` absolute; `floatz_clear.pse` produces roughly
`5.5e5`, where its `0.0625` difference is one float32 ULP and relative error
`1.1376449e-7`. Full per-program/sample errors and GL diagnostics stay in
external `evidence/root-final-semantics/summary.json` and `*.result.json`.
They are not committed test outputs.

GL VP has no transform feedback here. The harness replaces OUTPUT declarations
with temporary registers, leaves the original instruction body intact, then
relays each written result through a float texcoord to an RGBA32F FBO. Fragment
programs execute unchanged. Both backends receive eight deterministic inputs,
the same environment/local/fog constants and varied RGBA32F nearest textures.
Unwritten output components are excluded; KIL includes discard/survival cases.
These tests do not cover implicit derivatives, mip selection, interpolation,
projection/clipping, linked raster interfaces or full material appearance.

An initial TXP case deliberately landed on exact repeat-boundary coordinates
through identity constants. It differed between GL's fastest-hint path and
Metal; ordinary varied constants remove that discontinuity without changing
instructions. Sampling boundaries remain an explicit raster follow-up, not a
claimed universal texture precision result. ARB precision/input conventions
are documented by the [vertex specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_program.txt)
and [fragment specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_fragment_program.txt).

Every unavailable GL reference is listed below. `NRM` is inferred as XYZ
normalization; undeclared `_abs` spelling is inferred as absolute value only
when the identifier is undeclared. These assumptions are retained in IR/CLI
metadata and setup prints `UNVERIFIED`; a genuinely declared `r_abs` stays an
ordinary register. None receives a same-original-program GL equivalence claim.
An additional original synthetic `DP2ADD` probe is also rejected by GL; the
16 `_abs` programs contain it, so repairing that identifier alone does not
restore their GL reference. Separate synthetic oracles compare the inferred
NRM, DP2ADD and `_abs` math against independently written valid ARB sequences.
Their results support the explicit mathematical interpretation, not execution
of the original rejected streams.

```text
fakelight_view.vsa                 NRM rejected
lprobe_sm_a_efog.pse               undeclared r0_abs
lprobe_sm_a_lfog.pse               undeclared r0_abs
lprobe_sm_a_s_efog.pse             undeclared r0_abs
lprobe_sm_a_s_lfog.pse             undeclared r0_abs
lprobe_sm_efog.pse                 undeclared r0_abs
lprobe_sm_lfog.pse                 undeclared r0_abs
lprobe_sm_s_efog.pse               undeclared r0_abs
lprobe_sm_s_lfog.pse               undeclared r0_abs
lprobe_smc_a_efog.pse              undeclared r0_abs
lprobe_smc_a_lfog.pse              undeclared r0_abs
lprobe_smc_a_s_efog.pse            undeclared r0_abs
lprobe_smc_a_s_lfog.pse            undeclared r0_abs
lprobe_smc_efog.pse                undeclared r0_abs
lprobe_smc_lfog.pse                undeclared r0_abs
lprobe_smc_s_efog.pse              undeclared r0_abs
lprobe_smc_s_lfog.pse              undeclared r0_abs
```

## Merge gate

Both stock and CoD2x Release arm64 client builds pass using the exact
`CONTRIBUTING.md` configuration/targets and `taskpolicy -b nice -n 19`, parallel
3. Save engine compile databases, then enable fixture assertions with `-UNDEBUG`
as the gate specifies. Do not use an all-target build for the inherited
dedicated-server issue; WS26 owns that target.

The full gate was attempted; **it is not wholly green**. Passing groups:

- Both arm64 client targets; all datagen/checker/manifest/Wine-trace unit tests;
  CoD2x full sanitized/native suites and SDK identity.
- Stock online fixtures, CoD2x fixes13 fixtures, performance source fixtures,
  LP64 game/script suites, renderer sanitized fixtures and real GL shader/
  mip/volume tests.
- HUD matrices, impact marks, all FX primitives/cloud/event fixtures, dedicated
  FX fixtures, and platform CTest: **25/25** with sanitizers and **27/27**
  without sanitizers.
- Private reference layouts/fixes13 facts and datagen round trip: original
  i386 bytes/relocations/symbols match, arm64 native/BSS/address-point probes
  match, and all 14 regenerated text files are deterministic.

Two unchanged fixture/configuration failures remain:

1. `python3 tests/fixes13/run.py --build build-macos`: its timing fixture tries
   to extract `Cod2x_LimitedFPS`, which the stock configuration preprocesses
   away. The earlier receive/snapshot/ABI/switch/storage/table/hotpath/trajectory
   fixtures pass. The complete suite passes against `build-macos-codx`.
2. `python3 tests/online/run.py build-macos-codx/compile_commands.json`: the
   isolated timeout fixture does not supply mocks for `Cod2x_Frame` and
   `Cod2x_DemoClientFrame`, and fails to link. Earlier text/userinfo/pure-IWD
   fixtures pass; the stock suite passes. Later CoD2x online fixtures are not
   reached by that command.

The tools unit suite initially hit tight timing/subprocess limits under the
shared Mac's background scheduling. Its final unchanged foreground rerun,
`python3 -m unittest discover -s tools/tests -v`, passes **14/14**. Production
and fixture compilation otherwise used the required background priority.
Logs/results are local under `build-macos/metal/gate/`; none are committed.

Both complete `sh tools/abi/check.sh` commands pass against the saved engine
databases and the private STABS reference:

| Check | Stock | CoD2x |
| --- | ---: | ---: |
| Translation units | 620 | 637 |
| Compiler errors / signature mismatches | 0 / 0 | 0 / 0 |
| Renderer bindings | 218 | 218 |
| Table / named-cast / unprototyped-floating findings | 0 / 0 / 0 | 0 / 0 / 0 |
| Native import sites | 1,776 | 1,778 |
| Proven extra / missing import dereferences | 0 / 0 | 0 / 0 |

Each import audit covers 514 import symbols and 185 placeholder objects.
The three guards against `3ef770b` also pass:

```sh
python3 tests/fixes13/legacy.py --base 3ef770b
python3 tests/cod2x/check_inactive_gates.py --base 3ef770b
python3 tools/abi/legacy.py --base 3ef770b
git diff --check
```

They compare modified existing files, of which this branch has **zero**;
their zero-mismatch result is not an artifact-level i386 comparison.

Every tracked change relative to `3ef770b` is a new file. Existing engine/test/
build source is identical to that base, and the saved stock/CoD2x engine
databases contain no new Metal source. Full linked Linux i386 object/binary
comparison could not run on this Darwin arm64 host without the Linux multilib
environment; no packages were installed. The passing data round trip and OFF
preprocessor guards supplement, rather than replace, that unavailable gate.
No live smoke was taken because this branch does not wire new runtime code
into either client and another workstream held the shared benchmark lock.

## Proposed next workstreams and merge instructions

| Scope | Owns / acceptance |
| --- | --- |
| Backend boundary / integration owner | Freeze native device/resource sidecars, dispatch, register ABI and submission timeline; guard direct engine GL escapes; availability and empty offscreen clear/readback; minimal shared-file edits |
| Resources | New buffers/textures/surfaces, upload/swizzle/BCn/mips/3D, lock/discard/no-overwrite rings, completion retirement/residency; GL synthetic readbacks |
| States / PSOs | State shadow/key equality, depth/sampler caches, material/observed permutation loading, Metal 4 datasets/archives; prohibit factories after readiness |
| Draws | Declaration-specific stream fetching, packed colors, topology/base/index validation, constant snapshots and argument tables; original synthetic raster parity |
| Shaders | Integrate local setup, resolve the 17 original GL-invalid cases against the licensed source/reference, linked raster wrappers, derivatives/LOD and fixed-function set; no distributed derived code |
| Targets / post | Real depth/stencil/render targets, pass/clear/copy/readback hazards, gamma/shadows/bloom/channel mixing; WS15 camera goldens |
| Presentation integration | Read WS23's design only after merge, then settle one device and queue/drawable ownership; display-link/resize/fullscreen parity |

Resources, states and shaders can proceed concurrently after the integration
owner freezes interfaces. Draws join their validated outputs; target/post
fixtures can be authored concurrently. Spatial MetalFX/MSAA/EDR follow classic
SDR parity; temporal velocity research is separate. Proposed all-pixel lossless
GL→Metal gates are MAE <=1/255, RMSE <=2/255, p99 channel error <=4/255 and
<0.1% pixels with any channel error >16/255. These are design thresholds, not
measured Metal imagery.

Merge this branch's focused commits in order; nothing edits the other workstreams'
shared runtime files. All commits have the repository DCO sign-off. Keep the
native library/tool out of OFF source globs, and add native target integration
deliberately in the next branch. The source/metadata/semantic output directories
are private and must never be merged, bundled or committed.
