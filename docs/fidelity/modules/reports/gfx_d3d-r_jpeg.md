# gfx_d3d/r_jpeg module reconciliation

Updated 2026-11-05.

**9/9 reconciled: 1 fixed, 8 intentional.**

The active Apple build selects the engine integration in r_jpeg.c and the ImageIO implementation in src/platform/macos_jpeg.c, as listed in cmake/macos-arm64.cmake. This documented native replacement remains in place. The seven bundled-JPEG allocation, error and destination callbacks are excluded from the active translation; their exact Mac symbols and retained legacy bodies were inspected rather than reconstructed.

The engine-side allocator was wrong. Mac R_LoadJpg allocates returned pixels through ri+0x10 at 0x1ea0c9, which is Hunk_AllocateTempMemoryInternal. Image_LoadRaw releases that pointer with Hunk_FreeTempMemory at 0xfd12b. Native ImageIO integration instead allocated it with Z_MallocInternal; it now uses the matching temporary-hunk allocator under COD2_X64.

The native provider continues to own JPEG codec memory, malformed-input/dimension handling and encoded output. RGB input/output and engine file ownership were compared with Mac 0x1e9d98/0x1e9f40. No claim is made that ImageIO produces identical compressed bytes or reproduces every bundled-codec error.

The arm64 Release integration object compiles. The existing native JPEG provider fixture passes under ASan/UBSan: encode, RGB roundtrip, maximum-dimension rejection and invalid input. That fixture calls the provider directly and does not exercise the corrected engine allocator or screenshot/raw-image commands. Bundled codec helpers are not compiled in the selected Apple path. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
