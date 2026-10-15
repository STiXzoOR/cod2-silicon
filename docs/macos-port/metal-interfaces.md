# Metal interface freeze — WS29 / ABI 1

These are the integration headers for the parallel resource, state/PSO,
shader, draw and target/post workstreams. They are native client interfaces,
not changes to the D3D9 COM contracts or engine/disk layouts. Keep implementations
in new `src/Mac/Metal/` files. The integration owner edits shim/CMake seams.

## Dispatch and object ownership

`src/Mac/Metal/shim_dispatch.h` defines `MacShimBackendOps` and the two backend
tables (0 GL, 1 Metal). `r_renderer gl|metal` is renderer-latched, archived and
starts at GL. Every nonempty exported shim object entry in
`tools/metal/shim_methods.json` calls `MacShim_Dispatch`. The GL continuation is
the existing typed method body. `tools/metal/add_dispatch.py --check` checks
coverage and the manifest. Empty compiler/container stubs have no operations.

`invoke(method, object, arguments, result, resultSize)` receives **addresses of
parameters**, in declaration order, including the receiver. Read each with its
actual declared C type; never cast a float/function pointer into `uintptr_t`.
`shim_methods.json` names parameters and return types; the source declarations
remain the signature authority. Return 1 after writing the return value and all
required output parameters, or 0 to continue into the original typed method.
The native implementation permits that continuation only for audited CPU-only
buffer/declaration/surface/capability methods. Unsupported HRESULT methods return
`D3DERR_NOTAVAILABLE`; unsupported scalar/pointer getters return typed zero.
`metal_hresult.inc` is generated and checked with the manifest so an error
bit pattern never becomes a pointer or float. Draws intentionally do nothing in
the core slice.

`MacShim_RecordObject` pins an object's creating backend independently of the
current selection. `MacShim_ForgetObject` removes it before address reuse.
`src/Mac/Metal/metal_backend.h` defines `MacShimKind` and opaque sidecars for
device, swapchain, surface, all texture types, both buffer types, declaration,
both shader stages, query and fence. Existing C object storage/vtables stay
unchanged; Objective-C ownership belongs to the sidecar payload. Engine COM
reference counts and GPU retention are distinct. Surface views retain their
texture while externally referenced. Providers must retain GPU objects with
`MacMetalGPU_Keep` and call `MacMetal_TouchSidecar` with the returned commit value.

`MacMetal_FindSidecar`, `MacMetal_SidecarKind`, `MacMetal_SidecarDesc`,
`MacMetal_SidecarPayload` and `MacMetal_SetSidecarPayload` are the sidecar
accessors. Payload destruction uses its C callback after its last submission
retires. All registry/state/provider calls occur on the single producer thread;
GPU callbacks never mutate the registry.
`MacMetal_SidecarObject` returns the original C identity; `MacMetal_SidecarBytes`
returns bounded borrowed CPU staging/source bytes. `MacMetal_CreateObject(kind,
desc)` creates an owned, zeroed C shell with the existing vtable and a sidecar
for provider implementations of creation methods (fences use their existing
constructor). `MacMetal_BindSurfaceView(texture, freshSurface, level, slice)`
transfers a fresh surface reference to its texture, enables the core level/face
getters and preserves external-view reference counts. `MacMetal_SurfaceParent`
returns that parent and mip/face identity. Providers initialize descriptors and
payloads before binding; bind rejects an occupied slot or an already shared
surface. Buffer/surface providers must implement their operations before
returning such a shell; its zero bytes are not a valid GL object. Replacing a payload follows the same
retirement rule. Do not store Objective-C pointers in legacy shader bytes.

## Device, submissions and presentation

`src/Mac/Metal/metal_core.h` is the GPU transport API. Presenter creation owns
one `MTLDevice`; `MacMetalGPU_Create` retains that exact device and owns one
`MTL4CommandQueue`, shared event and preparation compiler. Native rendering and
presentation share that transport through `MacMetal_NativeGPU`; no second queue
or device is created. macOS 26 availability, Metal4 GPU family and creation
success are all checked. Failure selects GL before creating scene objects;
there is no Metal 3 game backend. Existing GL-to-Metal interop presentation still
uses WS23's legacy queue and unmodified GL mailbox path.

`MacMetalGPU_Begin(gpu, role)` returns a slot or NULL immediately. Role 0 is the
renderer; role 1 is the drawable worker. Each role has two allocator slots.
BeginScene latches admission through Present. Exhausted producer slots skip
the whole client frame's GPU work (mailbox backpressure); retirement during
that frame cannot admit a suffix. Report these as **skipped frames
(backpressure)**, separately from partial-frame or lifetime/resource drops.
Only exhaustion of the two healthy producer slots is backpressure; failed
transport or provider/PSO/resource failures remain errors. Client movement and
network timing continue at the independent client cap.
`MacMetalGPU_Command` returns a borrowed `id<MTL4CommandBuffer>` begun with that
slot's allocator. Encode and end encoders before commit. `Keep` retains objects;
its `allocation=1` also adds `id<MTLAllocation>` to the slot's residency set.
Tables/PSOs use allocation=0; every texture/buffer they point to uses 1.
`Commit` ends encoding, commits residency, serializes queue operations, assigns
a monotonic value, commits and signals the one event. With a drawable it also
schedules `waitForDrawable`, then `signalDrawable` and `present` in SDK order.
The native presenter evaluates admission before that wait: at most one native
drawable presentation is pending GPU completion. A busy presentation
leaves the newest completed mailbox ready, and the next admitted update selects
the newest completed sequence. Physical presented callbacks report display
time without holding admission credit or referring to presenter storage. This does not change
the two allocator slots per role, feedback retirement or the ABI 1 headers.

GPU feedback retires the allocator/retained objects after execution, including
error completion. The shared-event value drives fences and sidecar retirement.
An error quarantines the transport against further submissions. Completion is
exactly once per committed slot, on the queue's serial feedback queue.
`Cancel` ends unsubmitted encoding and frees the slot. `Drain` is permitted only
for screenshots/lifecycle; it checks feedback completion and the event value.
No slot waits or drawable acquisition run in the client frame loop.

`MacMetalBackend_Submission()` acquires the current producer slot, possibly
NULL; `MacMetalBackend_SceneTexture()` returns its pre-gamma BGRA8Unorm target.
`MacMetalBackend_Present` copies the scene to a WS23 mailbox texture, seals it
before commit and publishes it only on GPU completion. Copy retains residency
for both textures even if no color clear occurred in that submission. WS23 retains the newest
completed frame, acquires drawables on its worker/display link, and drops older
ready images. Renderer and presenter clocks remain independent.

Resize/fullscreen are main-thread display lifecycle operations: cancel
producer encoding, stop/join display-link worker, drain sealed/present work,
remove the old layer and targets, recreate at exact requested dimensions while
retaining the **same device, queue and event value** through
`MacMetalGPU_Retain` / `MacMetal_CreateNativeWithGPU`. Fullscreen with unchanged
render dimensions only updates geometry.
Shutdown additionally releases the last transport owner before destroying the
SDL window. Only a full renderer shutdown starts a new timeline epoch; resize
never invalidates map assets or ring retirement values. The SDL Cocoa
empty-current-GL-context query is guarded in native mode; the original IMP is
used on GL restarts. This uses public Objective-C runtime APIs, no SDL internals.

## Register and argument-table ABI

`src/Mac/Metal/metal_abi.h` is byte layout authority, ABI version **1**:

| Structure/field | Byte offset | Bytes |
| --- | ---: | ---: |
| float4 (alignment 16) | 0 | 16 |
| ARBInput attrib[16] | 0 | 256 |
| texcoord[8], color[2], position, fogcoord | 256 | 192 |
| ARBInput total | 0 | 448 |
| ARBConstants env[256] | 0 | 4096 |
| local[256] | 4096 | 4096 |
| state[64] | 8192 | 1024 |
| ARBConstants total | 0 | 9216 |
| ARBOutput position,color[2],texcoord[8],fogcoord,depth | 0 | 208 |
| killed (uint32), explicit padding[3] | 208 | 16 |
| ARBOutput total/array stride | 0 | 224 |

VS and PS have separate environment/state banks; locals are per shader.
`MacMetal_WriteRegisters` validates first/count without overflow before copying
whole float4s, preserving float bits. No numeric conversion/transposition occurs.
MSL's final float4 alignment makes output stride 224 even though killed is one
uint. Semantic compute uses input buffer 0, constants 1, output 2. Raster tables
use input/fetch descriptor 0, constants 1, stream buffers 2..17; max buffers 18,
textures 16 and samplers 16. Texture/sampler slots retain original register
numbers and reflected 2D/cube/3D types. Stage tables and constant slices are
immutable for their submission. Typed null resources are the resource provider's
responsibility. Never reuse a table/slice merely because two CPU frames passed.

## Provider tables and starting points

Install `MacMetalResourceOps`, `MacMetalStateOps`, `MacMetalShaderOps` and
`MacMetalDrawOps` from `metal_backend.h` before any objects are created. Install
copies the table and rejects a version other than ABI 1 or installation after
objects exist. Callbacks must outlive the renderer. Null callbacks select the
core stubs. The tables are intentionally small and C-facing.
`MacMetal_PrepareProviders(sourceCache, translatedCache, cacheNamespace,
datasetPath)` explicitly invokes shader then PSO preparation during loading.
`MacMetal_LoadShader(sidecar, library)` checks ABI 1 and invokes the shader loader;
loading code supplies verified identity/reflection, not a draw. The core does not
invent shader names or cache paths.

**Resources:** `MacMetalResourceDesc` carries exact logical dimensions, levels,
format/usage, byte length and row/slice pitches. `shim` handles object creation,
views, metadata, locks and format-specific operations using typed argument
addresses. Buffer `lock` and `unlock` are wired directly; upload is a provider
entry for explicit staging, invoked through `MacMetal_Upload` (NOTAVAILABLE
without a provider). The core creates CPU-only buffers/declarations and
texture mip/face staging with surface identities. Replace staging with native
shared/private buffers/textures, compression/conversion, uploads and residency.
Implement DISCARD generations/NOOVERWRITE ranges in provider-owned rings, retire
by commit value, validate range arithmetic, and copy DrawUP bytes synchronously.
The resource and draw tables each expose `submitted(value)`, called on the
producer after every producer commit (present, screenshot or fence). Value 0
instead notifies cancellation of an unsubmitted producer slot: release pending
CPU references/ring reservations without assigning a last-use value. Track the
resources used while encoding, holding their COM references until the
notification; use this exact returned value to mark sidecars
and publish ring generations. The drawable worker uses the same timeline but
does not call these producer notifications.
No game texture is uploaded by this core slice. `MacMetalRetirement_*` is a
producer-thread queue, not a thread-safe global allocator.

**States/PSOs:** `MacMetalState` exposes raw render[256], sampler[16][16],
textureStage[8][33], transforms, viewport, targets, streams and two register
blocks. `update(state, method, arguments, result, resultSize)` can handle a shim state
operation before the core shadow stub, including writing HRESULT/output values.
`MacMetalPipelineKey` is explicit serialized bytes plus schema version, never a
hash of padding/pointers. `prepare` and `readyPipeline` are loading/lookup APIs.
Implement normalized state, complete key equality, ready PSO/depth/sampler maps,
Metal4 datasets/archives and a scene-ready factory prohibition. The core creates
only its original present PSO during startup and has no game PSO cache.

**Shaders:** retain original ARB source/reflection locally, then consume WS24
`MacMetal_PrepareShaderCache` after verifying extraction manifest/CTAB SHA-256.
Its output `<original-name>.metal` is **MSL source**, not a metallib or pipeline
archive. `MacMetalShaderLibrary` supplies ABI/stage (vertex=0, fragment=1), original name, source and
metadata SHA-256, translator revision, libraryPath/entryPoint and bounded
reflection bytes. `load` must validate these identities and compile/read the
local library during loading, returning a retained payload registered with its
release callback. `prepare` is the cache preparation entry. Current core shader
sidecars retain the original source; there is no translation or compilation
while drawing. Preserve `arb_evaluate`, `arb_vertex`/`arb_fragment` and the
register struct ABI above. Declaration-aware input, interpolation/derivatives,
local/state reflection and fixed function remain this provider's work. Never
commit/bundle translated shaders or caches.

**Draws:** `MacMetalDraw` receives raw D3D topology/count/index/base values,
state and borrowed active submission. `encode` is synchronous and may return
an HRESULT. `MacMetal_ReadyPipeline(key)` performs only the installed ready
lookup and returns NULL without a provider. It must validate indices/ranges, copy UP inputs before returning,
normalize declaration/producer color order, obtain a prepared PSO, allocate
immutable argument tables/constants, retain all referenced resources and encode
passes/barriers into the supplied command. A NULL submission from healthy slot
exhaustion means a latched whole-frame skip,
never CPU wait. The core stub counts and drops draws. All four primitive/indexed/UP entry points are decoded. `count` is the D3D
**primitive count**; `first` is start vertex/index as appropriate. `indexed` and
`userMemory` disambiguate the variants; UP stride is `vertexStride`, and
`indexFormat` is the original D3D format. `vertexBytes/indexBytes` are zero in
the core and must be calculated/validated by the provider using topology and
range semantics; zero does not make a non-null pointer safe. Patch draws return
NOTAVAILABLE.

**Targets/post:** start at the scene target, clear encoder, conservative
render→copy barrier and real pre-gamma readback in `metal_frame.m`. Present
preserves scene contents after a screenshot/fence commit; initialization and
engine color clears are pending until a producer slot can encode them. Implement
attachment identities/depth/stencil, target changes, masked/partial clear,
copy/stretch and gamma/post hazards through the resource/state/draw seams.
Resize refreshes the existing backbuffer descriptor/CPU surface dimensions;
the target provider replaces its payload from the scene accessor. Core clear
is full scene color only; depth/stencil and offscreen pass semantics
are not implemented. WS23's original gamma presentation stays separate from
scene screenshots. Optional MetalFX/MSAA/EDR follows classic parity.

## Verification and compatibility scope

`sh tests/metal/run_core.sh` verifies dispatch selection/recorded ownership,
retirement and register layout/bounds/bit packing. `tests/platform` adds
`metal_core` and `metal_gpu`; the GPU test executes exact NPOT BGRA clear,
allocator slot exhaustion/cancellation/reuse and monotonic completion on the
actual Metal4 device. `metal_frame` additionally exercises engine ARGB clear,
real RGB/RGBA readback and transport continuity over NPOT scene resize. Existing WS23 tests remain required. The renderer GL
fixtures, no-GL trapped Toujane smoke, GL before/after golden comparison,
full ABI and inactive-source gates are integration-owner checks.

The clear-only game is an integration skeleton. It does not establish shader,
resource, PSO, target/post or visual parity. Keep GL default until all those
providers and the design's full-frame gates pass.
