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

No new tests were added. Six existing texture/volume/depth fixture and runner files were updated to link real texture, surface, cube, COpenGL and vertex-array owners; generated ABI vtable import data remains isolated. Existing pixel, lifetime, dirty and binding assertions are unchanged. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. All five final production objects compile. Existing ASan/UBSan CGL fixtures now pass: `run_volume_upload.sh`, `run_texture_mips.sh` (requested levels 1, 2 and automatic full chain), and `run_depth_surface.py` (all 18 ARB/fallback scenarios). The isolated fixture runs used the actual read-only COpenGL worker production source through `COD2_OPENGL_PROVIDER_SOURCE`; the default runner links the integrated local provider. The 2D DXT5 upload and 2x2x2 uncompressed volume paths were exercised. Cube construction, Usage==1, compressed volume pitches, zero-name regeneration, standalone surface recreation and the full renderer remain unexercised.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The production-file generic guard comparison additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim. Native-only fixture plumbing is outside the legacy engine comparison.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. The actual sibling Register/Release/Shutdown source was read directly: pointer-identity registration/removal, no freeing of embedded names, cache invalidation and shutdown zeroing agree with this unit. This report does not claim whole-frame or populated-cache invalidation runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.

## Integrated batch 15 verification

Engine bcafce5 and fixture-only follow-ups ae7b674, 5642801 and 4e73a89 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
