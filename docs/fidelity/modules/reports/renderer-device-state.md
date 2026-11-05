# Renderer device-state follow-up

Updated 2026-11-05.

13/14 assigned entries reconciled: 5 fixed, 8 intentional native adaptations, 1 unchecked. Mac 1.3 exact entries and the coalesced texture-stage initializer are the authority. The legacy source and actual OFF line expansions are unchanged.

The central correction is the state owner. Native device stages now have widened pointer storage, independent matrices, the original sampler/combiner fields and initialized lifetimes. Material and light caches occupy the existing 68/1440-byte device regions. DeviceImpl is 3480 bytes, within the existing 4096-byte static allocation. The stage vector owns 17 typed records, including bounded defined storage for the original getter's inclusive limit. Creation, reinitialization and destruction were audited; imported four-byte CTexStage placeholders and ILP32 offsets are never used as native storage.

The earlier evidence misidentified COpenGL+0x800. Exact Init 0x21c50 loads string 0x2410ec, `GL_ATI_array_rev_comps_in_4_bytes`, and stores its result at 0x21c5f. Its existing native field, supportsATIArrayRevComps, remains authoritative. Shader selection never changes that capability. Material 0x145c6, LightEnable 0x146ec, SetLight 0x1580e and lighting validation 0xe784 now use that actual contract. COpenGL_SetLight 0x20fcc and its Init self-comparison remain unchanged.

The restored light conversion preserves the original float operation order and constants, including the spotlight exponent, zero-falloff3.14 angle, tiny-angle cutoff and ATI cosine branch. Lighting validation restores color-material selection, white color in the original branch and enabled-light direction transforms. Native bounds reject invalid light indices and singular inverses retain prior driver light state rather than reading undefined storage. The narrow LIGHTING/COLORVERTEX cache branches, their original defaults and declaration/FVF diffuse-color tracking were coordinated with the parent. They are partial prerequisite corrections; this report does not close the separate SetRenderState row.

Transform 0xedd6 now keeps raw and converted view matrices, immediately converts/loads changed projection matrices and stores each texture-stage matrix. Modelview validation 0xecd2 checks/clears the original dirty contract and multiplies world by converted view before updating the native GL cache. The native direct GL adapter maintains matrix-mode/modelview, active-unit, target-enable and binding caches for all its direct state calls.

Stage usage 0x15590 and Init 0x2edd06 were reconstructed from the original bodies and decoded operation tables. The original alpha-op24 factor-selector quirk is retained. Sampler get/set 0x15ba6/0x15ac4 restore quarter-scale LOD, float-bit anisotropy, truncating getters, supported-case limits and invalidation. Stage get/set 0x15d84/0x15ca8 restore supported fields and the asymmetric inclusive getter bound. Native adaptations replace undefined unsupported-setter comparisons, uninitialized usage results and out-of-vector reads with bounded initialized state; unsupported getter outputs remain unchanged.

Texture synchronization 0x100ce now has a real device caller and restores per-target wrap/border/filter/LOD/mip/aniso transitions and unit/object caches. It preserves the original mirror-address-to-zero behavior and min/mip mapping. Existing typed dirty uploads and embedded GLuint ownership replace the original separately allocated name/list backend. A fallback lightmap substitution stays bound instead of being overwritten by the selected D3D texture. The shader helper's only change passes its existing device to sampler validation.

The selected texture prerequisite is e692f10, not the older local texture providers. Compiler layout inspection of all three exact owners verifies secondary-base offset8, name16, last-level48, anisotropy52, dimensions56/60/64 and refcount68. The shared prefix has 60 bytes of fields, rounded to64; its tail padding is reused by the flat owner's refcount and is never bulk-copied. Texture/Cube/Volume extents are120/112/120 bytes. COpenGL's existing private prefix is checked against the shared declaration. These prerequisite source files were not edited or committed here.

The existing native Reset retains its context and caches while resizing the window. The new native fields follow that retained lifetime; Init reinitialization replaces the stage allocation and clears material/light state. This work does not claim parity for the separately unassigned Reset method.

## Remaining validator

ValidateRasterization 0x10ff2–0x12094 remains unchecked. The original packet/declaration stream fields, SetTextureCoordStream, generated-coordinate modes 0x10000/0x20000/0x30000, full combiner dispatch, fixed-stage termination/disable ordering and pixel-shader texture-mask orchestration are not established by the native draw approximation. The new typed stage and synchronization contracts remove the earlier placeholder blockers but do not settle those remaining callers. No VARange integer was cast to a pointer. Metal does not retire this original engine-side validator.

## Verification and limits

A dedicated arm64 Release CMake build compiled the changed Device and COpenGL objects with `taskpolicy -b nice -n19` and at most two jobs. Native material/light/prefix/extent assertions pass. Twenty inactive guard comparisons pass, also with actual __LINE__ expansion. Constants/triage helpers ran; exact-entry hand reading supersedes their symbol-name and coalesced-body limitations.

A private source overlay records hashes for the actual selected texture, GL, buffer, shader, array and color prerequisites. Existing texture_stage_enums, code_samplers and shader_raster fixtures pass with ASan/UBSan. Existing gl_state passes after private static-initializer namespace and teardown-unlink plumbing; its original assertions are unchanged. All 18 existing depth_surface cases pass in ARB/fallback modes with real D3DXMath/MacOpenGLMath/COpenGL providers, replacing its stale unexpected-multiply stub and supplying typed validation globals plus abort-only native texture aliases. No repository fixture or assertion was changed, and no new cases were added.

Those fixtures exercise device/stage initialization, identity modelview validation, the no-enabled-light validation branches, existing raster/depth behavior and caller enum routing. They do not exercise material/light mutations, spotlight conversion, direct sampler/stage setter bodies, nonidentity transforms or the new texture synchronization path. Those changes are compiled and reviewed only. No whole combined client link, full CI, game replay or gameplay validation was run; local non-owned texture providers are older than the required integration prerequisites.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-device-state.noindex/`.
