# WS6 — renderer LP64 runtime correctness

Status: static renderer fixes and standalone verification complete. Full-engine execution is blocked at platform/C++ linking; first launch and map loading have not been tested. Worktree `/Users/stix/Projects/cod2-native-wt/lp64-render`, branch `port/lp64-render`, baseline `a2f4477` (includes the orchestrator's arm64 guards, infoParms and seam aliases).

Read all of PLAN.md, the LP64 inventory, WS1 and WS2 reports. Worked only in this worktree. No sibling worktrees, pushes, PRs, issues, remotes, installed packages or global configuration changes. Reference binaries and licensed assets were read only; no binaries, game assets or decompiler dumps were added. No required tool was missing.

## What works, with verification commands

Run from the repository root:

```sh
cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCOD2_STABS_BINARY="$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
cmake --build build-macos -j12 -- -k
sh tests/lp64/renderer/run.sh
python3 tests/lp64/renderer/diagnostics.py --base a2f4477
python3 tests/lp64/renderer/legacy_guards.py --base a2f4477
git diff --check
```

Results:

- Configure succeeds. All 797 compile-database object entries exist and have Mach-O 64-bit magic and arm64 CPU type. The keep-going build exits 2 at the two link attempts, with zero compilation errors. Final build output is locally retained in ignored `build-macos/ws6-final-objects.log`.
- Seven standalone arm64 checks pass under AddressSanitizer and UndefinedBehaviorSanitizer. They exercise the implementation C files, using synthetic assets and GL/filesystem/runtime mocks: buffer locks and delayed frees; texture-unit/VAO/map-node storage; material offsets and water conversion; font insertion/relocation and short reads; mixed render command sizes/alignment; recursive static-model cache free; shader argument union/routing and Dx7 tables. They do not create a GL context or substitute for engine execution.
- The diagnostic script independently replays the actual 170 owned client/dedicated CMake compile entries for each version. It exports only tracked source/header files from the baseline into ignored build storage, avoiding parallel diagnostic interleaving. Both versions produce zero compile errors. Deduplication and LP64 flag categories match `tools/macos-port/lp64_inventory.py`.
- 180 inactive source/header-body comparisons across five COD2_X64-off configurations have zero mismatches. All new native headers preprocess to nothing with the port off. This checks guard discipline, including every changed shared declaration; it is not an i386 executable/object byte comparison. A full Linux/Windows i386 build was not run on this Mac.
- `git diff --check` passes. The shader-argument test compilation has one existing SDK sprintf deprecation warning; there are no sanitizer failures.

## Warnings before and after

Counts below are unique location/flag/message diagnostics within the owned directories, deduplicated across client/dedicated. The starting branch has 168 renderer LP64 warnings, one fewer than the older WS1 inventory.

| Directory | All warnings before | All warnings after | LP64 before | LP64 after |
| --- | ---: | ---: | ---: | ---: |
| `src/PC/gfx_d3d` | 196 | 110 | 168 | 29 |
| `src/Mac/DirectX_9` | 250 | 279 | 138 | 57 |
| `src/Mac/DirectX 9` | 0 | 0 | 0 | 0 |

Renderer LP64 categories change from 34 pointer-to-integer, 25 integer-to-pointer, 107 narrowing and 2 constant-overflow warnings to 27 narrowing and 2 integer-to-pointer warnings. The latter two are in the untouched `R_RecoverLostDevice` path.

D3D LP64 categories change from 58 pointer-type mismatches, 75 narrowing, 2 format and 3 pointer-to-integer warnings to 52 pointer-type mismatches, 4 narrowing and 1 pointer-to-integer warning. The surviving pointer-to-integer cast is the swap-chain constructor, left for WS3. Most mismatches are reconstructed `fnptr_t[]` vtables stored as `void **`, with full-width pointer storage. No warning-only casts were added to hide them. Overall D3D warnings increase because correct SDK GL declarations expose deprecated legacy GL calls; the ABI warnings decrease.

Reproducible per-TU logs and before/after TSVs are under ignored `build-macos/ws6-probes-{before,after}` and `build-macos/ws6-diagnostics-{before,after}.tsv`. The shared inventory and its committed logs were not modified.

## Fixes by category

### GL signatures, widths and resources

- Apple LP64 uses SDK GL typedefs and prototypes, including GLsizeiptr/GLintptr, rather than reconstructed long-sized GLenum/GLint/GLuint and unprototyped `int gl...()` declarations. GL integer/enumerant scalars are four bytes; pointer-sized buffer lengths/offsets are eight. Existing buffer sizes and attribute/index pointer arguments now pass through real prototypes.
- DWORD, LONG/HRESULT and ULONG retain their Windows/D3D 32-bit semantics on Apple LP64. This also prevents vertex-color and shader-token layouts from widening, and restores negative HRESULT error values.
- Vertex/index constructors and draw consumers share typed native buffer structs. Removed the duplicated data-pointer offset formula and allocate `sizeof` the constructor's actual type.
- Texture and cube-texture GL-name getters use their actual implementation structs. Device/surface consumers and cube secondary-base adjustments use these getters and `sizeof(void *)`. Surface locks retain the full pixel pointer instead of classifying it through a truncated address.
- Memory-buffer virtual pointers are native function-pointer addresses. Its delayed-free sentinel is a full Node, replacing a four-byte integer used as linked-list storage. Tests verify high pointers, 32-byte allocation alignment and deferred frees.
- Vertex-array vtable address points skip two pointer slots, rather than eight bytes. Native C++ array-state flags are bytes; the engine's C `bool` typedef is an int and was unsuitable here.
- Native GL live state uses a private typed object. Texture units follow the verified CTexUnit field order; combiner caches use member-derived offsets and GLfloat scales. All owned native users share the canonical GL object instead of writing a VAO list at `+0x674` into the four-byte `COpenGL_sOpenGLE` stub.
- Generic VAO packet/tree storage is properly sized, map key/value temporaries are aligned, node values use `.second`, and the native tree count is size_t. The current selector wraps at one, so the generic packet has one full native slot. The old C++ tree operations themselves still require WS3's runtime.
- The native D3DXVec4Transform signature/return preserves the output pointer. No current repository caller uses this reconstructed entry point.

### Serialized assets and pointer fix-ups

The only owned binary loaders found directly treating a disk pointer-bearing image as a native object are material and font loading. Both native paths explicitly marshal fields. The old in-place material relocator is compiled only for the legacy path; it is not a second native loader.

| Loader/data | Disk representation | Native treatment |
| --- | --- | --- |
| `Material_Load`, `.material` | MaterialInfo 44, Material 68, texture entry 12, constant entry 20, water definition 32 bytes; pointers are four-byte blob offsets | Dedicated fixed-width disk structs, memcpy to aligned local records, then field-by-field native construction. Validate header/tables/strings/short reads; never overwrite file slots with native pointers. Constants and texture arrays use native sizes. |
| Water texture entry, semantic 5 | Offset to MaterialWaterDef32, including an obsolete four-byte map slot | Copy physical parameters to native MaterialWaterDef and water_t, set gravity to the original 800, call existing R_LoadWaterSetup, preserve its full pointer. Require square power-of-two dimensions up to 128 so WaterGlob.H's 16384 elements cannot overflow. Previously the upstream native marshaler dropped water to NULL. |
| `R_LoadFont`, fonts | 16-byte disk header, pointer-free 24-byte Glyph records, string offsets relative to the old expanded image | Preserve the legacy four-byte gap inserted after the header; explicitly copy scalar/offset header fields into native Font and assign native glyph/material pointers. Validate counts, strings, integer overflow and both reads. |
| `Image_LoadFromFile`, `.iwi` | Pointer-free 28-byte GfxImageFileHeader, encoded image/mip payload | Keep the disk header; reject short headers. Set native `image->noPicmip`, replacing a byte write into the widened texture pointer. Mip payload bounds still need runtime/malformed-file coverage. |
| `R_LoadLightDef`, lights | Type/sampler bytes and two NUL image-name strings | Already constructs native GfxLightDef field by field; now bounds both strings and the second sampler before registering images. |
| Technique sets/passes, statemaps, shader sources | Text; shader CTAB uses fixed-width offsets, not native pointers | Native Dx9/Dx7 temporary pass arrays and pass allocation offsets use real types. Samplers, stage bits, flags and pass validation use members. Shader routing has native pointer storage; code-constant writes target the union at its native offset. CTAB constant count is bounded to the scratch array. |
| `R_LoadWorldInternal`, BSP | Fixed-width, pointer-free disk lumps | Existing field-by-field world construction retained. Fix occluder pointer-array allocation, native occluder stride and full rgl resets. Sun-light output uses native members after its leading pointer. |
| `XModelReadSurface` / alternate `R_LoadXModelSurfsSurface` | Scalar stream and vertex/index payloads | Main loader already allocates sizeof(XSurface) and copies fields. Alternate loader now does likewise, updates typed XModel.memUsage and allocates full-width material pointer arrays. No raw disk XSurface is read into native storage. |

Shader-preload globals now use typed count/pointer members instead of the count-at-four assumption. Reconstructed zero static Dx7 function/option tables were restored to verified names/enumerants and native offsetof values. The Mac reference symbols are `_s_passOptionsDx7` at 0x36bbe0 and `_s_textureFuncsDx7` at 0x36bec0; only the small operational C tables were reconstructed, with no binary dump added.

Read-only licensed-data spot checks verified a water definition with width 64 and a font with 96 glyphs, pixel height 24, and string offsets 2320/2337 in a 2352-byte file. No licensed bytes are used in committed tests.

### Commands, world/cache and startup consumers

- Native command allocation rounds every command to eight-byte alignment, including header-only commands. Light properties use 80 bytes instead of 76; begin-view uses 64 instead of 48 because both viewParms and sceneDef contain pointers. Tests mix small commands with pointer commands and verify traversal and alignment.
- Native backend command storage is a real GfxBackEndData, replacing the generated raw ILP32 fallback whose size cannot grow with its pointer-bearing fields. No generated source or shared BSS definition was edited.
- Backend light commands retain the full light-definition pointer; sprite commands use a full GfxEntity. Backend-global initialization uses sizeof instead of a memset that overflowed the actual native object. Tessellation view pointers and the debug-light log pointer retain their full width.
- Static-model cache leaves use sizeof their union instead of 16 bytes. Recursive frees use typed tree nodes, leaves, linked-list members, cached LOD slots and statistics. Tests cover both used and free branches on high addresses. Transform/lighting accesses use native instance members.
- Material/image identity ordering uses uintptr_t comparisons rather than truncated pointer subtraction. Picmip/reload loops use native material hash/texture arrays. R_GetMaterialName accepts the actual pointer handle on the native path.
- Renderer resource cleanup follows DxGlobals buffer/query/window members; gamma/GPU-sync settings follow dvar members; world shutdown clears the full pointer; sun-query flags use SunFlareDynamic members. Context creation, display/reset parameter code, present/swap and AGL operations were not changed.

## Layout evidence and decisions

Used the STABS reader in `tools/datagen/stabs.py` against the user-supplied i386 Mac reference, plus repository type declarations. Verified field order/ILP32 sizes include CTexUnit 304 (combiner at 112, texcoord array at 260), CBaseVA 24 (stream at 20), material disk records above, and pointer-bearing render commands. Native assertions include CTexUnit 336, CBaseVA 32/stream24, map value/node offsets 8/40, shader argument 16/union8, Dx7 pass112, technique passArray16, static-model leaf24/tree528/leafs144, GL scalar and D3D scalar widths.

The native COpenGL private object contains only the live implementation's cached fields; it does not claim ABI compatibility with the entire original C++ class. Audited all owned callers: they go through the canonical opaque pointer or the native list accessor. Several other COpenGL operations are already reconstructed no-op implementations, so their rendering behavior is an integration limitation, not something this layout work validates.

Preserved original expressions under guards even where the old code appears wrong. No generalized cleanup/reformatting was attempted because legacy byte preservation and sibling merge isolation take priority. Performed a targeted manual review of the owned diff, reran tests/build after corrections, and kept remaining findings in this report; no external review/ticket publication was authorized.

## What does not work yet / first-launch watch list

1. Both executables remain unlinked. Current unresolved symbols include old-ABI std::string/COW, `_Rb_tree`, `_List_node_base`, C++ runtime seams, MacDisplay_CreateScreenContext, SDL_GL_SwapWindowDirect, input and Win32 crash/debug compatibility calls. WS3 must resolve these before engine execution. There are no duplicate-symbol errors.
2. Device loss/recovery and display/swap paths retain old raw DxGlobals offsets, dvar round trips and the swap-chain constructor's integer return. They are WS3-owned. Test window changes/fullscreen/device recovery after the platform merge; fixing resource cleanup does not validate reset parameter packing.
3. VAO cache linkage/storage is native-sized, but calls into WS3's old C++ container/runtime shims have not run. Reconstructed cache insertion/erase wrappers and existing GL no-op operations need integration review; a green storage test does not prove a functional cache or rendering.
4. First map load needs the real shader compiler/constant-table, material/technique parser, texture uploads and water FFT together. The synthetic marshaling/parser checks verify offsets and pointer widths; they do not establish shader correctness, water visuals or all statemap semantics. Existing statemap rule-header casts and reconstructed D3DX/shader behavior deserve attention if pass validation or initial shader creation fails.
5. IWI wavelet/mip decoding still assumes valid compressed payload lengths beyond the checked header. BSP entity strings and some lump-derived indices also need malformed-input coverage; these formats contain fixed-width offsets, not widened pointer slots. Watch truncated/invalid assets rather than assuming an LP64 issue at every numeric offset.
6. Vtable pointer-type warnings remain. Actual arm64 object storage is pointer-wide, but reconstructed virtual signatures and especially functions returning integer/pointer values need real runtime validation. Remaining string/size narrowing diagnostics were left visible, not suppressed with casts.

No launcher, connection, timedemo, benchmark or parity result is claimed. The full i386 binary parity check also remains for the orchestrator's Linux/Windows harness.

## Merge notes

Local focused implementation commits, in order:

- `d057c57` — GL declarations, native buffers, memory-buffer sentinel/vtables, first ABI check.
- `d5b5841` — fixed-width D3D scalars and typed texture/surface GL-name consumers.
- `00be612` — native GL/texture-unit/VAO cache storage and pointer-return math seam.
- `e450e49` — asset marshaling, bounded font/light/header reads, technique/argument layout consumers.
- `6e52a96` — aligned/sized render commands and backend storage/consumers.
- `cfd2cde` — world/static-model cache and renderer resource member access.
- `cd6ca04` — direct shader-routing regression check and typed traversal of trailing Dx7 pass storage.

Shared hunks are limited to `src/headers/cod2_defs.h` (Apple SDK GL types; Apple DWORD/LONG), `src/headers/Mac/mac_types.h` (Apple ULONG), `src/imports/opengl.h` (Apple SDK GL prototypes), and `src/headers/PC/gfx_d3d/gfx_funcs.h` (guarded native material-handle signature). CMake, platform files, stubs and generated blob outputs are unchanged.

All other implementation changes are in the owned renderer/D3D directories. Merge the complete branch rather than omitting its local native headers. WS3 should keep the SDK GL types/prototypes and D3D 32-bit scalar semantics, and should not restore fixed-offset access to the private native GL/VAO or backend command storage. The native private objects deliberately bypass undersized legacy stub/raw-BSS globals; those old symbols can still exist for legacy compatibility.
