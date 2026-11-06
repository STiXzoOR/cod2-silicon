# Mac/DirectX 9/CDirect3DPixelShader reconciliation

Updated 2026-11-05.

25 entries: 6 equal, 6 fixed, 13 intentional. Original Mac i386 bodies were read directly, including the coalesced getters. No entries remain unchecked.

ATI upload/constants were empty. The constructor at 0x1bad4/0x1bb9a saves binding, generates/uploads target 0x8200 in ASCII format and restores binding; 0x1b6c8 uploads consecutive environment vectors. Native now performs those operations. NV constants at 0x1b712 select global versus per-stage constants using stageCount<=2. Its constructor at 0x1bc62 now retains a pointer rather than truncating it to 32 bits. QueryInterface at 0x1b686 now invokes virtual AddRef.

Mac enable/disable/destruction uses COpenGL singleton fields at +0x818/+0x81c/+0x820. The indirect slot was independently resolved to COpenGL::sOpenGL. Current native draw providers also bind programs directly. The native branch therefore queries the current context's ARB/ATI binding and enabled capability instead of introducing a persistent GL cache. Disable preserves the program binding. NV queries only GL_TEXTURE_SHADER_NV and GL_REGISTER_COMBINERS_NV. Deletion and base cleanup are restored; matching native ownership is detached through actual dx.device without releasing it. Material_Shutdown precedes device release, and R_SafeRelease nulls that pointer.

These cache/ownership mappings are explicit native adaptations, not Mac state-layout equality. The selected pending device provider 9f05cde rejects ATI/NV creation; their SDK-visible methods are restored but unsupported backend activation is not claimed. Native C GL providers do not reproduce C++ exception unwinding. Pending System f8a3c70/5f88bd5 owns the ARB constructor binding/mask and destructor corrections; this unit preserves those hunks and requires that dependency at integration. Current GL remains selected; future Metal plans supplied no exemption.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.

## Integrated batch 16 verification

Engine 28e3856 and fixture-only follow-up ea23228 passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
