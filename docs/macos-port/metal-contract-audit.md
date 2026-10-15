# WS24: existing renderer contract audit

This is a characterization of the native arm64 renderer at the WS24 starting
revision. It accompanies [the Metal design](metal-design.md). It changes no
engine or shim behavior. Source citations below use repository paths and line
numbers; the reproducible inventory supplies each active preprocessed callsite.

The principal integration finding is that replacing the D3D9 shim vtables is
necessary but insufficient. Frame fences, render-target buffer selection,
screenshot readback and a texture prebind still call OpenGL from the engine.
They need small native-only backend dispatch points. Several D3D methods also
return success without doing the work their callers request. Metal should
implement the engine's requested semantics and use GL golden images for the
paths GL actually implements, rather than reproduce those omissions.

## Evidence and scope

The audit read every file in `src/Mac/DirectX_9`, including the native shader
headers and `D3DXShader.c`, plus WS15-render, WS18-ship and WS21-package.
`tools/metal/audit_contract.py` invokes the configured clang with
`-fsyntax-only -Xclang -ast-dump=json`. It removes object-output flags, preserves
the CMake native defines/includes, resolves constant vtable slots, and maps
recognized interfaces to the shim's actual vtable initializers. It records
unresolved indirect calls instead of silently assigning them D3D semantics.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 python3 tools/metal/audit_contract.py \
  --compile-commands build-macos/compile_commands.json > /tmp/ws24-contract.json
python3 tools/metal/audit_contract.py --trace-only \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-trace-courtyard.jsonl" \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-trace-rooftop.jsonl" \
  --trace "$HOME/Library/Application Support/CoD2-native-ws15/evidence/native-trace-sky.jsonl" \
  > /tmp/ws24-toujane-trace-summary.json
```

Keep trace files, summaries and compiled/generated shader outputs outside git.
The tool emits only aggregate trace values, not shader bodies or texture data.
It recognizes older WS15 numeric-array `nan`/`inf` spellings and reports how
many it normalized. The current trace writer emits JSON `null` instead.

The user/PLAN figure of about 277 engine callsites is not a count of named
`CDirect3DDevice_*` calls. The engine mostly dispatches through indexed COM
vtables (`rb_state.c:140`, `rb_shade.c:441`, `r_material.c:818`). Searching for
the shim function names alone misses those calls. The AST inventory is scoped
to the configured native renderer, not every legacy/preprocessor branch,
and call counts are static callsites rather than executed invocation counts.
Runtime draw evidence is separate from creation, upload, query and target
contracts.

On the configured WS24 native CMake database this command processed **22
translation units with zero compiler failures**. It found **164 indexed
indirect call expressions**, of which 93 resolve automatically to 45 distinct
shim method names. The 71 remaining expressions are emitted for inspection:
polymorphic `void *` resources, generic Release, query/CTAB/buffer interfaces,
color converters and three unrelated collision/render-command dispatches.
There are **89 statically resolved device callsites plus one dynamic
shader-create callsite** (`r_material_load_obj.c:1898`, slot 91 or 106), hence
**90 native device callsites and 41 distinct device methods** in this
configuration. This count does not include excluded real-D3D diagnostics or
expand repeated helper invocations into additional callsites.

Manual classification accounts for all **71** unresolved expressions. These
are source-verified classifications, not an assertion that the tool inferred
their interfaces. Preserve this distinction when regenerating the inventory.

| Unresolved category | Count | Evidence / reason for classification |
|---|---:|---|
| Vertex-buffer Lock/Unlock/Release | 14 | `r_buffers.c:45/57/65`, `r_model.c:216/367/375`, `r_staticmodelcache.c:367/889/927/1045/1074`, `r_xsurface_optimize.c:81/86/195`; receiver casts are `void *` but variables/fields refer to VBs. |
| Index-buffer Lock/Unlock/Release | 4 | `r_buffers.c:87/90/101/109`; generic buffer typedefs erase the IB type. |
| Texture views/descriptions/locks/releases | 15 | `r_image.c:283/509/789/794/1073`, six calls in `r_image_load_common.c:288–387`, four raw cinematic texture calls in `rb_backend.c:994/1011/1019/1031`; mapType branches establish 2D/cube/volume dispatch. |
| Surface AddRef/Release/GetDesc | 11 | Six calls in `r_rendertarget.c:80/89/155/197/204/297`, five in `rb_backend.c:1030/2284/2330/3556/3877`; each field/output is an IDirect3DSurface9. |
| Occlusion-query Issue/GetData | 6 | `rb_sky.c:84/96/108/802/863/870`; generic Query typedefs, slots 6/7. |
| D3DX CTAB / compiler-buffer access and release | 7 | `r_material_load_obj.c:883/1266/1850/1852/1873/1874/1902`; slots 2/3/4 backed by `D3DXShader.c` buffer/table vtables. |
| Generic resource/shader/declaration Release | 5 | `r_init.c:182` and `r_material.c:397/409/699/713`; common COM release helpers deliberately accept multiple object kinds. |
| D3D interface CreateDevice / multisample check | 2 | `r_init.c:307/1009`, slots 16/11; `d3d9` / `d3d` are void pointers. |
| Dynamic device shader creation | 1 | `r_material_load_obj.c:1898`; prior local expression selects 91 for VS or 106 for PS. |
| Shim color conversion | 3 | `r_bsp_load_obj.c:1274`, `r_staticmodelcache.c:907/1062`; converter slot0, not a D3D device slot. |
| Non-shim indexed dispatch | 3 | Collision trace at `r_utils.c:97`, tess dispatch at `rb_backend.c:1912`, render-command dispatch at `rb_backend.c:3255`; must not assign D3D semantics just because they use indexed function pointers. |

The two D3D-interface calls require `CDirect3D_CreateDevice` to choose backend
and `CheckDeviceMultiSampleType` to return supported sample counts/quality.
The current GL implementation creates a screen context and a static device
object (`CDirect3D.c:210`); its multisample check always returns success
without populating the quality output (`159`). A Metal capability check must
fill what the engine consumes. It must not promise arbitrary MSAA modes.

## Device methods required by engine source

Slot numbers below are indices, retaining the original `/ 4` expressions in
callers even though the native table entries are pointer-sized. Prefix every
device method name in this table with `CDirect3DDevice_`. Diagnostic-only
getters are explicitly identified and are excluded from the native count.

| Slots / methods | Engine evidence and semantics | Current GL implementation |
|---|---|---|
| 3 `TestCooperativeLevel`, 16 `Reset` | `r_init.c:978`, `1050`: lost-device recovery and display-mode changes. | Test always succeeds. Native Reset validates the presentation array, configures the window/mode and updates backbuffer dimensions (`CDirect3DDevice.c:2846`). Resource restoration belongs to the engine's reload paths. |
| 4 `GetAvailableTextureMem` | `r_texturemem.c`: renderer video-memory budget query. | Returns the device's stored textureMemory (`CDirect3DDevice.c:1480`). Metal should keep engine resource budgeting meaningful. |
| 14 `GetSwapChain`, 18 `GetBackBuffer` | `r_rendertarget.c:247`, `259`: initialize framebuffer aliases. | GetSwapChain succeeds without writing output (`3021`); GetBackBuffer returns the retained backbuffer and AddRefs it (`1613`). Only one framebuffer/swapchain is intended. |
| 17 `Present`, 41 `BeginScene`, 42 `EndScene` | `rb_backend.c`, `rb_sky.c:77/103`: frame and query brackets. | BeginScene opens a requested trace; EndScene is a no-op; Present captures pending screenshots, closes the trace and calls SDL GL swap (`1634`, `1644`, `1713`). Treat EndScene as an engine boundary, not existing GPU completion. |
| 21 `SetGammaRamp` | Gamma is driven by the platform display path as well as the D3D interface. | Forwards the RGB 16-bit LUT to `MacDisplay_SetGammaRamp` (`2869`). GetGammaRamp is a no-op. Screenshot output also uses `R_GammaCorrect`. |
| 23 `CreateTexture`, 24 `CreateVolumeTexture`, 25 `CreateCubeTexture` | `r_image.c:157`, `193`, `233`, `1008`, `1039`: loaded images, light volumes, cubemaps, sampled render targets and system images. | Allocates shim objects plus contiguous CPU mip storage and GL textures (`1135` onwards). Managed/default/system-memory pool and usage are mostly ignored. Requested level count and exact native image dimensions matter. |
| 26 `CreateVertexBuffer`, 27 `CreateIndexBuffer` | `r_buffers.c:40`, `r_init.c:731/742/751`: static WRITEONLY data and dynamic WRITEONLY buffers. Dynamic IB is INDEX16, size `0x200000`, usage `0x208`; dynamic VB sizes come from engine pools. | CPU allocations, no native GL buffer backing (`1260` onwards). IB length rounds up to four bytes, INDEX16 means 2-byte indices and any other format selects 4-byte storage. Pool ignored. |
| 29 `CreateDepthStencilSurface` | `r_rendertarget.c:101/166/276`: 128-square shadow targets and full-size scene depth; D24S8. | Allocates surface bookkeeping/storage; does not establish a working GL depth/stencil render-target attachment (`1348`). Multisample/discard flags ignored. |
| 34 `StretchRect` | `rb_backend.c`: ROQ frame-to-backbuffer blit and post/resolve copy paths. Source/destination rectangles and target identities are meaningful. | Source texture is drawn as an orthographic quad into the current framebuffer; destination surface and filter arguments are ignored (`2900`). This is enough for cinematic playback but not a general target copy/resolve contract. |
| 37 `SetRenderTarget`, 39 `SetDepthStencilSurface`; diagnostic 38 `GetRenderTarget` | `rb_state.c:192/202`: switch engine targets; real-D3D diagnostic getter at `rb_shade.c:338`. | Setters store pointers only (`2778`, `2799`); SetRenderTarget ignores index. GetRenderTarget AddRefs. Direct engine GL draw/read-buffer changes supply part of the old target behavior. GetDepthStencilSurface returns a separate `depthBuffer` field, not the setter's `depthStencil` field (`2812`). |
| 43 `Clear` | `rb_backend.c`, `r_screenshot.c:90`: target/depth/stencil bitmask, packed ARGB, depth float, stencil byte; no rectangle batches at examined callers. | `glClearColor/Depth/Stencil`, temporary full write masks and `glClear` (`1653`). Count/rectangle arguments ignored; special Flags=TARGET and Color=0 path skips work. Metal clear must order against prior draws and preserve unaffected attachment contents. |
| 44 `SetTransform`, 47 `SetViewport`; diagnostic 48 `GetViewport` | `rb_state.c:164/174`, DX7 world/view/projection and texture transforms; real-D3D diagnostic at `rb_shade.c:337`. | Copies world=256, view=2 and projection=3 matrices; all eight texture matrices alias one stored matrix (`2435`). Viewport immediately calls `glViewport/glDepthRange`, getter returns six words (`2475/2493`). |
| 49 `SetMaterial`, 51 `SetLight`, 53 `LightEnable` | `rb_state.c:183/1340`, `rb_shade.c:675/681/689`: DX7 light/material path and up to 8 lights. | All no-ops (`2505` onwards). Programmable lighting is supplied through shader constants and 3D textures, not these calls. A fixed-function emulation path needs explicit light/material state. |
| 55 `SetClipPlane` | `rb_backend.c:949`: render-command plane upload, with enabled-plane bitmask state152. | Empty setter (`CDirect3DDevice.c:2541`), and state152 is ignored. Plane requests do not establish working clipping in GL. |
| 57 `SetRenderState`; diagnostic 58 `GetRenderState` | `rb_state.c:154`, material state decoders; getter in real-D3D draw diagnostics. | A selective cache/setter (`2209`) and incomplete getter (`2330`); see state table below. Unhandled values return success and are not remembered. |
| 65 `SetTexture`, 67 `SetTextureStageState`, 69 `SetSamplerState`; diagnostic 64 `GetTexture` | `rb_state.c:212/222`, `rb_shade.c:2157`: image bindings, fixed stages, sampler settings; diagnostic getter at `rb_shade.c:352`. | Global 16 texture bindings, 8×33 stage values and 16×14 sampler values (`2380` onwards). Set/GetTexture do not AddRef; texture objects are discriminated by their GL vtable identity. Resource ownership cannot assume standard COM retention here. |
| 82 `DrawIndexedPrimitive` | `rb_shade.c:441`: primitive type 4 (triangle list), base vertex, minimum index, vertex span, first index and triangle count. | Native ARB draw uses all declaration streams, CPU index/vertex data and `glDrawElements(GL_TRIANGLES, count×3, GL_UNSIGNED_SHORT, …)`; type argument is ignored (`1757`, `lp64_shader_draw.h:49`). Other DrawPrimitive / Draw*UP methods are stubs. |
| 86 `CreateVertexDeclaration`, 87 `SetVertexDeclaration`, 89 `SetFVF` | `r_material.c:818/931`, `rb_shade.c:948/1365`, `rb_state.c:725`: four material declaration variants and DX7 FVF selection. | Declaration copies the terminated D3D element array. Set stores the active global declaration. SetFVF only stores a word; does not build GL vertex layout (`2610` onwards). `RB_ReleaseVertexDecl` uses slot 89 despite its misleading SetVertexDeclaration typedef; the actual operation is SetFVF(0). |
| 91 `CreateVertexShader`, 92 `SetVertexShader`, 94 `SetVertexShaderConstantF` | `r_material.c:843`, `rb_shade.c:956/1597/1638/1675`: original ARB program creation and float4 register spans. | Creates ARB vertex program; setter binds/enables it; constant setter mirrors and calls `glProgramEnvParameter4fvARB`, up to 256 registers (`2676`). No integer/bool constant setters are active in this audit. |
| 100 `SetStreamSource`, 104 `SetIndices` | `rb_state.c:352/371`: buffer pointer, byte offset and stride; deselection references protect engine bookkeeping. | Stores buffer pointers/offsets/strides with no AddRef, streams[16] (`2565`). Draw reads CPU backing directly. Stream frequency/instancing methods are stubs and not required by examined draw callers. |
| 106 `CreatePixelShader`, 107 `SetPixelShader`, 109 `SetPixelShaderConstantF` | `r_material.c:849`, `rb_shade.c:963/1709/1739/1777`: fragment program and register spans. | ARB compile/bind; env constants uploaded through the selected pixel shader's secondary method and trace mirror (`2700`). CreatePixelShader logs compilation trouble yet returns success, unlike the vertex create path. |
| Diagnostic 93 `GetVertexShader`, 95 `GetVertexShaderConstantF`, 108 `GetPixelShader`, 110 `GetPixelShaderConstantF` | `rb_shade.c:350/351` and real-D3D diagnostic block: original draw-trace introspection. | No-op getters that do not fill caller outputs. They are diagnostic requests, not working GL reflection/readback. The current native trace accesses private mirrored state instead. |

`GetRenderTargetData` (32), `CreateOffscreenPlainSurface` (36), surface readback
locks and extra texture getters also appear in real-D3D diagnostic branches.
Do not count an excluded Windows diagnostic branch as a native dependency.
The shim exposes these methods but UpdateSurface, UpdateTexture,
GetRenderTargetData, GetFrontBufferData and ColorFill are empty (`3048`
onwards). State blocks, palettes, scissor, software-VP, patches and most Get*
reflection methods are likewise empty. There is no evidence that native
Toujane draws require tessellation, instancing, palette sampling or point/line
primitive dispatch. Capabilities must describe implemented behavior, rather
than reuse GL's optimistic capability words unchanged.

## Resource calls and lifetime

| Object / used methods | Engine evidence | GL mapping / required behavior |
|---|---|---|
| Texture2D: Release(2), GetSurfaceLevel(18), LockRect(19), UnlockRect(20) | `r_image.c:283/509`, `r_image_load_common.c:324`; dynamic images use the same upload machinery. | Each mip owns an associated surface view; GetSurfaceLevel AddRefs it. Lock marks the mip dirty and delegates CPU pitch/pointer to the surface; Unlock uploads dirty level then clears the dirty mask (`CDirect3DTexture.c:98/109/122/324`). Mip chain can stop above 1×1 and native GL_MAX_LEVEL is set to the last allocated mip. |
| All texture kinds: GetLevelDesc(17) | `r_image.c:789/794`: image-list/debug command reads format at mip0 for volume, cube or 2D images. | Implemented per-kind mip description; generic typedef hides interface from the automatic inventory. Description layout is D3DVOLUME_DESC for volume and D3DSURFACE_DESC for 2D/cube. |
| Cube: Release(2), LockRect(19), UnlockRect(20) | `r_image_load_common.c:335` and image release. | Face enum 0–5 maps to POSITIVE_X + face; surfaces indexed level×6+face. Unlock uploads that cube face/mip. GetCubeMapSurface implements a retained face view even though examined engine image upload uses locks directly (`CDirect3DCubeTexture.c:98` onwards). |
| VolumeTexture: Release(2), LockBox(19), UnlockBox(20) | Native `r_image_load_common.c:288/297`: mapType=4, every slice copied using RowPitch and SlicePitch. | Level volumes own width×height×depth storage; lock ignores Box/Flags, marks dirty and returns pitched storage. Texture unlock uploads the dirty 3D level via glTexSubImage3D / compressed upload (`CDirect3DVolumeTexture.c:85/290/302/465`). Preserve depth and channel order; WS15's volume BGRA fix affects model lighting. |
| Surface: Release(2), GetDesc(12), LockRect(13), UnlockRect(14) | `r_rendertarget.c:298`, image surface ownership and native/legacy screenshot paths. | GetDesc fills format/width/height, not every D3DSURFACE_DESC member (`CDirect3DSurface.c:170`). CPU memory lock returns block or pixel pitch; missing backing uses a static 4 MiB scratch buffer. LockRect marks dirty even for READONLY; rectangle offsets are not correct compressed-block offsets. Unlock uploads owner texture. Metal needs real readback storage for render targets, not this scratch behavior. |
| VB: Release(2), Lock(11), Unlock(12) | `r_buffers.c`, `r_staticmodelcache.c`, `rb_shade.c:871/913`: static upload, dynamic vertex rings and skinned cache. | Lock returns data+offset, ignores size/flags; Unlock is empty (`CDirect3DVertexBuffer.c:54/63`). Engine can reuse/wrap the same buffer. Native VB methods WaitFence/SetFence are empty (`185/193`). Immutable uploads and dynamic ring ranges need GPU-lifetime tracking behind the same CPU pointer contract. |
| IB: Release(2), Lock(11), Unlock(12) | `r_buffers.c`, `rb_backend.c` / geometry assembly. | CPU storage, size=0 means full recorded buffer length, offset included in pointer, flags ignored; Unlock leaves lock bookkeeping unchanged (`CDirect3DIndexBuffer.c:66/79`). Native draw currently requires 16-bit data; INDEX32 allocation support alone does not prove a 32-bit draw path. |
| Shader and declaration Release(2) | `r_material.c` reload/shutdown; `R_SafeRelease` helpers. | Standard decrement and destroy. Shader objects have primary plus secondary interfaces containing backend-specific getters/setters (`CDirect3DPixelShader.c`, `CDirect3DVertexShader.c`). A Metal implementation must preserve table slot shape while replacing GL payload. |
| Query Issue(6), GetData(7) | `rb_sky.c:84/96/108/802/863/870`: sun-flare sample count, Issue BEGIN=2 / END=1, GetData four bytes with FLUSH=1. S_FALSE=1 means pending; success=0 requires populated sample count. | `QueryStub` ignores query type, Issue does nothing, GetData returns success without filling output (`CDirect3DDevice.c:239/281/1414`). No initialized native CreateQuery callsite was found in the setup path; null checks suppress most use. Implement real asynchronous occlusion semantics if sun queries are enabled. Never equate this stub with a fence. |

Most object QueryInterface implementations return the same object and AddRef
without checking IID. The query stub's QueryInterface does not AddRef. GetDevice,
private-data, priorities and preload methods are mostly no-ops. Texture LOD,
autogen filter, GenerateMipSubLevels and AddDirtyRect are not working
regeneration controls. Material/image loading supplies the mip chain explicitly.

The current CPU allocator (`CMemoryBuffer.c`) provides aligned allocations
and frame-count deferred freeing. Frame age is not proof that a Metal command
buffer has completed. Resource deletion and ring reuse require backend
completion tracking without exposing Metal details to these callers.

`COpenGL`, `COpenGLVAO`, `CVAOPacket` and `CVertexArrays` describe internal GL
state/array helpers. Many of their reconstructed validation and dispatch
functions are stubs; the working native ARB path binds declarations directly
from `lp64_shader_draw.h`. The three engine color-converter calls are an
actual shared data contract. There is no requirement to clone internal GL
helper classes in Metal when the active object vtables and producer byte
ordering can be preserved directly.

## Render states actually requested

Source: `rb_state.c:1090` (state word 0), `735` (state word 1), `1376` (decode
tables), `rb_fog.c`, `rb_shade.c:1350`, and initialization at `rb_state.c:1327`.
The draw trace only contains the subset marked **trace** below.

| D3D state numbers | Requested semantics / source value range | GL treatment |
|---|---|---|
| 7 ZENABLE, 14 ZWRITEENABLE, 23 ZFUNC (**trace**) | Enable/write booleans; material comparisons LESS_EQUAL=4, EQUAL=3, ALWAYS=8. | Cached and applied using GL depth test/mask/function. All eight compare enum values supported by mapper. |
| 195 DEPTHBIAS, 175 SLOPESCALEDEPTHBIAS | Float-bit words; polygon offset count 0–3 multiplied by dvars. Depth bias includes `1/65536` factor in engine. | Setter bitcasts, then `glPolygonOffset(slope, bias×16777216)`. Preserve equivalent final depth shift, rather than copy GL's units factor blindly. Neither is in the draw trace. |
| 15 ALPHATESTENABLE, 24 ALPHAREF, 25 ALPHAFUNC (**trace**) | Material alpha cases: GREATER=5/ref0, LESS=2/ref128, GREATER_EQUAL=7/ref128. | Alpha reference normalized by 255 and legacy glAlphaFunc applied. Metal emulates the comparison/discard after fragment output; shader saturation does not replace alpha testing. |
| 27 ALPHABLENDENABLE, 19 SRCBLEND, 20 DESTBLEND, 171 BLENDOP (**trace**) | Blend op table ADD/SUBTRACT/REVSUBTRACT/MIN/MAX; factors decoded from ZERO through SRCALPHASAT plus BOTH factors. | GL map covers the active ordinary factors; native shader draw calls separate equation/function when requested. Color-write mask is independent. |
| 206 SEPARATEALPHABLENDENABLE (**trace**), 207 SRCBLENDALPHA, 208 DESTBLENDALPHA, 209 BLENDOPALPHA | Separate RGB and alpha equations/factors from the second material blend fields. | Stored and applied in native ARB path. The trace omits 207–209, so its state dictionary alone is an incomplete PSO key. |
| 22 CULLMODE, 168 COLORWRITEENABLE (**trace**) | NONE/CW/CCW decoded through `{0,1,3,2}`; write mask RGB=7 and A=8. | Native ARB maps cull2 to GL front CCW and cull3 to front CW, both backface cull; mask bits map RGBA. Approximate draw path disables culling. Coordinate conversion must preserve the effective winding. |
| 8 FILLMODE | Material wireframe: WIREFRAME=2 / SOLID=3. | Setter ignores it; GL parity cannot establish wireframe correctness. |
| 52 STENCILENABLE, 185 TWOSIDEDSTENCILMODE, 53/54/55 fail/zfail/pass, 56 func, 186/187/188 CCW fail/zfail/pass, 189 CCW func, 57 STENCILREF | Stencil material state decoder uses all 1–8 comparison/operation enums and front/back sets (`rb_state.c:1384`); render-command ref value at `rb_backend.c:1428`. No explicit read-mask/write-mask setter was found. | All ignored by current setter, despite advertised StencilCaps. Need explicit defaults and depth/stencil pipeline state for target/post correctness. None is in trace. |
| 152 CLIPPLANEENABLE | `rb_backend.c:935` emits `(1<<planeCount)-1` alongside plane coefficients. | Ignored; SetClipPlane is also empty. |
| 28 FOGENABLE, 34 FOGCOLOR, 36 FOGSTART, 37 FOGEND, 38 FOGDENSITY, 140 FOGVERTEXMODE | Engine DX7 fog branch requests exp/linear fog and float ranges. | FOGENABLE stored; color sent glFogfv; range/density/mode ignored. Native ARB draw disables GL fog because programmable materials handle fog. |
| 60 TEXTUREFACTOR | Packed ARGB stage constant via `RB_SetSamplerConstantDx7`. | Stored, unpacked for GL texture-env constant. |
| 137 LIGHTING, 143 NORMALIZENORMALS, 147 COLORVERTEX | DX7 lighting enable; normalized normals; initialization color vertex true. | Ignored by setter. Both native shader and approximate draw paths disable GL lighting. |
| 181 vendor alpha-AA token | `RB_SetAlphaAntiAliasingState` sends zero, SSAA FourCC or ATOC FourCC when supported. | Ignored. A native alpha-to-coverage implementation must explicitly opt in and check MSAA sample count. |

No active native engine setter for SRGBWRITEENABLE, SCISSORTESTENABLE
or palettes was found by the source search. Do not infer an
sRGB pipeline from a D3D9 enum being declared in `cod2_defs.h`.

## Texture stage and sampler states

`CDirect3DDevice_SetTextureStageState` remembers Stage<8 and State<33;
`SetSamplerState` remembers Sampler<16 and Type<14. Apply logic is in
`CDirect3DDevice.c:820` and `1050`.

Engine sampler changes (`rb_state.c:878/409`) request ADDRESSU=1,
ADDRESSV=2, ADDRESSW=3, MINFILTER=6, MAGFILTER=5, MIPFILTER=7 and MAXANISOTROPY=10.
Address bits select WRAP=1 or CLAMP=3. Filter decode supports NONE=0,
POINT=1, LINEAR=2 and ANISOTROPIC=3; anisotropic magnification becomes linear.
GL also implements MIRROR address=2 and W=3 if supplied. Native upload/draw
applies min/mag/mip filtering and S/T/R wrapping per bound texture.
MAXANISOTROPY is cached but never applied: anisotropic minification falls back
to linear. Border color, mip LOD bias/max level and sampler sRGB are ignored.
Do not confuse per-texture allocated mip limit with sampler MAXMIPLEVEL.

Texture coordinates: TEXCOORDINDEX=11 is either sampler index or
CAMERASPACEREFLECTIONVECTOR `0x20000`; TEXTURETRANSFORMFLAGS=24 is DISABLE=0
or COUNT3=3. Engine also submits a texture transform (`rb_state.c:523`).
The GL approximation uses selected UV offsets/texture units and a single
stored texture matrix, not faithful per-stage coordinate generation.

The intended color/alpha decoder lists DISABLE, SELECTARG1/2, MODULATE,
MODULATE2X/4X, ADD, ADDSIGNED/2X, SUBTRACT, ADDSMOOTH, alpha-blend variants,
premultiplied texture-alpha blend, modulate/add variants, DOTPRODUCT3,
MULTIPLYADD and LERP (`s_textureOpTable`, `rb_state.c:1376`). Argument sources
are CONSTANT, CURRENT, DIFFUSE, SPECULAR, TEMP, TEXTURE, TFACTOR; modifiers
COMPLEMENT=`0x10`, ALPHAREPLICATE=`0x20` (`1378`). GL combine implements only
a subset and uses fallback modulation/replacement for unknown operations.

There is a reconstruction defect before that mapper: both `texStageEnums`
objects are tentative `static const` definitions with no initializer
(`rb_state.c:20/21`). Their op/arg enum words are therefore zero.
`RB_ChangeTextureStageState` submits state 0 for the color/alpha decoder, not
the intended COLOROP/ARG and ALPHAOP/ARG slots. This is source-verified and
must not be described as complete fixed-function support. A separate native
fixed-function workstream should restore/verify those enum tables and test
DX7 combinations; the approximate GL screenshots are not sufficient proof.

## Formats, mips and uploads

The actual IWI loader dispatch (`r_image_load_obj.c:348`) requests A8R8G8B8,
X8R8G8B8, A8L8, L8, A8 and DXT1/3/5. The bitmap and wavelet paths share these
formats; 24-bit input expands to four CPU bytes for X8R8G8B8. Generated lookup
textures include L8 and ARGB color, and generated lighting uses 3D texture
storage. Render targets use backbuffer X8R8G8B8 or A8R8G8B8; depth surfaces
request D24S8. Index uploads request INDEX16. `Image_FormatMemory` also knows
R5G6B5, D32, D16 and R32F; that sizing switch does not prove they are used by
the current asset loader.

| Requested format family | GL upload mapping in `MacOpenGLUtils.c:295` | Backend requirement / limitation |
|---|---|---|
| DXT1 / DXT3 / DXT5 | S3TC compressed RGBA DXT1 / DXT3 / DXT5; 8 / 16 / 16 bytes per 4×4 block. | Preserve ceil(width/4)×ceil(height/4)×depth block sizes and partial final mips. Metal resource design must verify runtime BC support and supply a local decode fallback if unavailable. No BC4–7 inputs found in loader. |
| A8R8G8B8 / X8R8G8B8 | GL_RGBA storage, GL_BGRA unsigned-byte input. | Byte order and X8 alpha semantics matter; choose view/swizzle/upload conversion deliberately. |
| A8 / L8 / A8L8 | Legacy GL_ALPHA / LUMINANCE / LUMINANCE_ALPHA. | Modern single-/dual-channel textures need sampling swizzles yielding legacy RGBA values, not raw `(r,0,0,1)` by accident. |
| R5G6B5 / A1R5G5B5 / A4R4G4B4 | Packed unsigned-short uploads with selected RGB/BGRA ordering. | Implement explicit packed conversion if a direct native format is unavailable. These shim capabilities are broader than the verified IWI format requests. |
| D24S8 | GL_DEPTH_COMPONENT + unsigned byte in helper; not a functioning depth/stencil attachment path. | This is a GL omission. Metal needs a compatible depth+stencil attachment and clear/query ownership. |

Palette P8/A8P8 and DXT2/4 occur in generic sizing/type utilities but no current
asset/upload request for them was found. The shim maps DXT2 to DXT3; DXT4 is
classified as compressed but the GL type switch lacks its corresponding
upload mapping. R32F is counted by engine memory accounting but the helper
does not supply a float GL texture mapping. These are latent gaps, not
evidence of active features to implement immediately.

The native 2D and cube constructors retain exact NPOT dimensions and set the
last available mip through GL_MAX_LEVEL. Full-chain allocation is used when
Levels=0, capped at 16 by these implementations. The asset loader can supply
truncated cube chains (`r_image.c:207`). Preserve level count, per-face layout
and all volume slices. Upload visibility must be complete before a draw that
samples a freshly unlocked texture. Metal can batch uploads, but must enforce
that dependency within its encoder/queue ordering.

## Vertex declarations, streams and draws

Material declaration construction is data-driven but its source/destination
tables are source code (`r_material.c:1332/1334`), not extracted shader data.
Four variants use stream 0 only; their types are FLOAT2=1, FLOAT3=2,
FLOAT4=3, D3DCOLOR=4 and SHORT2=6. Destinations are POSITION0, NORMAL0,
COLOR0/1 and TEXCOORD0–7. Each variant appends the eight-byte terminator
Stream=255, Type=17. Elements are copied into the declaration object,
including the terminator (`CDirect3DVertexDeclaration.c:222`).

The native ARB attribute route (`lp64_shader_draw.h:33`) is POSITION/POSITIONT
→0, BLENDWEIGHT→1, BLENDINDICES→2, NORMAL→3, PSIZE→4, COLORn→5+n,
BINORMAL→6, TANGENT→7, TEXCOORDn→8+n. The implementation additionally accepts
FLOAT1, raw UBYTE4, normalized UBYTE4/SHORT2/4/USHORT2/4. Packed DEC3 and
FLOAT16 formats fail the draw instead of being decoded. Those extra types
are implemented input support, not proof they are requested by material
declaration tables.

Six dynamic vertex strides have explicit CPU color conversion in
`RB_SetVertexData` (`rb_shade.c:880`): `0x18`, `0x20`, `0x24`, `0x40`, `0x44`
and no-color `0x14`. The native ARB path corrects different producer byte
orders for world/FX/skinned data (`lp64_color_order.h`). This work is essential
for faithful lit models and FX. Defining one global BGRA color vertex format
would regress the WS15/WS18 fixes. Attribute defaults for disabled attributes
1–15 are `(0,0,0,1)`; position0 is not given that fallback.

The engine performs indexed triangle-list draws. The active native ARB draw
requires compiled VS+PS, a declaration, CPU IB data and indexSize=2; it checks
the requested index range against buffer length. Stream pointer is
`data + streamOffset + baseVertex×stride + elementOffset`; the original index
values stay unchanged. This matters when moving from client pointers to a
Metal vertex/index ring. Do not apply base vertex both in upload relocation
and draw arguments. The current trace stores these numeric draw parameters,
but no actual indices or stream contents.

When original programs are unavailable, `CDirect3DDevice.c:1810` follows a
material/stride heuristic path with stream 0, fixed GL arrays and selected
color/base/lightmap textures. It sets its own matrices and HUD ortho,
disables lighting/fog/culling, omits shader tangent/binormal inputs and does
not provide full programmable light-volume behavior. This is the known
approximate renderer, not fixed-function fidelity. Shader cache setup and
valid original-program selection are prerequisites for GL reference images.

The native projection helper (`MacOpenGLUtils.c:455`) applies the D3D
integer-center to GL half-integer-center shift of `(+1/viewportWidth,
-1/viewportHeight)` in clip-space matrix terms, converts clip depth through
`2*z-w`, and compensates the legacy view Z convention. `RB_GetCodeMatrix`
also supplies an OGLworldViewProjection variant to programmable shader
constants. These converted constants/state bindings must be accounted for
when defining translated shader output space; do not add both a D3D-to-GL
projection conversion and an independently assumed D3D-to-Metal conversion.
Viewport MinZ/MaxZ is a separate requested state and can change for depth
hacks (`rb_state.c:696`). HUD has a separate orthographic/top-left convention.

## Shader object and metadata contract

The extracted cache has **834 payload files but 417 programs**: 208 `.vsa`
vertex programs, 209 `.pse` fragment programs and matching `.vc` / `.pc`
constant metadata. The metadata is consumed by the engine's constant setup;
it is not another ARB program. WS21's native extractor validates the local
player-owned source and writes the private manifest/cache. The translator
must preserve register/state binding, attributes/outputs and reflection
compatibility, not just emit compilable arithmetic.

`D3DXShader.c` and `lp64_shader_cache.h` load the chosen ARB payload by
material filename and attach a `#COD2CTAB:` filename marker, then hand program
text to shim shader constructors. The constant-table loader reads the
separate metadata payload named by that marker. CTAB serialized offsets remain
32-bit and `GetBufferPointer` exposes the raw constant table. Most public
constant-table reflection methods are empty; engine code interprets the
buffer directly. Preserve that handoff while adding private MSL/Metal state.
Do not substitute pointer-sized fields into the serialized CTAB layout.

`lp64_shader_cache.h` removes unused vertex OUTPUT alias declarations
before GL compilation: WS15 verified that unused sky color aliases could
corrupt texcoord routing in the GL optimizer. Preserve ARB semantics in IR;
do not make this driver workaround a new shader-language rule. Float
contraction is off in the native build and representative GPU tests should
record compiler math/optimization settings.

Shader text, metadata, translated MSL, libraries and pipeline caches stay
local. No copied program or translated game shader is included in this audit.

## Direct OpenGL escape paths and synchronization

| Engine path | Source evidence | Required integration seam |
|---|---|---|
| Frame GPU fence | `rb_backend.c:637`, `1152`, `1237`, `1279`, `3189`, `3385`, `3434`: glGen/Set/Test/DeleteFenceAPPLE and `flushGpuQueryIssued`. | Backend frame submit/completion token. Test must correspond to the renderer's GPU queue; do not leave APPLE fences on an idle GL context when Metal draws. Existing CFence and VB fence methods are stubs. |
| Render-target selection | `rb_state.c:1000`, `1031–1044`: GL_BACK / GL_AUX1 / GL_AUX0 read/draw buffers after SetRenderTarget. | Native backend target binding and pass transition, with GL dispatch retaining current behavior. Metal attachments correspond to engine target objects, not GL buffer constants. |
| EndSurface texture prebind | `rb_shade.c:2130–2177`: directly reads texture object+`0x54`, updates `g_prebind_texID/Target`, calls glBind/Enable/Disable. | Backend-neutral texture binding or native-only guard around legacy heuristic prebind. Metal objects cannot share this GL integer field contract. This is also a stale LP64 layout hazard. |
| Screenshots / cubemap shots | `r_screenshot.c:125–219`: GL state stacks, read/draw selection, glReadPixels and front-buffer retry; Present captures before swapping. | Backend readback method with deterministic frame timing, row orientation, gamma correction and CPU result lifetime. Retain screenshot behavior without GL context. |

The native CMake command already maps glDrawBuffer/glReadBuffer to
`MacGL_DrawBuffer/ReadBuffer`, which clamp unsupported auxiliary buffers.
Those wrappers still represent GL behavior, not Metal target ownership.
This reinforces the need for explicit render-target contracts instead of
assuming the reconstructed old GL AUX path supplies offscreen parity.

`gpuSync` and buffer wrap/lock flags regulate engine reuse, but the actual
frame fence is global engine state. Dynamic locks can request DISCARD=`0x2000`
when offset=0 or GPU sync is disabled; static locks use other flags. Current
CPU backing tolerates it because GL consumes client arrays synchronously at
draw issue. Metal must retire dynamic ranges after submitted GPU completion,
and keep old resources alive while outstanding commands reference them.

## Toujane trace coverage and matched-image reference

`lp64_draw_trace.h` is request-file controlled. Set COD2_MAC_DRAW_TRACE before
the first BeginScene, since the environment value is cached once. Write a
newline-terminated output path into that request file while the game runs.
The next BeginScene removes the request and opens the output; Present closes
it. EndScene does not close it. Request/output paths must stay private.

```sh
COD2_MAC_DRAW_TRACE=/tmp/ws24-draw-request \
COD2_MAC_SHADER_CACHE="$HOME/Library/Application Support/CoD2 Silicon/shaders" \
  build-macos/cod2_macos +set fs_basepath "$HOME/Games/CoD2" \
  +set fs_homepath "$HOME/Library/Application Support/CoD2-native-ws24" \
  +set com_maxfps 333 +set r_aaSamples 1 +devmap mp_toujane
printf '%s\n' /tmp/ws24-toujane.jsonl > /tmp/ws24-draw-request
```

The example is a capture control, not a timed benchmark. Performance work
requires PLAN's external benchmark lock and screen/background-work record.
The audit itself did not launch the game or measure performance.

The current trace contains draw counts/ranges, 2D flag, material/shader
labels, view origin, 256 float4 VS/PS mirrors, 13 render-state words,
16×14 sampler words, 8×33 texture-stage words and texture object/GL ID/target.
It omits geometry bytes, declaration layout, render-target identities,
viewport, clear transitions, depth bias, stencil, separate-alpha factors,
texture dimensions/formats/texels and fences. A trace-driven PSO key therefore
needs extension before claiming complete prewarming. An offscreen captured
Toujane batch needs a private geometry/resource capture in addition to this
trace; replay cannot reconstruct it from numeric draw counts alone.

Only old local WS15 traces were available to this audit; no fresh WS24 game
capture was taken because another workstream held the benchmark lock. WS15's
final report records native matched views of 117, 118 and 4
draws; its old files named `native-trace-*.jsonl` may predate that result and
contain nonfinite constants. The audit tool reports the file's actual count
and normalized-value count rather than assigning the report's count to it.

| Local old WS15 capture | Draws | Triangles submitted | Recorded state sets | Nonfinite constants normalized |
|---|---:|---:|---:|---:|
| courtyard | 143 | 105,028 | 15 | 286 |
| rooftop | 164 | 108,528 | 15 | 328 |
| sky | 77 | 53,655 | 14 | 154 |

All three contain type4 draws, no 2D draws, depth enabled, ZFUNC=LESS_EQUAL,
blendop=ADD and separate-alpha disabled. Observed cull values are NONE=1 and
CCW=3; color masks RGB=7 and RGBA=15. Texture targets include GL_TEXTURE_2D
(3553) and CUBE_MAP (34067). These captures do not exercise all static paths:
in particular they do not prove volume textures, stencil, depth bias, target
copies or HUD behavior are absent. Their state sets omit fields required by
a complete PSO key and are exploratory coverage, not a Metal parity result.

Use WS15's matched courtyard/roof/sky views: 1280×720, cg_fov=80, gamma=1,
ignoreHwGamma=0, fog=1, AA=1 and picmip=0 with gun/HUD hidden. Eye origins
are `(2569,2274,182)`, `(3051,2178,221)`, `(3051,2178,351)`, with the latter
pitched to 85 degrees. The final native-versus-Windows D9VK MAEs are about
2.005, 1.872 and 0.586 byte levels respectively (WS15 report, final followup).
Those establish the inherited reference quality, not a Metal acceptance
threshold. For GL-versus-Metal same-machine testing use stricter thresholds
and account explicitly for edge coverage/texture filtering differences.
Different backends order depth prepasses differently, so compare semantic
draw groups/resources rather than matching draw number blindly.

## Workstream boundaries implied by the audit

* **Resources:** preserve texture/surface subresource identity, formats,
  channel swizzles, partial mips, volume slices, upload visibility and COM
  lifetime; implement CPU locks over safe GPU-backed storage.
* **States/PSOs:** full engine state-word decode, depth/stencil defaults,
  separate-alpha factors, sampler key and faithful culling/coordinate
  convention; extend observed keys before claiming complete prewarming.
* **Draws/synchronization:** unchanged draw vtable slots, declaration routing,
  producer color order, base vertex/ring placement and frame completion;
  native backend dispatch for engine GL escape calls.
* **Shaders:** preserve ARB/CTAB reflection, register and state bindings;
  distinguish the 417 program payloads from 417 metadata payloads; keep all
  derived outputs private and prove GPU semantics against GL.
* **Targets/post/readback:** implement real target/DS binding, ordered clears,
  StretchRect copies/resolve, screenshot and cubemap readback; validate
  inherited GL omissions with isolated synthetic tests before full views.
* **Fixed function:** initialize/verify the currently zero stage-enum tables,
  implement DX7 FVF/matrices/lights/fog/combiner set, and test separately from
  both original ARB programs and the approximate fallback.

No shared source file is changed by this audit. The exact active native AST
results and trace summaries are reproduced locally using the commands above.
