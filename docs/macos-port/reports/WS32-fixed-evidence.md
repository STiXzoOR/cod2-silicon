# WS32 — original fixed-function shader set

This is the fixed-function unit of WS32 in `port/metal-shaders`, implemented only
in new files in the assigned worktree. The standalone focused gate passes:
**666 GL/Metal offscreen comparisons, every compared RGBA byte identical**.
Three MSL libraries compile using `MTLDevice newLibraryWithSource`, and seven
render PSOs link before the first fixture draw. No offline Metal toolchain,
licensed cache, game data, game process, benchmark or sibling worktree is used.
This report is evidence for the original fixed shaders; the parent WS32 report
owns the original-program, application-setup and whole-branch merge gates.

## Delivered paths

- `src/Mac/Metal/shader_fixed.h`
- `src/Mac/Metal/shader_fixed.cpp`
- `tests/metal/fixed_contract.cpp`
- `tests/metal/fixed_gpu.mm`
- `tests/metal/fixed_run.sh`
- This report.

No integration-owned header, shared engine source, CMake file, git index,
remote or system package was changed by this unit. The parent agent commits
these files. The source is original code, including all MSL emitted by the
source generator. No original shader or translated/extracted game content is
included.

## Implemented contract

The local helper API stays inside ABI 1. `MacMetalFixed_Init` initializes an
original provider-owned sidecar state. `MacMetalFixed_InitDefaults` initializes
only the fixed-function portion of `MacMetalState` at device creation, before
engine setters. `MacMetalFixed_Update` handles the existing typed-address shim
calls `SetFVF`, `SetMaterial`, `SetLight`, `LightEnable` and `SetClipPlane` without
extending engine layouts. Invalid indices/pointers produce D3D INVALIDCALL.
The state/dispatch owner must forward these methods to this helper; this unit
does not install a competing state provider.

`MacMetalFixed_Pack` validates first, constructs a complete immutable
`MacMetalARBConstants`, and copies it to the output only on success. Failure
includes a readable diagnostic and preserves the caller's previous output.
The packer accepts the raw D3D world/view/projection and texture matrices held
by ABI 1, rather than a projection already corrected for GL. Fixed vertex
positions therefore use D3D/Metal 0–1 clip depth directly. Applying the ARB
wrapper's `(z+w)/2` to them would be an extra conversion.

`MacMetalFixed_ExpandFVF` bounds-checks a single CPU vertex, expands float
positions/normals/point-size, packed ARGB diffuse/specular and 1–4 component
texture coordinates into the normalized 448-byte ABI-1 input, and rejects
unsupported blend-position formats. The four actual engine FVF/stride pairs
are tested: `0x152/36`, `0x242/32`, `0x102/20`, `0x142/24`
(`rb_shade.c:s_fvfForVertDeclType`). Defaults and a one-byte short input are
tested independently of GPU code. Producer-specific packed-color correction
still belongs to WS33; this helper decodes a D3D ARGB word, rather than applying
a blanket stream swizzle.

The original raster program implements:

- World/view/projection, inverse-transpose normal transformation and optional
  normal normalization; XYZRHW reconstruction including viewport origin,
  the D3D9 half-pixel offset and reciprocal W.
- Perspective interpolation tagged with the translated ARB wrapper's ABI-1
  user semantics: `color0`, `color1`, `fog`, `texcoord0..7`. Fog is a float4
  varying; only X is used by the fixed fragment body.
- Eight bounded texture stages, all **22 enabled operations** in the engine's
  `s_textureOpTable`, plus DISABLE stopping higher stages. Arguments preserve
  DIFFUSE, CURRENT, TEXTURE, TFACTOR, SPECULAR, TEMP, CONSTANT, complement and
  alpha replication. RGB and alpha evaluate against the previous CURRENT,
  then clamp per stage; RESULTARG can write TEMP independently. ARG0 is
  supported for multiply-add/LERP.
- All eight texture matrices, explicit coordinate-index selection, camera
  position (`0x20000`) and reflection (`0x30000`) coordinate generation,
  COUNT1–4 and projected division. Source
  generation specializes 2D/cube/3D argument types during preparation,
  preserving texture/sampler slots 0–7. Fragment samples use implicit
  derivatives through Metal `sample`.
- Linear, exponential and exponential-squared vertex fog, optional range
  distance, manual pretransformed fog from secondary alpha, all eight alpha
  comparisons with the normalized 8-bit reference and fragment discard.
- Material diffuse/specular/ambient/emissive sources, global ambient, eight
  directional lights, optional local viewer/specular and independent diffuse
  and specular saturation. The source-reachable `RB_DeriveEntityLights`
  produces directional lights only; enabled point/spot lights explicitly fail
  preparation rather than silently approximating them.
- Six world-space user clip planes using vertex-only Metal `clip_distance`
  outputs, preserving positive-half-space clipping.
- Original colored, solid/partial-clear, copy/scale and RGB16 gamma-LUT
  fragment entries. Clear uses the solid shader together with the target
  provider's prepared color/depth/stencil masks and stencil replacement state.
  Fog/alpha rejection are deliberately bypassed by clear/copy/gamma entries.

Undefined alpha-stage DISABLE with active RGB, color-only operations in alpha,
bump/premodulate operations absent from the engine operation table, invalid
arguments/coordinate modes/transforms/material sources/FVFs, out-of-range clip
masks, singular lighting transforms and pixel-table fog fail preparation.
Camera-space normal generation (`0x10000`) is explicitly rejected: the original
shim supports it, but no engine coordinate-generation setter emits it.
These are diagnostics, not fallback substitutions. The engine requests vertex
EXP/LINEAR fog through `rb_fog.c`, not table fog. Shader functions do not create
libraries, PSOs, samplers, encoders or GPU devices.

## Fixed constants and linking

All entries keep buffer 0 normalized ABI-1 inputs and buffer 1 the 9,216-byte
ABI-1 register block. Stream/declaration expansion is WS33's responsibility;
this original reference entry consumes normalized input records, not a legacy
vertex-buffer layout. Constants are separate from translated-program banks.

| Fixed constants | Meaning |
| --- | --- |
| env[0..3], [4..7], [8..11] | Raw D3D world, view, projection rows |
| env[12..43] | Eight texture matrices |
| env[44..46] | Inverse rows used for inverse-transpose normal multiplication |
| env[47] | Viewport x/y/width/height |
| env[48] | Bit-preserved lighting/color-vertex/normalize/specular flags |
| env[49] | Pretransformed, fog enable/mode/range flags |
| env[50..54] | Fog parameters/color, texture factor, alpha flags/function/reference, global ambient |
| env[55..58], [59..61] | Material values/power/source flags |
| env[64..127] | Eight light records, eight float4s reserved per light |
| env[128..133], [134] | Six world clip planes and enabled mask |
| local[stage*4..stage*4+3] | RGB op/args; alpha op/args; coordinates/transform/result/active; stage constant |
| local[0..255] for gamma only | Per-index normalized RGB16 LUT components |

`MAC_METAL_FIXED_REVISION` is `fixed-abi1-r1`. Cache keys must include this
revision, the generated source SHA-256, texture dimensionality tuple, ABI 1
and strict compiler options. `MacMetalFixed_SourceForTypes` returns malloc-owned
original MSL; free it after preparation. The default `Source` has process
lifetime. It is setup/loading API, not a draw-time compilation fallback.

Prepare and retain the entries named in the header:

- `fixed_vertex` with `fixed_fragment`, `fixed_colored`, `fixed_solid` or
  `fixed_copy`.
- **`fixed_post_vertex` with `fixed_gamma`**. This vertex entry accepts direct
  normalized clip/UV inputs and does not inspect `local[]` stage settings.
  Otherwise a gamma LUT would be reinterpreted as stage metadata. A failing
  spatial gamma fixture reproduced that actual collision before this repair.

Gamma caller normalizes each channel's 16-bit ramp to float by division by
65535, writes `local[index].rgb`, and keeps alpha unchanged. Copy receives
source coordinates through the packed texture-stage transform or direct UV;
texture orientation is not globally flipped. Clip z/depth, stencil masks,
scissor and target attachment formats remain state/target provider decisions.

## Verified original native enum-table repair

`rb_state.c` currently has two tentative `static const DxTextureStageEnums`
definitions, so intended combiner updates are submitted as texture stage state
0. This unit did not edit that shared engine file. The required native repair
is now verified against the read-only original Mac MP 1.3 i386 binary:

```text
RB_ChangeAlphaStageState at 0x000cce5c passes 0x0032ad5c:
    (4, 5, 6, 27) = ALPHAOP, ALPHAARG1, ALPHAARG2, ALPHAARG0
RB_ChangeColorStageState at 0x000cce78 passes 0x0032ad6c:
    (1, 2, 3, 26) = COLOROP, COLORARG1, COLORARG2, COLORARG0
```

Commands were `nm -a ~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`
filtered for `texStageEnums`/those two functions, and:

```sh
xcrun llvm-objdump --disassemble --start-address=0xcce5c \
  --stop-address=0xcce94 \
  "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
```

A local in-memory Mach-O `LC_SEGMENT` mapping and `<4I` read independently
verified those four-word constants. No dump or binary was written into git.
Exact additional native tables proposed for the integration owner are
`metalAlphaStageEnums = { 4, { 5, 6, 27 } }` and
`metalColorStageEnums = { 1, { 2, 3, 26 } }`, only under the existing
Apple/arm64/COD2_X64 guard. In the two stage-change functions, a guarded
`MacShim_Metal()` branch calls `RB_ChangeTextureStageState` with the appropriate
new table and returns; retain each original call and tentative table verbatim
for the GL/OFF continuation. Initializing the existing native tables globally
would change GL behavior, contrary to the byte-preservation requirement.

One WS24 audit detail also needs correction: D3DRS_COLORVERTEX is **141**,
not 147; 147 is AMBIENTMATERIALSOURCE. The engine's initialization call
`SetRenderState(0x93,1)` sets ambient material source to COLOR1. The helper
uses the actual enums, including LOCALVIEWER=142 and material sources
145/146/147/148. The source records are consistent with Microsoft's
[D3D render-state definitions](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3drenderstatetype).
The stage arithmetic follows the documented
[D3D texture operations](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop);
[SetClipPlane](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setclipplane)
specifies the fixed pipeline's world-space, positive-half-space rule.

## Verified texture-coordinate generation

The WS24 audit mislabeled `0x20000` as reflection. The actual
[D3D coordinate-generation flags](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtss-tci)
are normal=`0x10000`, camera position=`0x20000`, reflection=`0x30000` and
sphere map=`0x40000`. The original Mac MP 1.3 binary confirms these semantics;
there is no Mac-specific reassignment:

- `RB_ChangeGenTexCoords` at `0x000ccee2`, case 1, passes `0x20000` at
  `0x000ccfc8`, COUNT3, and inverse view with its translation removed.
- `CDirect3DDevice::ValidateRasterization` compares `0x20000` at
  `0x0001125b`; its branch at `0x00011fea` supplies the position stream and
  disables all texture generation. The separate `0x30000` branch at
  `0x00012013` enables reflection generation and supplies normals.
- `CDirect3DDevice::SetTextureCoordStream` compares `0x20000` at
  `0x00019387` and `0x30000` at `0x00019393`. The position path binds vertex
  positions as texture coordinates and composes world/view with the texture
  matrix. The reflection path uses GL texture generation.
- `COpenGL::SetTexGenEnable` emits `GL_REFLECTION_MAP=0x8512` at
  `0x000217f0` only when its reflection parameter is enabled.

Verified commands (bounded reads; no proprietary dump was written):

```sh
ref="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
xcrun llvm-objdump --disassemble --start-address=0xccee2 --stop-address=0xcd05f "$ref"
xcrun llvm-objdump --disassemble --start-address=0x1123b --stop-address=0x112da "$ref"
xcrun llvm-objdump --disassemble --start-address=0x11fea --stop-address=0x12094 "$ref"
xcrun llvm-objdump --disassemble --start-address=0x19324 --stop-address=0x194df "$ref"
xcrun llvm-objdump --disassemble --start-address=0x2172e --stop-address=0x21821 "$ref"
rg -n 'genEyeDirCoords|texScroll|genTexCoords|RB_ChangeGenTexCoords' src/PC/gfx_d3d
```

The material parser maps `genEyeDirCoords` to engine mode 1 and `texScroll` to
mode 2. `RB_ChangeGenTexCoords` maps those to camera position and transformed
passthrough, respectively; mode 0 is plain passthrough. Thus the engine emits
only generation high words 0 and 2. Normal mode 1 remains rejected with an
explicit diagnostic; reflection mode 3 is supported because the original shim
supports it and it has an independent GL fixture. Sphere-map mode 4 and unknown
modes are rejected.

The original `RB_DrawSingleTechnique` calls `RB_ChangeGenTexCoords` at
`0x000f8e6c`. The reconstructed DX7 sampler loop in `rb_shade.c` is empty,
so the current engine has no caller and does not forward these material modes.
WS33 must restore the transition for its Metal DX7 path: read
`((MaterialPassDx7 *)pass)->genTexCoords[samplerIndex]` for samplers 0 and 1,
and call `RB_ChangeGenTexCoords` when it differs from
`dxState.genTexCoords[samplerIndex]` or equals 2 (the scrolling transform changes
with the view). Keep that repair behind the native Metal guard; this unit
changes no engine or GL/OFF source. The reconstructed GL approximation also
does not implement these generation flags, so the fixture uses actual GL
position streams/matrix composition and reflection texgen as independent
references to the original shim behavior.

The initial cube fixture incorrectly associated GL reflection texgen with
`0x20000`; it has been corrected to `0x30000`. New position fixtures cover an
inverse-view texture matrix with a rotated/translated view and transformed
world, and a labeled cube whose +Z position face differs from its -Z reflection
face. Explicit GL center-pixel assertions verify both face identities.
The new CPU contract initially failed on reflection-mode acceptance; after
correcting the packer, the new position/matrix fixture still failed with the
old MSL calculation (MAE 9.60931/255, RMSE 23.3336/255, p99 53/255, 103 pixels
over 16/255). Using the camera-space vertex position for mode 2 and reflection
for mode 3 makes all cases exact. The unmerged revision remains
`fixed-abi1-r1`; the changed generated-source SHA invalidates previous caches.

## Fixture evidence and limits

```sh
sh tests/metal/fixed_run.sh
```

Final output on this Mac:

```text
PASS fixed contract: stage/coordinate rejection, FVF bytes/bounds/defaults
PASS fixed GPU 666 fixtures, runtime source libraries=3 linked pipelines=7 MAE<=0/255 RMSE<=0/255
```

The CPU suite uses ASan/UBSan, warnings as errors and FP contraction disabled.
It tests exact engine FVF sizes, packed colors, defaults, short buffers,
rejections preserving output, material/light/plane/FVF typed dispatch,
coordinate-generation acceptance/rejection and light-index bounds. GPU
compilation uses safe math and precise floating-point
functions. All Metal libraries and PSOs are created in the fixture constructor,
before its first draw. Fixtures use ordinary standalone offscreen transport;
they do not claim Metal4 game submission/residency correctness, which belongs
to WS29/WS33 and integration gates.

The 666 cases comprise 616 independent GL GLSL arithmetic-oracle combinations
(22 operations × four modifiers × seven sources), 24 real GL alpha comparisons,
six actual GL fixed texture operations, colored, three actual GL vertex-fog
modes, actual GL directional material lighting, solid/clear, copy, uniform
and spatial gamma, nonuniform labeled textures, world/texture matrices,
perspective interpolation, XYZRHW, TEMP, eight stages, user clipping, actual
GL cube reflection, camera-position cube sampling, camera position with composed
world/view/inverse-view texture matrices and actual GL volume sampling. All use
NPOT 19×13 targets.
Canonical row conversion is the known GL bottom-left to Metal top-left readback
conversion; there is no image alignment, exclusion, crop, smoothing or color
adjustment. Every compared RGBA byte is identical: MAE/RMSE/p99=0 and zero
pixels over the 16/255 threshold.

An initial nearest-sample fixture hit exact texture-center boundaries and
showed driver precision differences. Its revised labeled UV range uses
`.013+.91*x` and `.027+.87*y`, avoiding discontinuities without changing either
shader. Exact boundary tie behavior remains unverified. Another failed fixture
exposed a reference sampler mismatch (GL repeat versus Metal's default clamp),
which was corrected to explicit repeat on both backends. The spatial gamma
collision was a production defect repaired by the dedicated post vertex entry.
Failures were investigated; thresholds were never relaxed.

Still unverified by this unit: complete game DX7 visuals, arbitrary point/spot
lighting, exact nearest-boundary tie rules, all reflection face transitions,
all gamma ramp entries as spatial images, shader pairing with a programmable
stage, and full scene stencil/depth/pass/copy semantics. Geometry tests include
perspective W and a transformed textured quad, not arbitrary declaration
fetching. The native enum-table repair, DX7 material coordinate-mode transition
and state-provider forwarding are still required for engine use. No game launch
or full CONTRIBUTING/ABI/build gate was
run by this delegated unit; the parent WS32 agent owns and reports those gates.

## What WS33 can rely on

Prepare original typed fixed libraries and PSOs while loading. Install a
provider-owned `MacMetalFixedState` for the device; initialize fixed defaults
once and forward the five additional state methods above. Fix the verified
native stage tables before claiming DX7 support. Normalize declarations/FVFs
and producer colors into immutable ABI-1 records; pack separate immutable
fixed constants for every submission. Bind resources in their unchanged stage
slots, with typed nulls for absent bindings, and retain allocations/tables until
submission retirement. Fetch only ready PSOs while drawing.

All listed shader entries compile and link on this Mac; the documented
synthetic paths match GL exactly. No integration ABI change is proposed: local
capture and fixed-bank packing express the required state within ABI 1. Missing
or rejected state must keep the scene unready, never invoke an approximate
shader or compile during drawing. Full-frame parity acceptance remains the
combined renderer workstreams' gate.
