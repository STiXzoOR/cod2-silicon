# Mac/DirectX 9/CDirect3DPixelShader reconciliation

Updated 2026-11-05.

25 entries: 6 equal, 6 fixed, 13 intentional. Original Mac i386 bodies were read directly, including the coalesced getters. No entries remain unchecked.

ATI upload/constants were empty. The constructor at 0x1bad4/0x1bb9a saves binding, generates/uploads target 0x8200 in ASCII format and restores binding; 0x1b6c8 uploads consecutive environment vectors. Native now performs those operations. NV constants at 0x1b712 select global versus per-stage constants using stageCount<=2. Its constructor at 0x1bc62 now retains a pointer rather than truncating it to 32 bits. QueryInterface at 0x1b686 now invokes virtual AddRef.

Mac enable/disable/destruction uses COpenGL singleton fields at +0x818/+0x81c/+0x820. The indirect slot was independently resolved to COpenGL::sOpenGL. Current native draw providers also bind programs directly. The native branch therefore queries the current context's ARB/ATI binding and enabled capability instead of introducing a persistent GL cache. Disable preserves the program binding. NV queries only GL_TEXTURE_SHADER_NV and GL_REGISTER_COMBINERS_NV. Deletion and base cleanup are restored; matching native ownership is detached through actual dx.device without releasing it. Material_Shutdown precedes device release, and R_SafeRelease nulls that pointer.

These cache/ownership mappings are explicit native adaptations, not Mac state-layout equality. The selected pending device provider 9f05cde rejects ATI/NV creation; their SDK-visible methods are restored but unsupported backend activation is not claimed. Native C GL providers do not reproduce C++ exception unwinding. Pending System f8a3c70/5f88bd5 owns the ARB constructor binding/mask and destructor corrections; this unit preserves those hunks and requires that dependency at integration. Current GL remains selected; future Metal plans supplied no exemption.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.
