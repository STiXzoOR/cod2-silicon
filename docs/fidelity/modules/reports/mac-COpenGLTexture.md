# Mac/DirectX 9/COpenGLTexture module reconciliation

Updated 2026-11-05.

**5/5 reconciled: 4 intentional, 1 fixed.** Mac 1.3 i386 is the authority. Current OpenGL remains selected; future Metal work does not replace these bodies. Legacy OFF code is preserved.

## Corrections and native ownership

The ledger places the two 2D texture constructors, 0x1dc4c and 0x1e190, in this module although their production owner is CDirect3DTexture.c. They now preserve 32-byte aligned ordinary mip offsets, automatic/requested level counts, Usage==1 rounded backing with exactly one child, actual child count, portable secondary ownership and reference initialization. Native retains defined backing for every requested child even beyond Mac's natural-chain sizing stop; it does not introduce a read beyond the native allocation merely to reproduce that case.

The portable secondary prefix is `vtable; GLuint *name; wrapS,T,R; border; minFilter,magFilter; GLint lastLevel; GLfloat anisotropy; GLint width,height,depth`. There is no extra reserved scalar. Mac initializes wraps to 0x2901, min/mag to 0x2702/0x2601, last level to 1000 and anisotropy to 1. Native's previous last-level/anisotropy positions were swapped. A flat derived object reuses the prefix tail padding; compile-time extent assertions pin that contract.

The base no-argument virtual update at 0x2ed54e is empty. Native incorrectly subtracted a parent offset and ran the 2D updater, including when invoked on a cube. It is now empty. The actual destructor bodies at 0x2ed600/0x2ed632 are reconstructed in CDirect3DCubeTexture.c, along with the standalone portable allocation used by Surface. Derived secondary destructors adjust by one native pointer, not a four-byte Mac offset.

Mac owns separately allocated GLuint values registered in COpenGL and ReleaseTextureBinding 0x21ba6 deletes them immediately. Native names are embedded in stable owning allocations. The coordinated native provider registers those pointers without owning/freeing them, invalidates caches before deletion, deletes/zeros names, and unregisters even zero names before owner destruction. D0 frees only the actual standalone native prefix allocation. The current inert provider retains direct-delete fallback. This is a concrete native storage adaptation, not a claim that GL is superseded.

The owned device helper follows Mac Sync 0x10740–0x107c3 for zero names: generate/register the stable name, restore sampling defaults and pass recreate=true through the derived updater. Native explicit GL_TEXTURE_MAX_LEVEL is retained for truncated chains. Native synchronous logical extents avoid Mac client-storage expanded reads outside proven backing. All scalar storage is initialized rather than importing indeterminate i386 bytes.

## Verification and limits

No tests were added or changed, as requested. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. These corrections are compiled and reviewed, not exercised with a GL context.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The generic guard checker additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. This report does not claim whole-frame or cache-provider runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.
