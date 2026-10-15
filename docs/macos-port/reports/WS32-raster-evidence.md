# WS32 raster translation evidence and WS33 handoff

This is the raster subtask of WS32 in the `metal-shaders` worktree. It changes
only `arb_to_msl.{h,cpp}` / `arb_raster.h`, `tests/metal/parser.cpp`, new `tests/metal/raster.*`
fixtures and this report. No frozen ABI header, engine header, CMake seam,
remotes, sibling worktree, packages or game files were changed. Root WS32 owns
the complete provider/cache integration, full merge gates and commits.

## Implemented against ABI 1

`arb_vertex` now fetches declaration attributes from the immutable descriptor
in buffer 0 and stream allocation starts in buffers 2..17. Buffer 1 is the
stage's 9,216-byte register snapshot. Fetch supports D3DDECLTYPE 0..16:
float1..4, D3DCOLOR, UBYTE4, SHORT2/4, UBYTE4N, SHORT2N/4N, USHORT2N/4N,
UDEC3, DEC3N and FLOAT16_2/4. It reads unaligned little-endian bytes, preserves
the implicit `(0,0,0,1)` components, clamps normalized signed minima to -1,
and applies explicit producer RGBA/BGRA/ARGB order only to D3DCOLOR. A
disabled attribute, null stream or bounded-range failure returns the default.
WS33 must reject invalid CPU draw/declaration/index ranges before encoding;
the shader's defensive bounds handling is not HRESULT validation.

The semantic compute ABI is unchanged: packed input 448 bytes, constants
9,216 bytes, output stride 224 bytes, buffer bindings 0/1/2. Existing arithmetic
opcode implementations and semantic TEX/TXP/TXB sampling remain unchanged.
The raster evaluator takes the private descriptor in addition; the semantic
`arb_evaluate` signature retains its original arguments under
`ARB_SEMANTIC_TEST`.

The shared raster interface explicitly uses center perspective interpolation
for primary/secondary colors, fog and all eight float4 texcoords. The vertex
wrapper performs clip-depth conversion `(z+w)*0.5` once. The fragment wrapper
provides GL lower-left window Y as `framebufferHeight - position.y`, preserves
window Z and reciprocal W, and reconstructs fog input `(f,0,0,1)`. An actual
instruction writing `result.depth` activates a `[[depth(any)]]` output with
clamped **Z** component. An unused depth declaration leaves raster depth intact.
The ARB specification defines the depth component as Z, not X.

DDX/DDY are fragment-only. DDY negates Metal's Y derivative to preserve GL's
window-axis direction. Isolated semantic compute rejects these instructions
at compile time because it has no raster neighbors. TEX/TXP use implicit
derivatives; TXB adds instruction and sampler bias. TXL supports explicit LOD;
TXD supports explicit 2D/3D/cube gradient spellings. Sampler bias is added to
TXL's explicit level and scales TXD's gradients by `exp2(bias)`. That addition
was required by the independent GL shader texture LOD reference on this Mac.
Only 2D TXL/TXD GPU parity is established by this fixture; no general NV program
extension/control-flow support is claimed.

The fragment wrapper applies all eight D3DCMPFUNC alpha comparisons to the
final shader alpha, alongside KIL, with ALPHAREF clamped/normalized from uint
0..255. All runtime state is descriptor data, so a draw needs no shader variant
compilation. Native `MTLSamplerDescriptor.lodBias` must remain zero: MSL applies
the full float slot bias. The installed SDK exposes native sampler lodBias on
macOS 26, but its S4.6 precision/range would additionally quantize that value.

## Exact descriptor WS33 can consume

Use `cod2::arb::RasterDescriptor` in `arb_raster.h` (included by `arb_to_msl.h`), **736 bytes**, and bind the
same immutable packet in buffer 0 for both stages. This is a provider-local
payload within the frozen ABI-1 buffer slot, not a frozen-header change.

| Member | Offset | Layout / meaning |
| --- | ---: | --- |
| attributes[16] | 0 | 16 bytes each: uint32 stream, offset, D3DDECLTYPE, ColorOrder |
| streams[16] | 256 | 24 bytes each: uint64 byte offset, allocation byte length; uint32 stride, reserved |
| framebufferHeight | 640 | float full attachment height, used for window Y |
| reserved[3] | 644 | uint32 zeros |
| samplerLODBias[16] | 656 | float bias indexed by original sampler number |
| alphaEnabled | 720 | uint32 Boolean |
| alphaFunction | 724 | uint32 D3DCMPFUNC 1..8 |
| alphaReference | 728 | uint32 ALPHAREF, clamped to 255 |
| reservedAlpha | 732 | uint32 zero |

`ColorOrder` is RGBA=0, BGRA=1, ARGB=2. Disabled attributes use stream>=16 or
type>=17; the C++ defaults do this. Usage-to-attribute normalization remains
WS33's job; texcoord N maps to attribute 8+N, position to 0, normal to 2,
primary/secondary colors to 3/4 and fog to 5. Each stream pointer binds its
allocation start and the descriptor adds the declared stream byte offset and
attribute offset exactly once, plus Metal-provided `vertex_id * stride`.
Lengths are allocation lengths, not remaining lengths. Do not add the base
vertex to both the bound stream pointer and the Metal draw. Keep descriptor,
register blocks and all referenced stream allocations immutable/resident until
the ABI-1 submission retires. Separate vertex and fragment register banks are
required; locals belong to the bound shader, and ordered `stateBindings` supply
the stage's state[0..63].

## Verification

The test-first parser run initially aborted on the descriptor expectation;
subsequent additions independently failed on unsupported DDX, absent sampler
bias and absent post-shader alpha comparison before production edits. The
original parser rejection/address/mask/state tests remain and pass ASan/UBSan.

```sh
taskpolicy -b nice -n 19 clang++ -std=c++17 -Wall -Wextra -Werror \
  -ffp-contract=off -fsanitize=address,undefined -Isrc/Mac/Metal \
  tests/metal/parser.cpp src/Mac/Metal/arb_to_msl.cpp \
  -o output/ws32-raster/parser-test
output/ws32-raster/parser-test
sh tests/metal/raster.sh
```

The raster command builds with ASan/UBSan, compiles libraries using
`MTLDevice newLibraryWithSource`, links 60 render PSOs during fixture setup,
executes offscreen 32x24 float color/depth draws, and compares every pixel
against legacy GL. Six extra runtime compilations verify TXD's 2D/3D/cube
spellings in both raster and semantic libraries; a deliberate DDY semantic
compilation must reject with the raster-neighbor diagnostic. It requires no licensed cache and skips outside macOS or
without clang++. Outputs stay under the private `output/ws32-raster` path.
No offline `xcrun metal` command or game launch is used.

The **60 cases / 46,080 pixels** cover:

- window position, reciprocal W, perspective UV, all three producer color
  orders, fog components and missing attributes;
- replacement depth, unused depth declaration, clamped depth;
- TEX/TXB/TXP mip selection, DDX/DDY, TXL/TXD, and all five texture instructions
  with additional per-slot sampler bias;
- all 17 declaration types, truncated range and uint64 stream-offset overflow;
- all eight alpha functions with varying final alpha and exact equality,
  clamped ALPHAREF, separate VS/PS environment/local banks and local/fog state.

All cases pass. Maximum absolute color error is **5.96046e-7**, maximum depth
error **4.17233e-7**. Declaration conversions, all mip-selection cases and
local/fog-state outputs have **zero** color difference. DDX/DDY agree with
independently authored GLSL derivatives; TXL/TXD agree with
GL_ARB_shader_texture_lod references. Type decoding is checked against GL
provided with independently expanded float attributes, not against the shim's
own decoder. Uniform colored mip levels make the selected mip observable.
These errors are below every proposed WS24 pixel threshold, with zero pixels
over 16/255; no alignment, crop, pixel mask or color adjustment is used.
The GL readback rows alone are reversed to the documented Metal top-left
readback order.

The GL driver reports software fallback when it reaches a depth-declaration
fixture; the GL calls complete and their full pixel/depth outputs are still
compared. This is correctness evidence, not GL performance evidence. Screen
lock state was not measured and concurrent autonomous builds were present.
No performance measurement or benchmark lock was used.

Primary references checked during implementation:
[ARB fragment program](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_fragment_program.txt)
defines window/reciprocal-W, fog and depth semantics;
[ARB color buffer float](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_color_buffer_float.txt)
permits unclamped GL alpha reference on a float target, so the fixture clamps
the D3D reference before passing it to GL;
[ARB shader texture LOD](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_shader_texture_lod.txt)
defines the independent explicit-LOD/gradient oracle.

## Limits and merge notes

No ABI-1 header change is proposed. The root provider must invalidate translated
source/library/archive caches through its translator revision when adopting
this wrapper revision. WS33 can use `arb_vertex` / `arb_fragment` directly with
the descriptor above and the unchanged register/texture/sampler slots.
The standalone fixture uses ordinary Metal render commands as an independent
shader test; it does not implement a second game backend. Metal4 provider
link/archive evidence, licensed 417-program translation/runtime-load counts,
semantic 400/417 acceptance, fixed-function fixtures, full gates and smoke
results are reported by root WS32. This subtask does not establish all material
pairs, all raster states, arbitrary interpolation boundary equality, 3D/cube
explicit-gradient pixel parity or complete game frame parity.
