# Mac/DirectX 9/CDirect3DVolumeTexture module reconciliation

Updated 2026-11-05.

**44/44 reconciled: 18 fixed, 21 equal, 5 intentional.** Mac 1.3 i386 is the authority. Current OpenGL remains selected; future Metal work does not replace these bodies. Legacy OFF code is preserved.

## Corrections

Mac `CDirect3DVolume::Release` at 0x11766e invokes deleting destructor slot 12 at zero. Native only decremented. Both volume destructors now reset the proper vtable and D0 frees the actual calloc allocation. Texture D0/D1/D2 at 0x117a8e, 0x11870e and 0x11860a release every child through COM before freeing vector/pixel backing and destroying the portable secondary base.

`GetLevelDesc` at 0x117716 now acquires the child through `GetVolumeLevel`, dispatches its descriptor call and releases it. Other level accessors use exact 0x8876086c failure and child COM dispatch. `AddDirtyBox` at 0x11780c marks every child. `IsDirty` at 0x117a66 includes a zero owner GL name. The updater at 0x117d82 implements dirty-or-recreate traversal with caller-owned dirty clearing.

`Volume::GetDesc` at 0x117696 only writes format, width, height and depth; native previously overwrote Type, Usage and Pool as well. `UpdateOpenGLSurfaceObject` at 0x117c20 restores format remapping on recreate, DXT1/2/3/5 dispatch, compressed width/height/depth greater than 3 and no callee dirty clear. The format's DXT4 path is not silently added.

The constructors at 0x117e0a and 0x11820a now assign real child vtables, portable secondary owners, per-volume format/create state, child references, full mip counts and contiguous backing. Every explicitly allocated native child retains defined backing even beyond the natural image-size chain. The selected device's 0x6c+0x100 allocation fits the private struct by compile-time assertion.

## Deliberate storage and GL differences

Mac compressed LockBox at 0x11776e returns `SlicePitch = RowPitch * texelHeight`. Native `Image_UploadData` writes each depth slice at that returned stride into tightly packed image backing. An 8x8x4 DXT1 image allocates 128 bytes; the Mac stride would be 128 bytes and overrun from the second slice. Native retains the block-row slice pitch. No allocator/layout proof permits copying that overwrite.

Mac constructors query 0x806f with `glGetIntegerv` and later restore an uninitialized local; native retains the defined binding query 0x806a. Native synchronous logical-extent uploads replace Apple client storage/power-of-two reads, and depth images pass NULL. Native unlock deliberately uploads full levels immediately, preserving current Image_UploadData behavior and populating small compressed mips. Explicit deferred updates retain Mac's compressed-size skip.

## Verification and limits

No tests were added or changed, as requested. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. These corrections are compiled and reviewed, not exercised with a GL context.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The generic guard checker additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. This report does not claim whole-frame or cache-provider runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.
