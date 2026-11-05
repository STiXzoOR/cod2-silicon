# Mac/DirectX 9/CDirect3DCubeTexture module reconciliation

Updated 2026-11-05.

**28/28 reconciled: 7 fixed, 15 equal, 6 intentional.** Mac 1.3 i386 is the authority. Current OpenGL remains selected; future Metal work does not replace these bodies. Legacy OFF code is preserved.

## Corrections

The missing updater at 0x12504 now traverses six faces and every level, acquires/releases each child through COM, uploads dirty or recreated children, forwards the recreate flag and clears the dirty byte. Native had an empty body. `AddDirtyRect` at 0x124e2 marks every child, ignoring face and rectangle exactly as Mac does.

Both constructors, 0x129f8 and 0x12dd4, create full face-major chains. Every public child accessor now uses `face * levelCount + level`; native formerly interleaved six faces per mip. The first-face descriptor at 0x125aa now reads the correct child. Invalid mip HRESULTs are 0x8876086c. The native face bounds and descriptor-level bounds remain defined checks because no allocation proves Mac's unchecked table/index accesses safe; selected `Image_CubemapFace` callers supply faces 0 through 5.

The destructor family at 0x125d0, 0x127f0 and 0x128f4 releases children instead of directly deleting them. QueryInterface/Release use the selected virtual COM slots. The private portable prefix also restores max-level-before-anisotropy ordering.

## Native adaptations

The native device allocates 0x7c+0x100 bytes; compile-time checks prove the private object and borrowed secondary prefix fit. Surface children occupy proven 128-byte native allocations and their GL owner is the actual secondary subobject. Pixel backing is zero-filled and face-major. Every explicitly requested native child retains defined backing even if its mip count exceeds the natural chain, where Mac's size helper stops but its constructor continues creating children. The invented 16-level cap is removed.

Native synchronous GL upload and explicit truncated-chain maximum level remain. Mac's Apple client-storage/power-of-two upload behavior cannot be copied into these tightly packed allocations safely. The owned standalone COpenGLTexture helper bodies live in this file and have their own module report.

## Verification and limits

No tests were added or changed, as requested. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. These corrections are compiled and reviewed, not exercised with a GL context.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The generic guard checker additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. This report does not claim whole-frame or cache-provider runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.
