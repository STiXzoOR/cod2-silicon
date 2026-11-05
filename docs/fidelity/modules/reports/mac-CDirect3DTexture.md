# Mac/DirectX 9/CDirect3DTexture module reconciliation

Updated 2026-11-05.

**26/26 reconciled: 11 fixed, 15 equal.** Mac 1.3 i386 is the authority. Current OpenGL remains selected; future Metal work does not replace these bodies. Legacy OFF code is preserved.

## Corrections

Mac `AddDirtyRect` at 0x1da6e marks every child dirty. Native did nothing. The updater at 0x1d9e0 traverses actual children, uploads when dirty or recreating, forwards the recreate flag and clears dirty at the caller. Native instead used a lock-only bit mask and had no recreate argument. The reconstructed boolean entry is `CDirect3DTexture_UpdateOpenGLSurfacesNative`; the old one-argument entry delegates with false.

Mac level accessors at 0x1d988, 0x1dc16, 0x1dbce and 0x1da90 use exact error 0x8876086c and concrete child COM slots. `GetLevelDesc` acquires and releases the child around its descriptor call. Native returned -1, bypassed that lifetime and sometimes used the declared level count where the actual child count differs. QueryInterface/Release now follow the selected virtual AddRef/deleting-destructor slots.

The D0/D1/D2 bodies at 0x1dabe, 0x1e7d8 and 0x1e6d4 release each child through COM before freeing the vector and pixel backing. Native previously deleted children directly even when another native caller retained a reference. The parent then destroys the actual secondary COpenGLTexture subobject; only D0 frees the parent.

## Native layout and caller boundary

The selected device allocates 0x68+0x100 bytes for a native 2D texture. A compile-time size check proves the private native struct fits. The portable secondary prefix occupies two pointers, eight 32-bit sampling fields and three 32-bit dimensions; its final tail padding is reused by the flat derived layout. `lastLevel` precedes anisotropy, matching Mac, and the name pointer addresses owned native storage.

Native `Image_UploadData` enters COM lock/unlock slots 19/20 and writes the returned pointer before unlocking. The selected surface retains explicit eager native upload. The authorized device binding helper also saves the right target binding, regenerates zero names as Mac Sync 0x10740–0x107c3 does, passes the actual recreate boolean and dispatches all three texture types. No general device-state body or device ledger verdict was changed.

The two constructors are attributed to COpenGLTexture in the ledger and are reported there.

## Verification and limits

No new tests were added. Six existing texture/volume/depth fixture and runner files were updated to link real texture, surface, cube, COpenGL and vertex-array owners; generated ABI vtable import data remains isolated. Existing pixel, lifetime, dirty and binding assertions are unchanged. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. All five final production objects compile. Existing ASan/UBSan CGL fixtures now pass: `run_volume_upload.sh`, `run_texture_mips.sh` (requested levels 1, 2 and automatic full chain), and `run_depth_surface.py` (all 18 ARB/fallback scenarios). The isolated fixture runs used the actual read-only COpenGL worker production source through `COD2_OPENGL_PROVIDER_SOURCE`; the default runner links the integrated local provider. The 2D DXT5 upload and 2x2x2 uncompressed volume paths were exercised. Cube construction, Usage==1, compressed volume pitches, zero-name regeneration, standalone surface recreation and the full renderer remain unexercised.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The production-file generic guard comparison additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim. Native-only fixture plumbing is outside the legacy engine comparison.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. The actual sibling Register/Release/Shutdown source was read directly: pointer-identity registration/removal, no freeing of embedded names, cache invalidation and shutdown zeroing agree with this unit. This report does not claim whole-frame or populated-cache invalidation runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.
