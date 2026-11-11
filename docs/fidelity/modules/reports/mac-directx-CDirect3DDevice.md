# Mac/DirectX 9/CDirect3DDevice reconciliation

Updated 2026-11-05.

121 assigned entries audited: 80 equal, 13 fixed, 8 intentional, 20 unchecked. The tiny
success/no-output methods were read from both ordinary text and __textcoal_nt. Allocation
wrappers were compared for constructor arguments, returned pointers, ignored inputs and native
storage. Device capability fields at 0x1328a match the original.

GetSwapChain 0x2ed968 now returns/AddRefs the primary swapchain. Original quirks are preserved:
GetNumberOfSwapChains 0x2ed98e returns zero and ShowCursor 0x2ed956 returns true. Mac
ValidateDevice/GetVertexDeclaration/CreateQuery/ CreateOffscreenPlainSurface do not write
outputs. Native retains their existing one-pass result, current declaration, initialized query
object and surface provider, because removing defined native outputs requires an established
caller storage contract. Named-method and explicit slot 70/88/118 scans did not establish all
native consumer initialization. The explicit slot 36 offscreen diagnostic is
GFX_REAL_D3D9-only. These four methods are recorded as defined-storage adaptations, not
equality. The two surface allocation loops at 0x13dd0/0x13efa now terminate after 32 doublings,
including unsigned wrap, as Mac does.

SetAlphaFuncAndRef 0x13a4a now performs cached reference/function updates and calls GL when
either changes. GetRenderState 0x13b84 copies state 206's fallthrough to CULLMODE, while
retaining existing defined native extended/default outputs. The existing depth fixture's
stencil getter expectations stay unchanged. The fixture only adds definitions for three
original cache flags now written by Init; no new test or new assertion was added.

SetViewport 0x13978 compares a statically checked 24-byte cache snapshot built by typed reads
and flips Y against render-target height. GetViewport 0x2ed9f6 uses typed output fields.
Neither assumes the imported LP64 D3DVIEWPORT9 has 24 bytes; float cache comparisons/copies
preserve bit patterns. Vertex shader/declaration changes at 0x1406a/0x14024 now invalidate the
original caches only on change; native draw retains its existing direct D3D-element format.
GetTexture 0x2edc20 respects the fixed-function bound and AddRefs actual native COM texture
objects. Constant mirrors, raw native projection storage and frame-trace instrumentation are
documented adaptations of the current providers.

CreatePixelShader 0x1b3f6 now calls the ARB provider and returns failure for a GL program
upload error. Material_LoadPassShader discards its header without releasing its shader field on
failure. Native therefore keeps ownership until success, releases failed objects and clears
output, an explicit lifetime adaptation of the original publishing/leaking failure path. The
related header-attributed identity 0x1b0da was inspected: Mac splits Name::Code and selects
type 0 ATI, type 1 NV register combiners or type 2 ARB, then checks
GL_PROGRAM_ERROR_POSITION_ARB. The selected generic native Apple policy uses ARB and its shader
compiler/cache emits bare !!ARB text. Native accepts that concrete format plus Name::Code,
passes pointer-backed strings synchronously, and rejects unsupported vendor types rather than
constructing a success stub. The ARB provider's widened struct is 40 bytes with the selected
Apple SDK (ULONG is 32-bit), or 48 bytes without it; the retained 0x80 allocation covers its
vtable/refcount/mask, two string pointers and GLuint. It retains neither caller string
argument. Provider code itself was not changed here.

The other header-attributed identities 0x1a7ba/0x1ab62 are the two real device constructors,
not retired code. They set context/adapter/type, initialize original state/vectors/matrix and
declaration caches, run Reset and take the first reference. Selected native creation bypasses
them via static storage and Init. Init now reads GL capability limits, sets original blend and
first-eight-stage sampler/combiner defaults, and marks caches dirty; complete constructor/Reset
equivalence remains unchecked. These three extra identities are reported to the parent and are
not added to this unit's 152-entry ledger.

The three destructor identities 0x19b92 (D0), 0x1af0a (D2) and 0x1aff2 (D1) are also reported
separately and remain unchecked. Mac releases the swapchain, backbuffer, current depth-stencil
and two saved/default resources, shuts down COpenGL, releases context, then frees its stage
vector. Native now releases its owned swapchain and backbuffer before the context, matching
their actual constructor references. Native also releases the real COM textures retained in its
global cache before SDL destroys the context; that cache outlives an individual native device
and is an explicit storage/lifetime adaptation. Mac's stage-vector destructor advances through
elements without Release calls. Native current-depth ownership, default resources and full
COpenGL shutdown remain unresolved. Native D0 retains static device storage rather than
deleting it.

The 20 unchecked assigned entries cover CTexStage Init/GetUsesTexture (the imported native type
is still a 4-byte placeholder), full indexed drawing, pixel one-to-one state, material/light
updates, FVF/declaration switching, sampler/stage semantics, stream-source dirty/stride
caching, complete render-state/copy/target behavior, Present's frame/fence/debug
responsibilities and full Reset. COpenGLNative has no equivalent persistent
vertex-program-enabled field or accessor, and COpenGL_SetLight is empty. Per-draw shader
enable/disable does not establish that missing contract. Raw original +0x800 offsets were not
copied into widened native storage. The ledger gives the exact remaining gap per function.

Actual changed client objects compiled; the arm64 Release dedicated target linked. Existing
18-case depth-surface, shader-raster and saved-screen fixtures passed with ASan/UBSan. These
exercise the retained depth/raster/saved-screen paths and the retained native stencil getter
outputs, not every repaired device API. Other changes are compiled and reviewed only. An
artificial non-SDK source probe on this Mac fails on existing Darwin typedef conflicts and
missing GL constants elsewhere; it is not a verified non-SDK build. The explicit
viewport-layout assertions did not fail in that probe. No full CI, replay, new tests or game
run was performed. All changed COD2_X64=OFF C bodies remain unchanged in five preprocess
configurations; no changed production file uses __LINE__. Private evidence stays outside Git in
~/Library/Application Support/CoD2-native-decomp/recon-mac-device.noindex/.

## Integrated batch 15 verification

Engine bcafce5 and fixture-only follow-ups ae7b674, 5642801 and 4e73a89 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
