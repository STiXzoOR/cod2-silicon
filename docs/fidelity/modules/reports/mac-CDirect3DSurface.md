# Mac/DirectX 9/CDirect3DSurface module reconciliation

Updated 2026-11-05.

**25/25 reconciled: 4 fixed, 15 equal, 6 intentional.** Mac 1.3 i386 is the authority. Current OpenGL remains selected; future Metal work does not replace these bodies. Legacy OFF code is preserved.

## Corrections

This module was added to the unit to close the texture/surface ownership contract. Both constructors, 0x1d124 and 0x1d632, now keep an actual borrowed COpenGLTexture secondary prefix or allocate a standalone portable owner when absent and g_NoTextureID permits it. They restore format mapping, create gating and reference initialization. Children sharing a parent do not register or destroy another GL owner.

The destructor bodies at 0x1cd78 and 0x1cd9e call owned secondary deleting-destructor slot 1. Native previously treated the owner as primary IUnknown and called Release slot 2. Borrowed owners survive; native D0 frees the actual device malloc/child calloc allocation. The slot order was decoded from the Mac data and matched to selected native COM tables.

`IsDirty` at 0x1d32e includes a zero owner name. `UpdateOpenGLSurfaceObject` at 0x1d4f4 restores DXT1/2/3/5 cases, compressed width/height greater than 3, recreate dispatch and caller-owned dirty clearing. `RecreateSurface` at 0x1d356 now destroys/replaces standalone owners, preserves borrowed owners and remaps formats. It binds the newly created native name before allocating its GL image.

## Deliberate native boundaries

Create at 0x1cef4 uses synchronous native GL uploads with proven logical extents. Mac Apple client storage rounds ordinary dimensions up to powers of two; a 5x5 RGBA native mip aligned to 128 bytes does not prove a 256-byte 8x8 read safe. The native path retains actual extents, permits NULL GL allocation data and uses NULL for depth images. Device-owned standalone render/depth CPU backing is malloc storage, so initial and recreated GL images use NULL rather than introducing indeterminate reads. Zero-filled texture child backing is uploaded normally.

Mac UnlockRect at 0x1ceec is empty; native deliberately keeps eager full-image upload after Image_UploadData writes, with correct 2D/cube binding query and restore, stable standalone ownership and dirty clear. It does not regenerate an owner during ordinary unlock. Direct deferred updates retain Mac's small-compressed-level skip.

LockRect at 0x1ce30 matches pitch, compressed zero-width, rectangle arithmetic, ignored flags and dirty setting. Native retains its existing 4 MiB scratch fallback for NULL backing because the Mac returns NULL and no selected native allocator proves a later NULL write safe. This is a retained compatibility boundary, not evidence that arbitrary scratch-sized uploads are safe.

Native surface children are allocated as 128 bytes and fit the private struct by compile-time assertion. Standalone device allocations are larger. GetDesc's remaining fields are left to the caller exactly as in Mac; zero-return COM stubs leave output pointers untouched.

## Verification and limits

No tests were added or changed, as requested. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. These corrections are compiled and reviewed, not exercised with a GL context.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The generic guard checker additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. This report does not claim whole-frame or cache-provider runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.
