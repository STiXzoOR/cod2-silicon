# Native Metal renderer behind the D3D9 shim

WS24 design, 2026-10-04. This is an implementation proposal, not a renderer
or performance acceptance result. The implementation and measured shader
coverage are recorded in [the WS24 report](reports/WS24-metal-design.md).
The evidence below separates current engine behavior, documented API
capabilities, and proposed behavior. No WS23 branch or sibling worktree was
read, and this design does not depend on its unmerged presentation work.

## Decision and alternatives

Keep the engine's `CDirect3DDevice_*` functions and resource vtable contracts.
Add a native-only backend dispatch boundary in the shim, with implementations
in new `src/Mac/Metal/` files. Use Metal 4 on supported macOS 26+ devices;
retain the existing OpenGL backend on macOS 13–25 and for explicit parity
testing. In implementation, check OS availability before querying
`supportsFamily:MTLGPUFamilyMetal4` or creating Metal 4 objects; also handle
creation failure. Metal 4 availability and the capability query are supported
by [Apple's device documentation][device] and SDK
`M/MTLDevice.h:256,883,1382–1488`. Do not infer support from a marketing chip
name or OS version alone.

Three approaches were considered:

| Approach | Benefit | Cost / decision |
| --- | --- | --- |
| Native Metal 4 behind the shim, existing GL fallback | Keeps engine and material contracts; explicit command/resource lifetime; matches Phase 2 direction | Requires ARB shader and fixed-function emulation, pass tracking, and explicit synchronization. **Selected.** |
| Implement Metal 3 first, then replace submission with Metal 4 | More familiar encoders; possible Metal renderer on macOS 13+ | Creates a second Metal submission/caching implementation and delays the requested Metal 4 path. Keep it as a contingency, not the first workstream. |
| Replace engine renderer call sites / material format | Freedom to redesign the renderer | Broad shared-file changes and a new asset/shader contract; loses the existing GL parity boundary. Rejected for this phase. |

Use `r_renderer gl|metal`, latched across renderer restart. Until full parity
acceptance, default to GL in shipped builds. Switch the default to Metal on
supported devices only after the gates below pass. If startup preparation
fails, report the failed shader/pipeline and select GL before scene execution.
A mid-frame backend switch cannot preserve GPU resources or partial passes.
This is a rollout decision; Phase 2's intended final default remains Metal.

All production integration must be behind `COD2_X64 && __APPLE__` and arm64
guards, with new native sources added only to the native target. Keep existing
OFF preprocessing byte-for-byte unchanged; run the ABI and merge gates when
integration starts. WS24's design file changes no build or runtime behavior.

## Contract boundary and current GL reference

The detailed method/state inventory is in the [WS24 contract
audit](metal-contract-audit.md) and report;
the design must consume its machine-readable inventory rather than infer
coverage from declared D3D9 interfaces. Declarations, no-op shim methods,
engine call sites, and states exercised by a particular frame are four
different sets. A Toujane trace is useful evidence of one state set, not proof
that all maps, menus, mods, shadow passes, or fixed-function paths are covered.

The existing code establishes these boundaries:

| Contract | Current evidence / design consequence |
| --- | --- |
| Opaque COM-like objects and reference counts | `CDirect3DDevice.c`, texture/surface/buffer/shader constructors and vtables. Preserve method signatures, slot order, ownership, returned descriptors, and lock pitches. Avoid storing Objective-C pointers in 32-bit fields or shader payloads. |
| Shader reflection and register writes | `D3DXShader.c`, `lp64_shader_cache.h`, `CDirect3DPixelShader.c:283–292`. Reflection resolves engine constants and samplers; pixel constants currently update `program.env`. Keep metadata and register namespaces rather than converting every shader to one generic lighting formula. |
| Declarations, streams, index bounds | `lp64_shader_draw.h:48–139`, `lp64_buffers.h`. The GL ARB path consumes declarations, stream offsets, stride, base vertex, normalized packed types, and defaults missing attributes to `(0,0,0,1)`. Preserve the checked index range, not merely Metal's out-of-range handling. |
| Color byte order | `lp64_color_order.h`; WS15 cached-model BGRA versus skeletal ARGB repair. Treat color order as declaration/producer semantics; never swizzle all streams or all RGBA textures uniformly. |
| Sixteen sampler slots and typed textures | `lp64_shader_draw.h:141–165`, constructors for 2D/cube/volume textures. Preserve 3D light-grid slices and four directional lightmap planes. Bindings need texture dimensionality as well as slot number. |
| Raster and alpha state | `lp64_shader_state.h`, `lp64_shader_draw.h:167–195`, `CDirect3DDevice.c:2221–2315`. The native GL draw path applies culling, RGB/separate-alpha blend equations, alpha test and depth. Stencil render-state setters are ignored; the clear path only clears stencil and temporarily changes its write mask (`CDirect3DDevice.c:1647–1699`). Metal needs stencil test/operation implementation beyond current GL behavior. |
| Target changes, copies, gamma and screenshots | `CDirect3DDevice.c:2785–2819,2892–2975`, `src/platform/macos_display.c`, WS15 gamma fixtures. Preserve attachment contents across changes, pre-gamma screenshot timing, and per-channel gamma ramps. |
| Trace evidence | `lp64_draw_trace.h`. Trace requests produce one JSONL frame outside the repository in Application Support, with state/constant/texture labels, rather than shader bytecode. Use the trace to seed local PSO preparation and match geometry, never pair Windows and native draw numbers blindly. |

Some existing shim methods are stubs and some getters differ from setters
(for example, `GetDepthStencilSurface` currently returns `depthBuffer`, while
`SetDepthStencilSurface` assigns `depthStencil`). Preserve engine-required
observable behavior, but do not deliberately replicate an untested GL bug.
Give each such difference a fixture and an explicit compatibility decision
before integrating it. Report unsupported methods with their HRESULT rather
than claiming full D3D9 support from the vtable's size.

The audit also finds GL calls **outside the shim**. Keep the approximately
277 COM engine call sites unchanged, but add narrowly guarded native backend
hooks for these escape paths before enabling Metal:

- `rb_backend.c` uses `glGen/Set/Test/DeleteFenceAPPLE` for frame/dynamic-buffer
  synchronization; `CFence` and vertex-buffer fence methods are currently
  stubs, so replacing them alone does not remove the engine's GL fences.
- `rb_state.c:1031–1044` sets `glDrawBuffer`/`glReadBuffer` after COM target
  selection; the shim's setter currently only stores the target pointer.
- `rb_shade.c:2130–2177` reads a GL texture ID through a hardcoded object
  offset and prebinds it. That backend-private read must never execute on a
  Metal object.
- `r_screenshot.c` performs GL state-stack/readback work. Route screenshot
  capture through a backend hook with the same pre-gamma timing.

Native hooks dispatch to the current GL operation or the Metal timeline/pass/
readback equivalent. Preserve the literal OFF source path. The object-model
approach still avoids rewriting the COM callers, but a fully native renderer
cannot leave these external GL operations running against an absent context.

The existing trace does **not** capture complete replay inputs: vertex/index
bytes, normalized declarations, viewport/attachment identity and all stencil/
depth-bias state are missing. Its geometry labels/counts suffice for correlation
and observed-state analysis, not for reproducing a textured Toujane draw.
Optional batch replay needs a separate private capture extension and cannot
be claimed from `lp64_draw_trace.h` JSONL alone.

WS15's final matched views demonstrate the real shader path, four-plane
lightmaps, 3D light-grid upload, sky cube coordinates, rasterization and gamma
repairs. Its last firing blocker is superseded by WS18's producer-argument
repair and grounded combat soak. Neither report certifies all postprocessing
or all live FX appearance. Those omissions remain test cases for Metal.

## Objects and ownership

Use a small C-facing backend operations table. Existing exported entry points
dispatch through a native device/backend handle; resource methods use their
own recorded backend. A resource cannot cross devices or backends. Preserve
the public/native shim object layouts wherever existing code casts them:
first vtable pointer, reference count, and engine-visible fields remain C
storage. Keep Objective-C ownership in backend-private sidecars or appended
native-only storage after proving every allocation size. Audit direct casts
before choosing between those two storage strategies.

Each command submission retains its referenced sidecars until GPU completion,
even if the engine releases its final reference earlier. Surface views retain
their owning texture. Binding changes release old C references consistently;
deferred GPU retention is separate from the COM count. Add use-after-release,
resize, and lock-overlap fixtures before drawing game assets.

| Shim object | Proposed Metal representation | API evidence |
| --- | --- | --- |
| `CDirect3D` / `CDirect3DDevice` | One `MTLDevice`, one `MTL4CommandQueue`, state shadow, pass tracker, resource registry, preparation-only compiler and ready PSO map | [Device][device], [queue][queue]; `M/MTLDevice.h:1382–1488`, `M/MTL4CommandQueue.h:209–266` |
| `CDirect3DSwapChain` / backbuffer | Stable C surface identity referring to offscreen scene color and, at final presentation, a drawable texture; no drawable retained across idle frames | [Metal layer][layer], [next drawable][next-drawable]; `Q/CAMetalLayer.h:77–108` |
| `CDirect3DSurface` | Owning texture reference plus mip, cube slice / 3D plane, logical dimensions, D3D format, CPU lock staging and dirty region | [Texture][texture], [pass attachments][pass]; `M/MTLTexture.h:395–427,535`, `M/MTLRenderPass.h:76–94`, `M/MTL4RenderPass.h:32–39` |
| 2D / cube / volume texture | `MTLTexture` with corresponding type, exact declared mip count, upload staging; separate immutable sampler cache | [Texture][texture], [samplers][sampler]; `M/MTLTexture.h:23–32,141–174`, `M/MTLSampler.h:107–216` |
| Vertex / index buffer and `CMemoryBuffer` | CPU-visible shared `MTLBuffer` for streaming or staging; optional private immutable buffer after upload; keep length, format, dirty ranges, lock epoch | [Buffer][buffer], [storage][storage]; `M/MTLDevice.h:680–695`, `M/MTLResource.h:58–87` |
| Vertex declaration / vertex array packet | Immutable normalized declaration and stream layout; `MTLVertexDescriptor` for raster PSOs, constant input stream for missing attributes | [Vertex descriptor][vertex], [step function][step]; `M/MTLVertexDescriptor.h:95–147`, `M/MTL4RenderPipeline.h:167–174` |
| Vertex / pixel shader and constant table | Locally generated `MTLLibrary` / function descriptors plus original reflection, IR identity and used-binding map; shader pair forms a render PSO | [Compiler][compiler], [render pipeline][pipeline]; `M/MTL4Compiler.h:101–108,156–193`, `M/MTL4RenderPipeline.h:158–165` |
| `CFence`, buffer retirement, event-query completion | Monotonic submission value on `MTLSharedEvent`; CPU tests value for buffer reuse; GPU barriers/fences for actual resource hazards | [Shared events][event], [queue][queue], [barriers][barrier]; `M/MTLEvent.h:55–64`, `M/MTL4CommandQueue.h:261–275`, `M/MTL4CommandEncoder.h:48–148` |

The table is a proposed mapping, not a claim that those objects are already
implemented. `COpenGL` caches, GL names, APPLE VAOs and APPLE fences have no
role in the Metal branch. Reusable CPU conversion/math code can remain shared
where it has no GL side effects. Shader constructors must preserve metadata
without uploading an ARB program to a GL context when Metal is selected.

## Pipeline keys and preparation before play

Serialize state fields explicitly into a versioned key, with stable integer
encodings and full key equality after hashing. Never hash C structure padding,
object addresses, material pointers, or transient shader IDs. Key fields:

- SHA-256 of vertex/pixel source and constant metadata, translator revision,
  register ABI revision, and strict-math compiler options.
- Normalized declaration, stream stride/layout, packed color interpretation,
  topology class and shader output/interpolation contract.
- Active color formats, blend enable/factors/operations for RGB and alpha,
  color write mask, raster sample count, alpha-to-coverage mode, fixed-function
  emulation variant, and any function constants used in its implementation.
- Attachment compatibility signature, including depth/stencil format and
  count, even where a field is validation state rather than a baked Metal 4
  pipeline descriptor property.

Metal 4 exposes shader/declaration/sample/topology properties and per-color
blend/format/write-mask descriptors; these are the reason the corresponding
fields participate in the key. [Render pipeline][pipeline], [color attachment
descriptor][blend]; SDK `M/MTL4RenderPipeline.h:44–89,156–199`.

Keep separately cached depth/stencil and sampler keys. Keep viewport, scissor,
cull/front winding, stencil reference, depth bias, constants, stream offsets,
texture identities and dynamic buffer ranges in a draw snapshot. The encoder
has setters for these raster/depth inputs, while depth/stencil state and
samplers have distinct descriptors. [Render encoder][render], [depth/stencil
descriptor][depth], [sampler][sampler]; SDK
`M/MTL4RenderCommandEncoder.h:88–214,620–628`,
`M/MTLDepthStencil.h:36–65`, `M/MTLSampler.h:107–216`.

Start with fully specialized, observed variants. Metal 4 unspecialized blend
state is not a free draw-time blend toggle: it behaves disabled until
specialized. Additional specialization also creates pipeline states, so keep
it out of gameplay. [Metal 4 compiler][compiler], [blend state][blend-state];
SDK `M/MTL4PipelineState.h:54–67`, `M/MTL4Compiler.h:195–216`.

Preparation has five explicit phases:

1. Verify the user's 834-file extraction manifest. That is **417 executable
   ARB programs** (208 vertex + 209 fragment) and **417 constant files**, not
   834 separate shaders. WS21's native extractor is the starting point.
2. Parse/validate every program and preserve the paired reflection. Translate
   locally, compile strict-math libraries, and record per-stage failure.
   Compile all entry points, including those absent from the first map.
3. Enumerate shader pairs, declarations and state permutations from the
   currently registered material techniques plus a locally collected observed
   state manifest. Observations seed the manifest; they cannot replace loading
   all material techniques or validation of legal permutations.
4. Create every PSO, sampler, depth state, fixed-function and copy/gamma pass
   needed by the next scene while showing preparation progress. Exercise
   cached pipeline creation and a small offscreen validation draw before
   declaring the scene ready. Shader/library/PSO creation APIs are preparation
   APIs only. [Compiler][compiler], [render encoder's ahead-of-time advice][render];
   SDK `M/MTL4Compiler.h:101–108,156–193`.
5. Publish an immutable ready map to the renderer. A missing draw key increments
   a diagnostic counter and stops that scene before it can be accepted as
   ready; development can return to a loading state and prepare it, shipping
   can restart the renderer as GL. Never compile synchronously on an active
   draw, substitute a visually different shader silently, or mix GL/Metal
   resources in a partially rendered frame.

Cache namespace: shader manifest hash, translator/ABI/key schema revision,
compiler settings, device identity/family, and OS build. Keep the cache under
the player's Application Support directory, stage writes under an exclusive
setup lock, validate checksums/record counts, and atomically replace. Treat an
OS/device/library change or corrupted archive as a cache miss during loading.
Translations, pipeline scripts, archives and observed state sets derived from
game content stay outside the repository under the player's Application Support
directory and must never enter source control or app resources.

### Metal 4 persistence, and what it does not guarantee

Attach an `MTL4PipelineDataSetSerializer` configured for descriptor and binary
capture to the preparation `MTL4CompilerDescriptor`. After creating prepared
PSOs, serialize its binary archive and optional pipeline script. The latter
can feed Apple's offline `metal-tt` tool when that tool is available, but a
runtime compiler remains the package-independent first-run path. Serializer
data are opaque and only associated compilers benefit from it. [Pipeline data
set serializer][dataset]; SDK `M/MTL4PipelineDataSetSerializer.h:22–83`,
`M/MTL4Compiler.h:45–51`.

On the next launch, open a compatible `MTL4Archive` and use archive creation
or `MTL4CompilerTaskOptions.lookupArchives` during preparation. Archives can
hold IR and/or GPU/system binaries, and lookup archives *can* accelerate a
compilation task. An archive file's existence does not prove a binary hit or
zero driver compilation. Metal 4's render descriptor does not have the
Metal 3 `binaryArchives` property. [Archive][archive], [compiler task
options][compiler-options], [render descriptor][pipeline]; SDK
`M/MTL4Archive.h:21–24,78–109`, `M/MTL4Compiler.h:63–75`,
`M/MTL4RenderPipeline.h:150–153`.

Prove the no-compile-during-play contract at the application boundary: wrap
all library/compiler/pipeline factory calls, record the phase, fail tests if
any run after `scene_ready`, and preload actual PSO objects. GPU capture and
OS tooling should additionally check for unexpected deferred work; this
design alone cannot promise undocumented driver behavior. Fully creating and
validating all ready objects before gameplay is the robust guarantee under
our control.

If a later Metal 3 contingency is implemented, its persistent cache uses
`MTLBinaryArchive` and its own descriptor path, rather than pretending it is
interchangeable with `MTL4Archive`. The legacy archive can add render pipeline
functions and serialize to a URL. [Binary archive][binary-archive]; SDK
`M/MTLBinaryArchive.h:85–91,121–128`. The planned macOS 13–25 fallback remains
GL, so no Metal 3 cache implementation is required for the first slice.

## Resources and binding

### Argument tables and argument buffers

For initial correctness, use Metal 4 argument tables with the existing slot
numbers: stream/constant buffers, 16 texture slots and 16 sampler slots. Tables
can bind GPU buffer addresses, texture resource IDs and sampler resource IDs;
assign tables to the appropriate render stages. Allocate their maximum bind
counts explicitly. [Argument table][arguments], [render encoder][render]; SDK
`M/MTL4ArgumentTable.h:22–35,61–118`,
`M/MTL4RenderCommandEncoder.h:620–622`.

Each draw owns an immutable state snapshot. Pool tables and constant slices by
submission generation, not a single table that the CPU mutates while earlier
GPU work uses it. Keep typed null textures with deterministic values rather
than dereferencing absent resources; select 2D/cube/3D binding types from IR
reflection. All resource IDs and pointed-to allocations remain alive and
resident until retirement. Establish the exact shader slot/struct ABI in the
shader workstream before integrating the draw wrapper.

A later optimization can place related resource references in an argument
buffer and bind that buffer through one table slot. Apple's argument buffers
group resources into one shader argument, and the encoder API exposes how
to populate them. It is an alternative binding representation, not a residency
or synchronization substitute. [Argument buffers][argument-buffers]; SDK
`M/MTLArgumentEncoder.h:31–118`. Adopt it only after measuring CPU binding cost;
the game's existing 16-slot model does not require global bindless textures.

### Residency and explicit hazards

Create long-lived device/map resource residency sets and a transient set for
streaming buffers, constants, tables' referenced allocations, copies and
offscreen targets. Add allocations, commit staged membership changes, and
attach sets to the queue or reapply per-command-buffer sets after each begin.
The queue supports 32 unique sets; two or three coarse sets are sufficient.
Residency determines GPU accessibility, not hazard ordering or object
ownership. [Residency sets][residency], [queue][queue], [command buffer][command];
SDK `M/MTLResidencySet.h:40–47,80–136`,
`M/MTL4CommandQueue.h:310–330`, `M/MTL4CommandBuffer.h:66–68,137–152`.

Metal 4 queue work ignores the resource's automatic hazard-tracking mode.
Track read/write use by texture subresource and buffer interval; initially
use conservative encoder/queue-stage barriers at upload→sample,
render→sample/copy, copy→render and query→CPU transitions. Narrow barriers
after correctness, not before. Same-queue commit order alone must not be
treated as completion of every resource write. [Resource synchronization][sync],
[Metal 4 barriers][barrier]; SDK `M/MTL4CommandEncoder.h:48–115`.

For unloading, detach a map set only after its last recorded submission
completes. Keep new/old map sets overlapping during transition if needed.
The same retirement rule covers in-flight texture replacement and buffer
renaming. Do not infer that unified memory makes concurrent CPU/GPU writes
safe.

### Locks, dynamic vertex/index data and retirement

Keep CPU-visible shared buffers for frequently written geometry and constants;
private buffers are an optional choice for static geometry after a staging
copy. Shared storage is accessible by CPU and GPU; private storage is GPU-only.
[Resource fundamentals][storage]; SDK `M/MTLResource.h:58–87`,
`M/MTLDevice.h:680–695`.

Use a bounded ring with two submission slots initially, adjustable after
latency/throughput measurement. Slot count is a policy, not a promise of a
particular frame rate. A slot contains its command allocator, vertex/index
ranges, constant ranges and pooled argument state. `DISCARD` starts a new
backing generation; `NOOVERWRITE` must not overlap an unretired interval.
Ordinary locks use staging or wait for the affected generation. `Unlock`
publishes dirty ranges; `Draw*UP` copies caller bytes before returning because
the engine may immediately reuse them. Maintain 16/32-bit index formats,
signed base-vertex handling, stream offsets and checked length arithmetic.

Signal a monotonic shared-event value after submitted GPU work. Check/notify
its completion before reusing resources, and apply bounded backpressure when
all slots are busy; never recycle memory merely because two CPU frames passed.
[Shared event][event], [queue signaling][queue]; SDK `M/MTLEvent.h:55–64`,
`M/MTL4CommandQueue.h:261–275`. `CFence` retirement should use the same
submission timeline instead of inventing a second fixed frame-delay heuristic.

Allocate one Metal 4 command allocator per concurrently encoding worker, with
separate allocators for in-flight slots. Begin, encode, end, then queue commit.
An allocator may service another command buffer after the previous encoding
ends, but `reset` of its heaps requires *all GPU work using that memory to
complete*. Keep these two events distinct. [Begin command buffer][begin],
[allocator reset][allocator-reset], [queue commit][queue]; SDK
`M/MTL4CommandBuffer.h:54–103`, `M/MTL4CommandAllocator.h:43–50`,
`M/MTL4CommandQueue.h:220–259`.

### Textures, formats and mip chains

Retain exact logical NPOT dimensions, every supplied mip and cube face,
3D slice pitch, and existing color conversion. Upload via shared staging into
the texture's intended storage. Metal 4's compute encoder exposes texture/
buffer copies and mip generation; use explicit barriers after copies. CPU
`replaceRegion` is suitable only for compatible CPU-accessible resources;
do not call it on private textures. [Texture][texture], [compute encoder][compute],
[storage][storage]; SDK `M/MTLTexture.h:508–523`,
`M/MTL4ComputeCommandEncoder.h:172–242,355–478,543`,
`M/MTLResource.h:58–87`.

Allocate the actual mip count (`mipmapLevelCount`) and constrain sampler LOD
to the levels the engine provides. Preserve stock mip data; generate mips only
when the original contract requests it, not automatically for one-level HUD
textures. WS15's one/partial/full mip-chain fixtures must be reproduced.
Metal exposes mip count and sampler LOD clamps. [Texture][texture], [sampler][sampler];
SDK `M/MTLTexture.h:170–174`, `M/MTLSampler.h:179–190`.

Proposed initial format mappings, subject to audit fixtures:

| D3D / decoded input | Proposed storage / policy | Evidence |
| --- | --- | --- |
| DXT1, DXT3, DXT5 | BC1 RGBA, BC2 RGBA, BC3 RGBA, with classic non-sRGB sampling. Use only after `supportsBCTextureCompression`; decode to BGRA if unsupported | [Pixel formats][formats], [BC support query][bc]; `M/MTLPixelFormat.h:219–224`, `M/MTLDevice.h:598–602` |
| DXT2 / DXT4, if used | Same BC2 / BC3 block family, retaining their premultiplied-alpha semantics in material/shader state; do not silently treat them as straight alpha | Proposed compatibility mapping; same format evidence as above; confirm actual use in audit |
| ARGB/BGRA/XRGB8 | BGRA8Unorm after current converter semantics; preserve opaque X alpha and the per-producer vertex-color distinction | [Pixel formats][formats]; `M/MTLPixelFormat.h:103–104`; `lp64_color_order.h`, `CColorConverter.c` |
| RGB8, A8/L8/A8L8 and packed 16-bit inputs | Match existing expansion/channel replication with original CPU conversion first, then consider narrower validated formats | [Pixel formats][formats]; `M/MTLPixelFormat.h`; exact conversion contract comes from `CColorConverter.c` / audit |
| P8 / palette inputs | CPU-expand with the existing palette conversion; retain palette change/dirty semantics. No assumed indexed-color sampling hardware | Proposed emulation; `CColorConverter.c`, palette methods in `CDirect3DDevice.c` |
| Color render targets | BGRA8Unorm for classic output; explicit sample/attachment usage. Additional float targets only where verified engine use needs them | [Pixel formats][formats], [pass][pass]; `M/MTLPixelFormat.h:103,176`, `M/MTL4RenderPass.h:32–39` |
| D24S8 / depth-stencil | Depth32Float_Stencil8 initially; preserve stencil byte semantics and test depth-equality / depth-bias differences. This increases precision rather than promising bit-identical 24-bit depth | [Depth/stencil format][depth-format]; `M/MTLPixelFormat.h:333–334` |

Apple silicon on macOS is a valid target for BC textures, but query actual
device support; SDK availability is not an affirmative runtime result. Do not
recompress BCn to ASTC as part of the faithful path. Test compressed block
alignment, tiny mips, transparent DXT1 pixels, DXT5 HUD alpha, cube order and
light-grid volumes against GL readback. The D24→D32 choice avoids depending
on the deprecated Depth24Unorm_Stencil8 format in this SDK, but precision/bias
parity needs evidence before acceptance. [Pixel formats][formats], [BC][bc];
SDK `M/MTLPixelFormat.h:219–224,333–334`, `M/MTLDevice.h:598–602`.

Cache samplers from wrap U/V/W, min/mag/mip filter, anisotropy and LOD clamps.
Honor per-slot LOD bias in the translated sampling path unless its device
sampler capability has been positively checked. Arbitrary D3D border colors
require emulation if observed: the SDK enumerates only transparent black,
opaque black and opaque white sampler border colors. These restrictions must
not become silent visual changes. [Sampler][sampler], [feature tables][features];
SDK `M/MTLSampler.h:64–90,114–216`.

## Depth, passes, copies and coordinates

### Depth/stencil and pass scheduling

Build separate depth/stencil objects from depth-enable/compare/write and
front/back stencil compare, fail/depth-fail/pass operations, read mask and
write mask. Stencil reference, winding/cull and depth bias remain encoder
state. Alpha test becomes a fragment-stage rejection using the original
compare function and normalized reference after computing the final shader
alpha. Do not replace it with alpha-to-coverage. Metal depth/stencil and
encoder controls are documented separately. [Depth/stencil][depth], [render][render];
SDK `M/MTLDepthStencil.h:36–65`,
`M/MTL4RenderCommandEncoder.h:122–214,628`.

Current GL stencil clears do not establish stencil draw semantics: the shim
ignores `STENCILENABLE`, stencil functions/operations/masks and two-sided
stencil state in `SetRenderState`, while the ARB draw path never applies them.
Validate Metal stencil behavior against explicit synthetic reference GL
fixtures and the engine's intended state decode/Windows reference, rather
than treating this incomplete production GL behavior as the parity target.

Delay render-encoder creation until the first draw or actual clear. Track a
pass signature: color/depth/stencil surfaces, mip/slice, dimensions, sample
count, pending clear, and last access. `BeginScene`/`EndScene` describe engine
sequencing, not a forced one-to-one Metal pass. A target or depth attachment
change, target sampling/copy, or pending readback closes the current pass.
Initially preserve every attachment with `store`; prove dead contents before
using `dontCare` or memoryless storage. Metal 4 pass descriptors use existing
attachment descriptors and load/store actions. [Pass][pass], [load][load],
[store][store]; SDK `M/MTL4RenderPass.h:19–39`,
`M/MTLRenderPass.h:32–45,70–101`.

Fold a full clear before any draw into the next pass's corresponding load
action. A later full clear ends/stores the pass and starts another clear/load
pass. Partial rectangle or masked stencil/color/depth clears need a small
scissored emulation draw preserving unaffected pixels and channels; a load
clear writes the whole attachment. Keep clear shaders and their state variants
prepared ahead of play. [Load action][load], [scissor setter][render]; SDK
`M/MTLRenderPass.h:32–36`, `M/MTL4RenderCommandEncoder.h:153–171`.

`StretchRect` uses a copy only for compatible equal-size, unfiltered formats;
scaling/filtering/conversion uses a prepared fullscreen shader. `UpdateTexture`
and dirty-volume copies preserve untouched mips/slices. Add explicit
render→sample, render→copy and copy→render barriers. Screenshot/readback waits
only at its requested synchronization point and reads the engine's pre-gamma
scene target. Avoid attaching a subresource and sampling it simultaneously;
use a distinct intermediate texture if an observed post pass needs feedback.
Copies and barriers have separate Metal 4 API contracts. [Compute][compute],
[barriers][barrier]; SDK `M/MTL4ComputeCommandEncoder.h:172–478`,
`M/MTL4CommandEncoder.h:48–115`.

Metal 4 color attachment mapping can route logical outputs to physical
attachments, but it does not remove attachment lifetime or load/store
requirements. Do not use it to collapse engine target changes until those
passes have independent parity tests. [Render pipeline][pipeline], [render][render];
SDK `M/MTL4RenderPipeline.h:25–38,219–223`,
`M/MTL4RenderPass.h:125–126`, `M/MTL4RenderCommandEncoder.h:75`.

### Coordinate contract

The translator's register-level body must reproduce the ARB program's raw
output. The renderer wrapper alone converts its resulting clip position to
Metal's raster contract. Preserve the same GL-corrected projection constants
initially, then apply `zMetal = (zARB + wARB) * 0.5` once. Metal NDC depth is
0–1 and window/2D texture coordinates have a top-left origin. [MSL
specification][msl], section 1.4, pages 23–24; SDK
`M/MTLRenderCommandEncoder.h:44–50`,
`M/MTL4RenderCommandEncoder.h:82–88`.

Do not run `MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL` twice. Its
current native path already inserts D3D9 pixel-center offsets and converts
depth (`MacOpenGLUtils.c:455–495`). The chosen wrapper preserves x/y output
and tests the vertical orientation using a labeled four-corner texture, both
windings, a one-pixel HUD quad and screen-target UVs. Copy/upload/screenshot
orientation must have a single documented conversion point. Avoid a blanket
`uv.y = 1 - uv.y`: stock textures and GL-rendered target textures can have
different reference conventions. Cube faces and 3D axes require their own
fixtures. This is a proposed adaptation to the verified GL path, not measured
Metal parity.

Programs that read `fragment.position` need the ARB window-coordinate
contract supplied explicitly by the fragment wrapper, including conversion
from Metal's top-left Y to the reference GL window Y when that binding is
used. The vertex depth conversion alone cannot validate fragment-position
inputs. Preserve their depth and reciprocal-W semantics separately from
2D texture orientation. [MSL specification][msl], section 1.4; SDK
`M/MTL4RenderCommandEncoder.h:82–88`. This is a renderer-wrapper requirement;
raw synthetic compute inputs bypass the raster coordinate conversion.

Alternative: send original D3D depth constants directly to Metal and remove
the GL projection conversion on that backend. That reduces work but changes
the shader's state bindings, handedness/pixel-center adjustments, and derived
post UV constants together. Defer it until the wrapper path proves parity.
Honor the D3D viewport's MinZ/MaxZ through Metal viewport values rather than
changing the depth transform again. [Viewport setter][render]; SDK
`M/MTLRenderCommandEncoder.h:48–50`,
`M/MTL4RenderCommandEncoder.h:82–88`.

### Classic color, sRGB and optional EDR

Classic mode reproduces current GL texture bytes and BGRA8Unorm intermediate
arithmetic. Keep the 256-entry RGB16 gamma LUT in the final present shader,
independent of scene screenshots; `r_ignoreHwGamma` selects identity. Do not
silently change all textures or attachments to sRGB: Metal's `_sRGB` formats
apply transfer conversion on reads/writes and treat alpha linearly. [Pixel
formats][formats]; SDK `M/MTLPixelFormat.h:103–104,219–224`.

Use a BGRA8Unorm layer for initial classic output and configure its colorspace
deliberately alongside the presentation owner. `CAMetalLayer` exposes a
colorspace and accepted drawable pixel formats. [Layer][layer], [layer pixel
format][layer-format]; SDK `Q/CAMetalLayer.h:77,118–129`. Add separate gray-ramp,
colored-ramp, blend and screenshot tests; visually inspecting one map cannot
distinguish duplicate gamma from shader-lighting errors.

EDR is an opt-in presentation mode after SDR parity. Query both potential
and current display headroom, use an accepted floating-point drawable format,
set the layer's extended-range flag/colorspace, and define explicit SDR UI
white/tone mapping. Apple documents separate potential/current EDR values;
RGBA16Float and `wantsExtendedDynamicRangeContent` are exposed in the layer
API. [EDR support][edr-support], [displaying HDR][edr], [layer format][layer-format];
SDK `A/NSScreen.h:84–97`, `Q/CAMetalLayer.h:77,118–138`,
`M/MTLPixelFormat.h:176`.

Original SDR textures/shader arithmetic do not become authored HDR simply by
selecting a float drawable. Preserve classic output as a reference and describe
any expanded highlights/tone mapping as an enhancement. Respect headroom
changes when brightness, display, or window placement changes. No physical
EDR display or output has been tested by this design work.

## Fixed-function and shader integration

The faithful programmable path uses original locally translated programs and
paired constants. Translation, library compilation and PSO linking are
different gates: successful arithmetic kernels do not prove linked vertex /
fragment interfaces, interpolation, derivatives, sampling or raster state.
The shader workstream should expose a stable register-level body plus both
headless validation and renderer wrappers. Reflection resolves `program.env`,
per-program `program.local`, state parameters, attributes, outputs and sampler
types; all dimensions/indices are validated before emission. Keep unused
aliases out of wrapper interface generation (WS15's sky alias failure).

WS24's current emitter in
[`src/Mac/Metal/arb_to_msl.cpp`](../../src/Mac/Metal/arb_to_msl.cpp) proposes
the concrete starting ABI below. These structures are local validation/
translation interfaces, not engine on-disk types:

```text
ARBInput:     attrib[16], texcoord[8], color[2], position, fogcoord — float4
ARBConstants: env[256], local[256], state[64] — float4
ARBOutput:    position, color[2], texcoord[8], fogcoord, depth — float4; killed — uint
```

`arb_evaluate` is the common register-level body. `ARB_SEMANTIC_TEST` selects
the `arb_test` compute wrapper with input buffer 0, constants buffer 1 and
output buffer 2; deterministic texture samples use level zero. Its raster
wrappers are `arb_vertex` (packed input buffer 0, constants buffer 1) and
`arb_fragment` (constants buffer 1 plus typed texture/sampler slots). The
vertex wrapper performs the GL→Metal clip-depth conversion above. Before
game integration, replace packed validation inputs with a declaration-aware
vertex fetch wrapper and implement any renderer-required state bindings;
the current cache state binding is `state.fog.color`. Test the actual raster
interfaces as well as the level-zero compute path, which does not establish
derivative/implicit-LOD parity. Compiler coverage/results are in the WS24
report and must not be inferred from this ABI description.

The native setup API,
[`MacMetal_PrepareShaderCache`](../../src/Mac/Metal/shader_setup.h), validates
source SHA-256/CTAB metadata, translates locally and compiles validation libraries;
its standalone driver is the current execution path. It is **not yet wired
into the game's first-run flow**. Integrate it after WS21 extraction/manifest
verification, before renderer readiness, and reuse its local output as input
to PSO preparation. Its libraries are released after validation; the backend
must separately retain functions/PSOs during loading and implement persistent
pipeline archives. Its cached MSL source is not a GPU binary archive.
Original cached programs with syntax the local GL compiler
rejects cannot receive a GL-based semantic proof merely by compiling their MSL
translation; inferred `_abs` handling and unsupported original opcodes need
explicit per-program limitations in the WS24 report.

For fixed-function draws, author an original MSL emulation set with:

- declaration/FVF input expansion, world/view/projection and texture matrices;
- diffuse/secondary color, texture stage CURRENT/DIFFUSE/TEXTURE/TFACTOR
  arguments, combine operations and modifiers observed by the audit;
- fog, alpha compare/discard, and the actual fixed-function lighting/material
  state required by active engine paths;
- dedicated stencil-clear, solid/colored/textured 2D, copy/scale and gamma-LUT
  variants using the same depth/blend/pass handling as programmable draws.

Do not equate the current approximate GL fallback with the whole fixed-function
contract. Build variants from observed and source-reachable states, keep a
bounded interpreter-like stage evaluation as a correctness option, and only
specialize during preparation. Unsupported combinations produce a visible
diagnostic before scene readiness. New emulation shaders are original code;
translated stock shaders and their test output remain local under PLAN rule 9.

In particular, `rb_state.c` currently has tentative, uninitialized
`texStageEnums` tables. Several reconstructed fixed-function stage updates
therefore use state 0 despite intended operations in `s_textureOpTable` and
`s_textureArgTable`; the populated coordinate states are 11 and 24. The
fixed-function workstream must verify/fix native mappings under focused guards
and validate the DX7 path separately. Observed approximate GL output is not
the semantic authority for those broken state updates.

Primitive mapping uses point, line, line strip, triangle and triangle strip
where contracts match; expand D3D triangle fans to a transient indexed triangle
list if reached. Metal's primitive enumeration contains no fan. Base vertex,
index length and type go to the indexed draw API only after shim validation.
[Primitive types][primitive], [Metal 4 render encoder][render]; SDK
`M/MTLRenderCommandEncoder.h:30–36`,
`M/MTL4RenderCommandEncoder.h:331–345,393–425`.

Occlusion queries, if the audit proves an active path, use a visibility result
buffer and the counting/boolean mode matching the requested query, returning
not-ready until the relevant submission completes. Allocate 8-byte-aligned
result offsets and preserve nonblocking polling. Event fences use the shared
submission value. Do not add speculative timestamp/disjoint query support
where the current engine only creates an event query/stub. [Render encoder][render],
[pass][pass]; SDK `M/MTL4RenderCommandEncoder.h:217–229`,
`M/MTL4RenderPass.h:96–100`.

## Presentation interface and latest-standards checklist

Keep renderer tests headless and offscreen. A future merged WS23 implementation
can supply a device/layer and a final-present operation, but the renderer must
not assume WS23 currently supplies Metal 4 submission, shader translation or
render passes. Define ownership explicitly when the two workstreams integrate:
one device, distinct renderer versus presenter command work, and a completed
offscreen color texture with dimensions/color contract and synchronization.

| Capability | Decision and prerequisite | Apple / SDK evidence |
| --- | --- | --- |
| Metal 4 queues and command allocators | Use explicit begin/end/queue commit. Pool per concurrent encoder and retirement slot; reset only after completion. Initial single renderer queue avoids unnecessary cross-queue synchronization | [Queue][queue], [allocator][allocator-reset], [begin][begin]; `M/MTL4CommandQueue.h:220–275`, `M/MTL4CommandBuffer.h:54–103`, `M/MTL4CommandAllocator.h:43–50` |
| Drawable handoff | For direct Metal 4 rendering to a drawable: `waitForDrawable` before commits targeting it, `signalDrawable` after them and before `present`. The wait schedules a GPU operation and returns immediately; it does not make `nextDrawable` nonblocking | [Wait][drawable-wait], [signal][drawable-signal], [nextDrawable][next-drawable]; `M/MTL4CommandQueue.h:277–308`, `Q/CAMetalLayer.h:102,147–153` |
| `CAMetalDisplayLink` | Available macOS 14+, gives display callbacks and a supplied drawable with target timestamps. Coordinate preferred frame-rate range/latency with the presentation owner; a 333 Hz simulation must not be tied to a 60/120 Hz callback | [Display link][display-link], [target timestamp][display-time]; `Q/CAMetalDisplayLink.h:19–47` |
| MetalFX spatial | Feasible opt-in after scene parity: resolved scene color at render scale → spatial scaler → native-size HUD → gamma/present. Query support, obey required texture usage, create scaler during preparation / resize and pass Metal 4 compiler for Metal 4 variant | [Spatial descriptor][spatial-descriptor], [Metal 4 spatial][spatial]; `F/MTLFXSpatialScaler.h:36–95,137–167`, `F/MTL4FXSpatialScaler.h:13–31` |
| MetalFX temporal | Deferred: requires reliable current→previous pixel motion, depth, jitter, reset/history management, including skinned models, viewmodel, particles and disocclusion. No motion texture/vector API was found by the shim/renderer search below. Camera-only reprojection is insufficient for moving geometry | [Temporal base][temporal-base], [Metal 4 temporal][temporal], [temporal descriptor][temporal-descriptor]; `F/MTLFXTemporalScaler.h:266–275,311–344,128–171`, `F/MTL4FXTemporalScaler.h:12–33` |
| Frame interpolation | Excluded in competitive play by Phase 2. Temporal upscaling does not require adopting interpolation. Do not make it a hidden default | Project policy; no interpolation API needed |
| EDR | Optional after SDR parity; display headroom and color/tone mapping remain separate from renderer correctness. Do not claim HDR content reconstruction from original SDR assets | [EDR][edr], [headroom][edr-support]; `A/NSScreen.h:84–97`, `Q/CAMetalLayer.h:118–138` |
| GPU capture | Developer-only one-frame `MTLCaptureManager` request; test destination support and capture the `MTLDevice` to cover Metal 4 queue work. Header names `MTLCommandQueue` but does not explicitly document `MTL4CommandQueue` as a capture object; do not assume queue capture interchangeability | [Capture manager][capture]; `M/MTLCaptureManager.h:42–62,87–95` |
| Debug labels / validation | Label submissions/resources/PSOs by local stable IDs; use command-buffer debug groups and preparation-time pipeline shader-validation options. Capture files can contain licensed resources and remain outside the repository in Application Support | [Command buffer][command], [pipeline options][pipeline-options]; `M/MTL4CommandBuffer.h:51–52,154–160`, `M/MTL4PipelineState.h:84–90` |

Motion-vector evidence command (no matches, exit 1):

```sh
rg -ni 'motion(texture|vector|_texture|_vector)|velocity(texture|vector|_texture|_vector)' \
  src/PC/gfx_d3d src/Mac/DirectX_9
```

This is a bounded source search, not proof that the engine has no previous
transform data. A later temporal workstream must audit previous camera/entity/
bone transforms, add a velocity attachment and settle jittered/nonjittered
motion conventions before claiming feasibility. MetalFX's SDK contains
macOS 27-only motion options as well as the macOS 26 base path; avoid using
newer options unguarded in a 26+ implementation (`MTLFXTemporalScaler.h:98–113`).

GPU capture and frame-rate measurements are different activities. Any timed
performance measurement takes the PLAN benchmark lock and records screen
lock state and concurrent processes. This design's SDK reads and source
searches are not benchmark results; no performance target is claimed.

## Verification strategy

First prove original synthetic fixtures independent of licensed assets:
parser/IR errors, instruction operands, swizzles/masks/saturate, address
register behavior, state binding, default attributes, packed color formats,
index bounds/base vertex, lock/discard/no-overwrite, partial clear, separate
blend/stencil, render-target copies, mip completeness and RGB16 gamma ramps.
Run the same register/raster tests against GL and Metal. Keep translated game
programs, extracted payloads, traces, compiled derivatives and game captures
outside the repository in Application Support, including temporary outputs.
Tests skip with a clear message when the licensed cache is absent; synthetic
tests still run.

Cache coverage must name counts correctly: **834 verified payloads, 417
translated/compiled program entry points**, plus the number of linked shader
pairs and PSO state combinations. Report every filename/error; never count
metadata files as Metal compiler successes. Cover at least one executable
program per distinct instruction set in the real GL-versus-Metal headless
semantic harness. Include adversarial masks, signed values, texture/cube/3D
inputs, near-zero denominators and indirect parameter reads where reached.
Shader arithmetic results and full raster output results are separate tables.
Select numerical tolerance before execution; report absolute/relative error
and failures rather than smoothing them into a single mean.

For first-frame visual acceptance, create new lossless GL and Metal captures
on this same branch using WS15's final matched cameras and settings:

| View | `setviewpos` | Eye / pitch |
| --- | --- | --- |
| Courtyard | `2569 2274 181 180` | `2569 2274 182`, pitch 0 |
| Rooftop | `3051 2178 220 180` | `3051 2178 221`, pitch 0 |
| Sky | `3051 2178 350 0`, then clamped lookup | `3051 2178 351`, pitch 85 |

Use 1280×720, Toujane DM, FOV 80, gamma 1, hardware-gamma handling enabled,
fog on, AA one sample, manual picmip with all three values 0, hidden HUD/gun,
fixed noclip pose and matched scene time. Save camera/matrix/state hashes with
each image; reject a mismatched pose or frame before comparing it. Preserve
all-pixel RGB comparison without alignment, crop, exclusion masks or color
adjustment, following WS15. Pair geometry/materials, not draw indices, because
the Windows reference has an extra depth prepass.

Proposed GL→Metal classic gates: RGB MAE ≤1/255, RMSE ≤2/255, 99th-percentile
channel error ≤4/255, and fewer than 0.1% of pixels with any channel error
>16/255. These are acceptance targets, not measured results. If even repeated
GL captures fail these repeatability gates, fix nondeterminism first and
report it; do not silently relax the threshold. Add depth/stencil checks and
targeted ROIs for thin sky/HUD/alpha features to prevent a small missing
feature disappearing in a whole-image mean.

For continuity with WS15's JPEG Windows comparison, preserve its existing
reference images and target no regression above its merged results plus a
declared 0.25/255 MAE allowance: courtyard 2.0050, roof 1.8726, sky 0.5856
(baseline values from WS15, not remeasured here). Fresh lossless GL→Metal
comparison is the primary renderer acceptance gate. Gamma presentation has
separate ramp/readback tests because scene screenshots are pre-gamma.

Then broaden to menus/HUD, grounded firing, smoke/frag FX, viewmodels,
light-grid static/skinned models, stencil shadows, bloom/channel mixing,
different target sizes, resize/renderer restart and cache invalidation.
MSAA, render scale, sRGB enhancement, MetalFX and EDR each receive separate
goldens; none replaces faithful one-sample SDR acceptance. Measure frame times
only after parity with the benchmark lock, unlocked screen, known processes
and separate cold/warm setup durations. A completed optional single-batch
Toujane slice is evidence for that batch, not a renderer/gameplay result.

## Staged implementation and parallel scopes

One integration owner defines the C backend dispatch, object lifetime,
submission timeline and shader binding ABI first. Freeze these contracts in
small native headers, then split work into new files without changing engine
call sites. All scopes retain GL and have synthetic offscreen tests.

| Stage / workstream | Owns | Inputs / acceptance before merge |
| --- | --- | --- |
| A: backend skeleton and ABI | Native selection, C dispatch, narrowly guarded engine GL-escape hooks, device creation, sidecar ownership, startup availability/failure path, frame-slot completion | Existing shim audit. Compiles at deployment target 13 with new APIs guarded; empty offscreen clear/readback, refcount fixtures, OFF/source guards and ABI zero mismatches |
| B: resources | Buffers, locks/ring retirement, 2D/cube/volume textures, palette conversion, mip uploads, typed nulls, residency membership | A ownership/timeline. Synthetic readbacks match GL; compressed/mip/3D/color fixtures; no CPU/GPU overlap |
| C: states and PSOs | State shadow normalization, key/equality, sampler/depth caches, local datasets/archives, material/trace permutation preparation | A ABI; shader entry interfaces from E. Cache invalidation/collision tests; no factory calls after scene ready; partial state-update fixtures |
| D: draw path | Declarations/streams, packed attributes, topology/index handling, argument tables, constant snapshots, pass hazard record | A, B, C and E interfaces. Original synthetic geometry matches GL before licensed batch replay; checked index/baseVertex cases |
| E: shaders | WS24 translator hardening, raster wrappers, interpolation/register bindings, strict math, fixed-function emulation, sampling derivatives | Licensed cache local only. 417 compile results plus linked-pair/PSO counts; instruction-set semantic results; honest skip when cache missing |
| F: render targets and post | Clear scheduling, depth/stencil attachments, copies/stretch/readback, gamma, shadow/bloom/channel pass parity | A timeline, B textures, C states, D draws. Partial clear/store/load fixtures; WS15 cameras meet GL/Metal thresholds |
| G: presentation integration | Integrate merged WS23 interface, drawable synchronization, display-link pacing, resize/fullscreen handling | Completed offscreen renderer and reviewed WS23 design. No duplicated device/queue ownership; classic screenshot/gamma parity and bounded queue depth |
| H: optional modern output | Spatial MetalFX/render scale then MSAA/EDR; temporal velocity research separately | Classic acceptance first, support guards and independent goldens. No frame interpolation in competitive play |

B, C and E can progress concurrently after A's interfaces. D joins their
validated resources/states/shaders; F can author pass fixtures in parallel
then joins D. G integrates the presentation work only after that branch is
merged and reviewed. Each branch owns new files; a single owner makes minimal
shared shim/CMake/header edits so sibling workstreams need not race them.

No renderer enabling change is complete without the existing client stock and
CoD2x builds, renderer/game/script/VM/cod2x/perf/online suites, ABI checker and
OFF source guard gate, plus new Metal tests. The orchestrator runs the exact
merge gate recorded in the WS24 report. Do not mistake a doc-only SDK audit
for those runtime checks.

## Documentation and SDK evidence

Read the whole `PLAN.md`, WS15's final matched-view/follow-up sections, WS18's
shader setup/firing result and WS21's native extraction/packaging result.
Apple references below were retrieved on 2026-10-04. Dynamic Apple pages offer
Markdown at the same URL with `.md`; when the web reader could not fetch that
content type, the official Markdown was read directly with Python `urlopen`.
The MSL PDF was read in memory with the already installed `pdftotext`; no
package was installed and no SDK/documentation file was added to the repo.

`xcrun --show-sdk-version` returned `27.0`; `xcodebuild -version` returned
`Xcode 27.0`, build `27A266a`. SDK headers were inspected with line numbers from:

```text
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/
```

Aliases in this document: `M/` = `Metal.framework/Headers/`, `F/` =
`MetalFX.framework/Headers/`, `Q/` = `QuartzCore.framework/Headers/`,
`A/` = `AppKit.framework/Headers/`. Header line references refer to the SDK
installed during WS24; verify symbol names/availability again when the SDK
changes. SDK reads establish declared capability, not runtime support or
measured performance. No game launch or benchmark was performed by the
documentation unit; runtime shader results belong to the WS24 report.

Reproducible read-only checks:

```sh
xcrun --show-sdk-path
rg --files "$(xcrun --show-sdk-path)/System/Library/Frameworks/Metal.framework/Headers" \
  | rg '/MTL4.*\.h$'
rg -n 'API_AVAILABLE|reset|beginCommandBuffer|endCommandBuffer|commit|signalEvent|waitForDrawable|signalDrawable' \
  "$(xcrun --show-sdk-path)/System/Library/Frameworks/Metal.framework/Headers/MTL4CommandAllocator.h" \
  "$(xcrun --show-sdk-path)/System/Library/Frameworks/Metal.framework/Headers/MTL4CommandBuffer.h" \
  "$(xcrun --show-sdk-path)/System/Library/Frameworks/Metal.framework/Headers/MTL4CommandQueue.h"
rg -n 'serialize|Capture|pipelineDataSetSerializer|lookupArchives' \
  "$(xcrun --show-sdk-path)/System/Library/Frameworks/Metal.framework/Headers/MTL4PipelineDataSetSerializer.h" \
  "$(xcrun --show-sdk-path)/System/Library/Frameworks/Metal.framework/Headers/MTL4Compiler.h"
rg -n 'API_AVAILABLE|motionTexture|depthTexture|jitter|reset|supportsMetal4FX' \
  "$(xcrun --show-sdk-path)/System/Library/Frameworks/MetalFX.framework/Headers/MTLFXTemporalScaler.h"
```

[device]: https://developer.apple.com/documentation/metal/mtldevice
[queue]: https://developer.apple.com/documentation/metal/mtl4commandqueue
[command]: https://developer.apple.com/documentation/metal/mtl4commandbuffer
[begin]: https://developer.apple.com/documentation/metal/mtl4commandbuffer/begincommandbuffer(allocator:)
[allocator-reset]: https://developer.apple.com/documentation/metal/mtl4commandallocator/reset()
[compiler]: https://developer.apple.com/documentation/metal/mtl4compiler
[compiler-options]: https://developer.apple.com/documentation/metal/mtl4compilertaskoptions
[dataset]: https://developer.apple.com/documentation/metal/mtl4pipelinedatasetserializer
[archive]: https://developer.apple.com/documentation/metal/mtl4archive
[binary-archive]: https://developer.apple.com/documentation/metal/mtlbinaryarchive
[pipeline]: https://developer.apple.com/documentation/metal/mtl4renderpipelinedescriptor
[pipeline-options]: https://developer.apple.com/documentation/metal/mtl4pipelineoptions
[blend]: https://developer.apple.com/documentation/metal/mtl4renderpipelinecolorattachmentdescriptor
[blend-state]: https://developer.apple.com/documentation/metal/mtl4blendstate
[arguments]: https://developer.apple.com/documentation/metal/mtl4argumenttable
[argument-buffers]: https://developer.apple.com/documentation/metal/managing-groups-of-resources-with-argument-buffers
[residency]: https://developer.apple.com/documentation/metal/mtlresidencyset
[sync]: https://developer.apple.com/documentation/metal/resource-synchronization
[barrier]: https://developer.apple.com/documentation/metal/mtl4commandencoder
[render]: https://developer.apple.com/documentation/metal/mtl4rendercommandencoder
[compute]: https://developer.apple.com/documentation/metal/mtl4computecommandencoder
[pass]: https://developer.apple.com/documentation/metal/mtl4renderpassdescriptor
[load]: https://developer.apple.com/documentation/metal/mtlloadaction
[store]: https://developer.apple.com/documentation/metal/mtlstoreaction
[depth]: https://developer.apple.com/documentation/metal/mtldepthstencildescriptor
[depth-format]: https://developer.apple.com/documentation/metal/mtlpixelformat/depth32float_stencil8
[vertex]: https://developer.apple.com/documentation/metal/mtlvertexdescriptor
[step]: https://developer.apple.com/documentation/metal/mtlvertexstepfunction
[primitive]: https://developer.apple.com/documentation/metal/mtlprimitivetype
[buffer]: https://developer.apple.com/documentation/metal/mtlbuffer
[texture]: https://developer.apple.com/documentation/metal/mtltexture
[sampler]: https://developer.apple.com/documentation/metal/mtlsamplerdescriptor
[storage]: https://developer.apple.com/documentation/metal/resource-fundamentals
[formats]: https://developer.apple.com/documentation/metal/mtlpixelformat
[bc]: https://developer.apple.com/documentation/metal/mtldevice/supportsbctexturecompression
[features]: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
[event]: https://developer.apple.com/documentation/metal/mtlsharedevent
[msl]: https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
[layer]: https://developer.apple.com/documentation/quartzcore/cametallayer
[layer-format]: https://developer.apple.com/documentation/quartzcore/cametallayer/pixelformat
[next-drawable]: https://developer.apple.com/documentation/quartzcore/cametallayer/nextdrawable()
[drawable-wait]: https://developer.apple.com/documentation/metal/mtl4commandqueue/waitfordrawable(_:)
[drawable-signal]: https://developer.apple.com/documentation/metal/mtl4commandqueue/signaldrawable(_:)
[display-link]: https://developer.apple.com/documentation/quartzcore/cametaldisplaylink
[display-time]: https://developer.apple.com/documentation/quartzcore/cametaldisplaylink/update/targetpresentationtimestamp
[edr]: https://developer.apple.com/documentation/metal/displaying-hdr-content-in-a-metal-layer
[edr-support]: https://developer.apple.com/documentation/metal/determining-support-for-edr-values
[spatial-descriptor]: https://developer.apple.com/documentation/metalfx/mtlfxspatialscalerdescriptor
[spatial]: https://developer.apple.com/documentation/metalfx/mtl4fxspatialscaler
[temporal-descriptor]: https://developer.apple.com/documentation/metalfx/mtlfxtemporalscalerdescriptor
[temporal-base]: https://developer.apple.com/documentation/metalfx/mtlfxtemporalscalerbase
[temporal]: https://developer.apple.com/documentation/metalfx/mtl4fxtemporalscaler
[capture]: https://developer.apple.com/documentation/metal/mtlcapturemanager

## WS29 implemented core and interface freeze (ABI 1)

The integration authority is now [metal-interfaces.md](metal-interfaces.md),
with C headers in `src/Mac/Metal/`. The implementation shares WS23's actual
native presenter device and **one Metal 4 queue/event** across rendering and
presentation. GL interop presentation retains its original transport. macOS 26
plus Metal4 family/creation checks gate the native backend; unsupported devices
fall back to GL before device/scene object creation. Resize retains the exact
transport and monotonic timeline while rebuilding layers/targets; full shutdown
ends the epoch. Metal command feedback retires allocator/resource slots even on
error, and errors quarantine the transport. An event alone cannot retire failed
work reliably, so feedback is the reuse authority and the event is the fence/
producer retirement value authority.

The translator's current register structs remain the contract: ARBInput is
448 bytes, ARBConstants is 9216 bytes (env/local/state offsets 0/4096/8192),
and ARBOutput has **224-byte stride**, including the implicit MSL float4 tail
alignment after `uint killed` at 208. The C ABI spells that padding explicitly.
Raster argument tables reserve buffers 0 input, 1 constants, 2..17 streams,
16 textures and 16 samplers; compute semantic tests use output buffer 2. WS24's
translated cache contains MSL sources rather than prebuilt metallibs, so the
shader provider must prepare/validate compiled libraries during loading.

The core is clear-only: CPU resource shells/staging, raw state shadows and
shader source retention let the game run while provider callbacks are absent.
Native game resource uploads, game PSOs/shaders/draws and depth/target/post
semantics remain the follow-up slices. The freeze documents their exact
starting APIs and responsibilities. GL remains the default.
