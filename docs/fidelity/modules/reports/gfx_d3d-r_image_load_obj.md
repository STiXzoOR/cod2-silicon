# gfx_d3d/r_image_load_obj module reconciliation

Updated 2026-11-05.

**16/16 reconciled: 9 equal, 4 fixed, 3 intentional.**

All sixteen rows were compared with Mac symbol bodies; all fourteen format-dispatch table entries at 0x32b7a0 were decoded. The generator matrix at 0x32b7e0 and six face-axis triples at 0x32b820 match the native tables exactly. The current GL engine loaders remain active.

`Image_BuildWaterMap` now reads native `dvar.current.integer`; the legacy +8 access at Mac 0xfbf20 points to metadata in LP64. `Image_GetWaterColorForVector` now passes normalized Z to FresnelTerm, as Mac 0xfc87c does, rather than Y. Light-grid generation now follows the sign mask and multiplication grouping at Mac 0xfc918 and 0xfc9d9–0xfca0b.

`Image_LoadWavelet` now determines cube faces after Image_Setup, matching 0xfc37a before 0xfc3c8. Previously a newly allocated cube was treated as one face. Native temporary upload allocation now covers the full copied mip size. Mac rounds that request down at 0xfc458 and copies the unrounded size at 0xfc4b6; recreating out-of-bounds one- or two-byte allocations would not be a valid native storage contract.

Existing native BGR/BGRA expansion, un-swapped little-endian wavelet uploads, exact-name comparisons, bounded IWI headers and six-byte wavelet lookahead remain intentional adaptations to current GL and allocator contracts. The JPEG caller still releases decoded pixels with Hunk_FreeTempMemory; the corresponding allocator was corrected in r_jpeg.

The arm64 Release object compiles. Existing ASan/UBSan `bitmap_channels`, `lighting_lookup` and `wavelet_loading` fixtures pass. Lighting checks compare all lightmap bytes and exercise light-grid normalization and distinct generated cube faces. The wavelet fixture substitutes the codec and tests one BGRA face and lookahead padding, so six-face loading and tiny L8/A8 allocation changes were compiled and reviewed, not exercised. Water-map recreation, the Fresnel callback and raw JPEG image loading were not exercised. No new tests or cases were added.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
