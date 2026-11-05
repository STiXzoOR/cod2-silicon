# gfx_d3d/r_image module reconciliation

Updated 2026-11-05.

**30/30 reconciled: 19 equal, 3 fixed, 8 intentional.**

The current GL renderer still uses this registry, texture creation, accounting and diagnostic code. Mac 1.3 is the authority. The native image hash table and image sizes remain widened for pointers; existing missing-device and incomplete-resource guards are preserved.

`R_ImageList_f` used a 67-byte cutoff before sorting a sixteen-element prefix. At Mac 0xe77c9 that means more than sixteen four-byte pointers; on LP64 the old expression selected the prefix for lists of 9–15 images and accessed beyond their end. The native caller now compares elements. Its format labels, heading, MB suffix and related-command footer now follow Mac 0xe787b–0xe7c2f. All program names, platform labels and image-track names match binary data.

`Image_CreateCubeTexture` now reports the effective mip count when cube mips are unavailable, as Mac 0xe6cd0/0xe6d44 does. `R_SetPicmip` restores complete diagnostics and the original unsigned low-memory thresholds. The WS11 native high-memory automatic policy remains selected for modern hardware; this is a specific current-renderer adaptation, not a Metal replacement.

A coordinated fix also changes the header-attributed GfxImage introsort partition at Mac 0x2f064d–0x2f068f: advance while `comp(*lo,pivot)`, then retreat while `comp(pivot,*hi)`. Negated, reversed predicates could run beyond equal elements. That helper row belongs to the system worker and is outside this module's thirty-row count. Its evidence is in `__TEXT,__textcoal_nt`; heap and insertion helpers were not changed.

The arm64 Release object compiles. The dedicated target links but excludes renderer sources. No existing focused image-sort fixture was found; imagelist, picmip selection and failure diagnostics were compiled and reviewed, not exercised. No new tests or fixture cases were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
