# WS31 — Metal state normalization, ready caches and pipeline archives

Branch: `port/metal-states`, based on `351fe824`. This implements the states/PSO
provider against ABI 1 in new `src/Mac/Metal/metal_states*` files. The four frozen
integration headers, CMake, existing engine sources and GL implementation are
unchanged. The provider is tested independently; linking and bootstrap wiring
belong to the integration/draws workstream. Engine smoke tests still exercise
WS29's clear-only backend, not an integrated WS31 provider.

## Provider behavior

`metal_states.cpp` supplies numeric defaults, typed ABI-1 state updates and a
canonical snapshot. It validates supported enum ranges and finite float values,
normalizes boolean states and signed zero, and preserves raw getter values.
Sampler normalization retains minification versus magnification anisotropy,
clamps the supported degree to 1–16 and removes irrelevant border colors.
Texture-stage float words, all transforms and viewport depth/ranges are checked.
Target, texture, declaration, shader and stream identities are borrowed dynamic
bindings. Offsets and strides are copied; formats/layouts are resolved into the
pipeline recipe by the owning providers and WS33.

Keys contain an `MPS` magic, object kind, schema 1 and explicit little-endian
words. Pipeline keys include both source/metadata/interface SHA-256 identities,
the full bounded translator-revision and entry-point strings, register ABI,
strict compiler options, sorted declaration attributes and producer color
interpretation, strides, attachment formats, samples, topology, blend/write
state, alpha-to-coverage and complete specialization bytes. Fixed-function
variants also include all texture-stage words. Depth/stencil keys cover both
faces, masks and depth compare/write. Sampler keys cover addressing, filtering,
anisotropy, border, mip limit, sRGB selection, bias and displacement fields.

Viewport, transforms, bindings, offsets, cull/winding, stencil reference, depth
bias and shader constants remain draw state. Disabled blend and stencil fields
are canonicalized where they cannot affect the object. No C padding, addresses
or hash-only IDs enter serialized keys. FNV-1a chooses an unordered-map bucket;
full byte-vector equality decides every lookup. A deliberately constant hash
fixture proves that collisions preserve distinct entries.

The GPU context retains ABI 1's exact device/transport and creates no queue or
device. Loading owns a separate preparation compiler, needed because WS29's
compiler has no dataset serializer and its descriptor cannot be changed after
creation. `BeginLoading` freezes ready lookups, clears previous maps/misses and
takes a nonblocking namespace setup lock. `Prewarm` creates PSOs, depth/stencil
objects and all sixteen sampler states. Repeated recipes reuse existing objects.
`Publish` persists the dataset, releases the preparation compiler/serializer
and exposes only ready maps. A failed prewarm prevents publication.

Ready lookups return borrowed objects or NULL. They never call a factory, open
an archive or wait on the GPU. Valid misses retain full bytes in a bounded
1,024-entry queue; duplicates coalesce and overflow is counted. Counters expose
per-cache hits/misses, unique/coalesced/overflow misses, invalid keys, factory
creation/prohibition and archive load/hit/miss/invalid/write totals. Counters are
cumulative for the context; `BeginLoading` clears the miss queue. WS33 must drop
the affected draw and request a later loading transition or GL restart, as
specified by WS29. No synchronous draw-time repair exists.

## Persistence and fallback

Archives and optional pipeline scripts live only below the user's
`~/Library/Application Support/` directory. Absolute paths are required;
repository ancestors, app/bundle components and symlink escapes are refused,
including a `.git` marker in an existing hashed namespace directory. The
namespace includes the caller's shader-manifest/translator identity, schema,
ABI, strict options, device registry ID/name and OS version/build description.
The full identity is compared after locating its SHA-256 directory.

An exclusive `setup.lock` protects immutable UUID generation files. The JSON
manifest stores complete base64 key bytes, record counts and SHA-256 checksums
for every archive/script. Publication atomically replaces the manifest last;
obsolete provider-generated UUID files are pruned afterwards. Malformed counts,
duplicate keys, redirected payloads, incorrect identity/schema, bad checksums and unreadable archives
invalidate the dataset during loading. Writer and reader enforce matching
limits: 16 MiB manifest, 1,024 archive records and 65,536 keys per archive.
Individual payloads must fit CommonCrypto's UINT32_MAX-byte one-shot checksum
input; an unavailable checksum fails publication and loading validation.
Exceeding these limits fails publication rather than publishing an unreloadable
dataset. Pruning is best effort; an interrupted/failed publication can leave
unreferenced generation files until the next successful publication.

Next loading opens `MTL4Archive` objects. A matching full key first attempts
archive PSO creation; success increments `archiveHits`. Otherwise the loading
compiler uses `lookupArchives` and rebuilds. An archive file's presence is not
claimed to prove undocumented driver behavior. The fixture proves an actual
successful archive PSO creation on the second context.

The tested macOS 27.0.1 runtime returns NO, without NSError, when flushing a
serializer configured with `CaptureDescriptors | CaptureBinaries`. Changing
flush order or suffix did not resolve it. Binary-only capture succeeds and also
produces a pipeline script here; the implementation therefore requests binary
capture and treats scripts as optional. This is a verified deviation from
WS24's proposed combined configuration, not a converted failure. See Apple's
[dataset serializer](https://developer.apple.com/documentation/metal/mtl4pipelinedatasetserializer)
and [compiler](https://developer.apple.com/documentation/metal/mtl4compiler)
contracts. Runtime creation and round trips need no offline toolchain.

Metal 4 requires macOS 26+, a Metal 4 device family and successful object
creation. Before macOS 26, the documented fallback is GL; no Metal 3 archive
path is implemented. Deployment-to-13 availability compilation passes. Actual
older-OS execution was unavailable. `xcrun metal -v` reports the missing Metal
Toolchain, and `xcrun --find metal-tt` fails; nothing was installed. Offline
`metal-tt` preparation remains untested and unnecessary for this runtime path.

## Fixture evidence and observed Toujane set

Run the committed, original synthetic fixtures with:

```sh
sh tests/metal/run_states.sh --cpu-only
sh tests/metal/run_states.sh
python3 tests/metal/observed_states.py -v
```

The CPU fixture uses ASan/UBSan and pins a schema-1 default pipeline key to 502
bytes and FNV hash `75d1e20c15b52c30`, independently computed before pinning. It
checks stable attribute ordering, shader/producer identity differences, dynamic
state exclusion, separate alpha, disabled-state equivalence, stencil faces,
NaN rejection, signed zero, typed updates, raw getter preservation, enum versus
boolean normalization, hardware attribute bounds and min/mag anisotropy
distinctions. Regression assertions
were observed failing before their corrections. The GPU fixture also uses
ASan/UBSan with unguarded-availability warnings treated as errors.

Hardware-fetch attributes are checked against Metal's 31 descriptor slots,
0–30, before indexing any Objective-C descriptor array. The logical recipe can
still carry 32 attributes for buffer-based fetch. This follows Apple's
[feature limits](https://developer.apple.com/metal/limits/) and leaves existing
valid key bytes unchanged.

GPU results: duplicate prewarm creates exactly one pipeline, depth and sampler
object; ready hits return the same pointer; two identical misses become one
unique/one coalesced miss; 1,100 distinct misses stop at 1,024 queue entries and
count overflow. Depth and sampler misses each independently record/coalesce two
lookups. A factory attempt after publish fails without changing create
counts. Archive reload has `archiveLoads=1`, `archiveHits=1` and zero draw misses.
Corrupting the archive fails validation and rebuilds during loading; replaced
generations are pruned. Namespace changes are cold loads, competing setup locks
fail immediately, and forbidden cache paths are rejected.

A prewarmed synthetic triangle writes a 19×13 BGRA8 target. Every pixel is
`(204,102,51,255)`. The independent accelerated CGL reference matches byte for
byte: MAE, RMSE and p99 are zero; pixels over 16/255 are zero. This exceeds the
WS24 proposed tolerance gate. It proves synthetic PSO execution, not every
depth/stencil, sampler or blend mode's pixel semantics.

`tools/metal/observe_states.py` reads the existing native GL JSONL captures,
groups shader labels/topology/recorded state deterministically and records
capture SHA-256 provenance. It strips transient object/material identities,
records explicit assumptions for legacy invalid-zero enum slots, tolerates old
nonfinite shader-constant arrays without changing shader labels, and refuses
derived output in a repository or bundle. Missing captures exit 77. Seven
synthetic tooling tests pass without licensed input.

The verified local set was generated with:

```sh
python3 tools/metal/observe_states.py \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-full-final-courtyard.jsonl" \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-full-final-rooftop.jsonl" \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-full-final-sky.jsonl" \
  --output "$HOME/Library/Application Support/CoD2-native-ws31/observed-toujane.json"
sh tests/metal/run_states.sh \
  "$HOME/Library/Application Support/CoD2-native-ws31/observed-toujane.json"
```

The three captures contain 117 + 118 + 4 = 239 draws, 65 observed subsets and
22 shader-label pairs. Provenance digest:
`d46e6078e7e1857bb137157f3617dbc049d9df382cebfe40ecf4e22b535bf2f8`.
The synthetic projection prewarms 30 PSOs, two depth states and seven sampler
states. Every pipeline/depth/all-sixteen-sampler lookup for all 65 records hits:
**zero misses**. The result is recorded in `output/ws31/states-final.log` locally.

This is an incomplete capture, explicitly marked as such in schema 1. Existing
tooling omits shader source/metadata digests, declarations, strides, formats and
sample counts, separate-alpha 207–209, stencil, depth bias, viewport/transforms
and texture types. Shader labels tag synthetic specialization records and are
never passed off as original shader digests. The fixture supplies an original
synthetic library and declared defaults for missing fields. It therefore cannot
certify zero misses for fully integrated original Toujane shaders and layouts.
WS33 must fill those fields and capture actual ready misses before that claim.

To refresh, run GL with `COD2_MAC_DRAW_TRACE` set to an absolute request-file
path in Application Support. At each matched WS24 courtyard/rooftop/sky view,
write the absolute external JSONL destination plus newline into that request
file; the hook consumes it and captures the next frame. Use fresh homes/ports,
process checks and `timeout -k`, following PLAN. Re-run the command above with
the new captures after material/state/renderer changes, then prewarm at loading.
Compare provenance hashes rather than timestamps. Capture files, observed sets,
translated shaders, libraries and archives are never committed or bundled.

Ubuntu CI always runs the portable CPU and observer fixtures. macOS CI always
runs CPU tests and runs the GPU fixture on SDK/runtime 26+; older SDKs/devices
skip cleanly. `tools/metal/run.sh` now includes these synthetic fixtures. Its
default invocation skips only the private observed-set projection; synthetic
keys, caches, archive round trips and offscreen GPU execution still run.

## Regression gate

Both Release arm64 configurations build: stock `build-macos` and CoD2x
`build-macos-codx`, using the exact CONTRIBUTING CMake commands. Actual engine
`-DNDEBUG` compile databases were preserved separately; fixture databases use
`-UNDEBUG`. All commands, exits and logs are local in
`output/ws31/gate/results.json` and `output/ws31/gate/final-results.json`.

The CONTRIBUTING suites were all invoked. Passing groups include tools/datagen
unit tests, synthetic ABI/import checks, shader setup, Wine trace parsing,
sanitized/native CoD2x, SDK identity, stock online, CoD2x fixes13, source perf
fixtures, LP64 game/script, 23 renderer fixtures, GL shader raster/mips/volume,
HUD/marks/FX/dedicated FX, private datagen round trip/determinism, layout/retail
offset/weapon checks and fixes13 reference facts. Platform CTest passes 31/31
with sanitizers and 33/33 without. `tests/metal/run_core.sh` and dispatch manifest
checking pass. Updated `tools/metal/run.sh` passes synthetic state/tooling tests
and 417/417 translated executable programs plus 417/417 metadata records.

Two inherited failures remain, matching WS29: stock fixes13 cannot extract
preprocessed-out `Cod2x_LimitedFPS`; the isolated CoD2x online timeout fixture
does not provide `Cod2x_Frame`/`Cod2x_DemoClientFrame` link mocks. They were not
modified or counted as passes. An initial Wine parser failure was caused by
the gate harness placing TMPDIR inside the repository; correcting TMPDIR to
external `/tmp/cod2-native-ws31-gate` makes the unchanged suite pass.

Full function audits pass: stock 626 TUs and CoD2x 643 TUs, each with zero
errors, mismatches and new functions. Each callback check covers 218 renderer
bindings, with zero table/named-cast mismatches or unprototyped floating calls.
The checks use the preserved actual Release databases:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json \
  output/ws31/gate/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json \
  output/ws31/gate/abi-codx-parallel
```

Both full commands exit 0 without a timeout. Each private audit checks 515
import symbols and 185 placeholders, covering 1,777 native sites in stock and
1,779 in CoD2x, with zero proven missing or extra dereferences. The authoritative
CoD2x result is the exact parallel execution above; no duplicate full check or
private-import scan was performed.
Supplemental guards against `351fe824` pass: fixes13 legacy 10 configurations,
inactive CoD2x 17 and COD2_X64-off ABI five configurations, all zero mismatches
and zero changed existing source files. `git diff --check` passes. The actual
`tools/ci/compare-x86.sh 410342a HEAD` gate was invoked and reports its missing
x86_64 Linux/multilib prerequisite. This Mac cannot establish artifact-level
i386 byte identity; preprocessor guards are supplemental evidence only. No
existing build or engine input was changed by this branch.

Four Toujane smoke runs pass: stock and CoD2x under `r_renderer gl` and `metal`,
each menu, CS_ACTIVE, two screenshots and exit 0. Command pattern:

```sh
timeout -k 10 180 python3 tools/metal/smoke.py build-macos/cod2_macos \
  --renderer gl \
  --home "$HOME/Library/Application Support/CoD2-native-ws31/smoke-build-macos-gl" \
  --port 29631
```

The other three use their matching binary/renderer/unique home. Metal runs use
the generated `DYLD_INSERT_LIBRARIES` no-GL interposer covering 1,342 SDK/imported
symbols, including 126 engine imports; neither run traps. Evidence and game
screenshots remain under the external WS31 homes. No competing game process was
present before launches; the benchmark lock was held and released by this
workstream. The screen was locked, and parallel autonomous builds/audits/system
processes were running. These are correctness smokes; no FPS, pacing, compiler
latency or other performance claims are made.

Current stock GL menu and static courtyard screenshots also match WS29's
untouched `golden-static-before` baseline, each **0 / 3,686,400 decoded RGBA
bytes different**, maximum and mean difference zero. The ignored local Swift
CoreGraphics comparator is `output/ws31/compare-goldens.swift`; logs are
`output/ws31/gl-golden-menu.log` and `gl-golden-courtyard.log`. It decodes both
external JPEGs through the same RGB context and compares every byte. No image
was copied into the repository.

## What WS33 can rely on and what remains

No frozen ABI changes are proposed. The initialization-order problem is handled
locally: ABI tables must install before scene objects, while WS29's GPU transport
exists only after CreateDevice/presenter initialization. `InstallDefault` installs
callbacks first; the preparation callback lazily creates a context for that exact
transport. A full transport epoch change replaces that managed context during
the next preparation.

Integration sequence:

1. Under the native Apple/X64 target only, compile `metal_states.cpp` and
   `metal_states_gpu.mm`, enabling C++17/Objective-C++ and ARC for the latter. Preserve
   the existing i386/GL target inputs. No new library dependency is needed beyond
   existing Foundation/Metal and platform CommonCrypto.
2. Call `MacMetalStates_InstallDefault()` before Direct3DCreate9/scene objects.
   Once the transport exists, ABI-1
   `MacMetal_PrepareProviders(sourceCache,translatedCache,namespace,path)`
   prepares shaders first and begins states loading. The namespace supplied by
   WS32 must include its complete manifest/translator identity; path is external
   Application Support, never the bundle. `Current()` returns a borrowed context.
3. At loading, normalize complete state and submit each full recipe with WS32's
   verified libraries/entry identities to `Prewarm`. Its library arguments are
   borrowed actual `id<MTLLibrary>` pointers, not arbitrary opaque sidecar
   payloads; use the shader provider's library accessor. Resolve declaration/fetch
   layout, color producer contract, formats, sample/topology signatures and
   shader specializations using WS30/WS32/WS34. Call `Publish` before any draw.
4. At draw time, build the three key kinds, use ready PSO/depth/sampler lookups,
   and drop/coalesce a miss. Set dynamic raster/viewport/stencil/bias/constants
   and bindings in WS33. Retain every borrowed object used by a submission with
   `MacMetalGPU_Keep`; caches do not replace GPU lifetime ownership.
5. Snapshot statistics and full miss keys before a later loading transition;
   retain corresponding recipes in the integration layer. At shutdown, drain
   submissions and uninstall at an object-free boundary with `Install(NULL)`.
   This destroys a default-managed context; explicit contexts remain caller
   owned and must then be destroyed separately.

Loading rejects W-buffer depth, legacy non-ATOC alpha supersampling tokens,
arbitrary D3D border colors, displacement sampler fields and LOD biases not
exactly representable as Metal's S4.6 value. Those require explicit shader or
target emulation before support, never silent approximation. sRGB sampling/write
selection belongs to resource views/target formats; their resulting formats must
reach recipes. Fixed-function/alpha-test/fog behavior and specialization entry
semantics belong to WS32, not generic substitutions in this provider.
The recipe's specialization bytes identify an already prepared variant;
WS32 must bake its behavior into the supplied library entry points. Min/mag
anisotropy requests remain distinct keys, as allowed by Microsoft's
[D3D9 anisotropic filtering contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/anisotropic-texture-filtering).

Full original Toujane PSO coverage, all state-mode pixel parity and integrated
draw-time miss telemetry remain dependent on WS33 and the sibling providers.
Shared merge edits are limited to two synthetic runner lines and five CI lines;
all provider code is new. Do not merge generated local outputs. No packages,
remotes, sibling worktrees, pushes, PRs or issues were touched.
