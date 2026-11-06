# Mac/DirectX 9/CDirect3DVolumeTexture module reconciliation

Updated 2026-11-05.

**44/44 reconciled: 16 fixed, 23 equal, 5 intentional.** Mac 1.3 i386 is the authority. Current OpenGL remains selected; future Metal work does not replace these bodies. Legacy OFF code is preserved.

## Corrections

Mac `CDirect3DVolume::Release` at 0x11766e invokes deleting destructor slot 12 at zero. Native only decremented. Both volume destructors now reset the proper vtable and D0 frees the actual calloc allocation. Texture D0/D1/D2 at 0x117a8e, 0x11870e and 0x11860a release every child through COM before freeing vector/pixel backing and destroying the portable secondary base.

`GetLevelDesc` at 0x117716 now acquires the child through `GetVolumeLevel`, dispatches its descriptor call and releases it. Other level accessors use exact 0x8876086c failure and child COM dispatch. `AddDirtyBox` at 0x11780c marks every child. `IsDirty` at 0x117a66 includes a zero owner GL name. The updater at 0x117d82 implements dirty-or-recreate traversal with caller-owned dirty clearing.

`Volume::GetDesc` at 0x117696 only writes format, width, height and depth; native previously overwrote Type, Usage and Pool as well. `UpdateOpenGLSurfaceObject` at 0x117c20 restores format remapping on recreate, DXT1/2/3/5 dispatch, compressed width/height/depth greater than 3 and no callee dirty clear. The format's DXT4 path is not silently added.

The constructors at 0x117e0a and 0x11820a now assign real child vtables, portable secondary owners, per-volume format/create state, child references, full mip counts and contiguous backing. Every explicitly allocated native child retains defined backing even beyond the natural image-size chain. The selected device's 0x6c+0x100 allocation fits the private struct by compile-time assertion.

## Deliberate storage and GL differences

Mac compressed LockBox at 0x11776e returns `SlicePitch = RowPitch * texelHeight`. Native `Image_UploadData` writes each depth slice at that returned stride into tightly packed image backing. An 8x8x4 DXT1 image allocates 128 bytes; the Mac stride would be 128 bytes and overrun from the second slice. Native retains the block-row slice pitch. No allocator/layout proof permits copying that overwrite.

Mac constructors query 0x806f with `glGetIntegerv` and later restore an uninitialized local; native retains the defined binding query 0x806a. Native synchronous logical-extent uploads replace Apple client storage/power-of-two reads, and depth images pass NULL. The native texture-level UnlockBox wrapper deliberately uploads full levels immediately, preserving current Image_UploadData behavior and populating small compressed mips. Direct Volume::UnlockBox remains the Mac no-op. Explicit deferred updates retain Mac's compressed-size skip.

## Verification and limits

No new tests were added. Six existing texture/volume/depth fixture and runner files were updated to link real texture, surface, cube, COpenGL and vertex-array owners; generated ABI vtable import data remains isolated. Existing pixel, lifetime, dirty and binding assertions are unchanged. The arm64 Release `cod2_macos_ded` target builds with `taskpolicy -b nice -n 19` and two jobs. That target excludes renderer sources, so the actual Apple SDK client compile commands were also run separately for `CDirect3DTexture.c`, `CDirect3DCubeTexture.c`, `CDirect3DVolumeTexture.c`, `CDirect3DSurface.c` and the authorized `CDirect3DDevice.c` helper edit. All five final production objects compile. Existing ASan/UBSan CGL fixtures now pass: `run_volume_upload.sh`, `run_texture_mips.sh` (requested levels 1, 2 and automatic full chain), and `run_depth_surface.py` (all 18 ARB/fallback scenarios). The isolated fixture runs used the actual read-only COpenGL worker production source through `COD2_OPENGL_PROVIDER_SOURCE`; the default runner links the integrated local provider. The 2D DXT5 upload and 2x2x2 uncompressed volume paths were exercised. Cube construction, Usage==1, compressed volume pitches, zero-name regeneration, standalone surface recreation and the full renderer remain unexercised.

Exact-symbol `otool -arch i386 -tV -p` output was read for ordinary text; executable coalesced entries were read from `__TEXT,__textcoal_nt`. The heuristic triage and constant tools were run, including explicit address ranges. Their generic C++ name lookup and ordinary-text-only instruction view do not cover every coalesced body, so the hand comparison supplies the verdicts. Actual Mac COM vtable entries were decoded from the binary data.

All five `COD2_X64=OFF` preprocessing configurations match the base, including actual `__LINE__` and `COD2_ASSERT_FIELD` expansions. There are no `__LINE__` uses in the changed bodies. The production-file generic guard comparison additionally flags the five deliberately active Linux X64 configurations, which are outside this OFF equivalence claim. Native-only fixture plumbing is outside the legacy engine comparison.

The new `COpenGL_RegisterTextureBinding` dependency and the cache-aware no-free `COpenGL_ReleaseTextureBinding` provider must be integrated from the COpenGL reconciliation before the combined client link/runtime gate. The original inert provider still has a direct-delete fallback in base destruction. The actual sibling Register/Release/Shutdown source was read directly: pointer-identity registration/removal, no freeing of embedded names, cache invalidation and shutdown zeroing agree with this unit. This report does not claim whole-frame or populated-cache invalidation runtime validation. No function identity is unresolved and no row remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-textures.noindex/`.

## Unmatched secondary deleting destructor follow-up

Mac coalesced thunk 0x2f1830 subtracts four bytes from its secondary-base pointer and jumps to the primary volume-texture deleting destructor. The restored native secondary vtable now installs CDirect3DVolumeTexture_SecondaryD0 in slot 1; it subtracts one native pointer, matching the adjacent primary/secondary vtable fields, then calls the actual D0. Constructor installation and D1/D0 teardown were read together. This also closes the separately catalogued unmatched row as fixed; the 44 module-row counts above are unchanged. Previously the secondary field remained NULL and there was no corresponding native body. The source is unchanged from the reviewed and compiled texture provider. This secondary dispatch itself remains compiled and reviewed, not runtime exercised.

## Integrated batch 15 verification

Engine c8ef702 and fixture-only follow-ups 1581779, 2c6cf4f and 7f49612 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
