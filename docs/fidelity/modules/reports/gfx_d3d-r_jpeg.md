# gfx_d3d/r_jpeg module reconciliation

Updated 2026-11-05.

**9/9 reconciled: 1 fixed, 8 intentional.**

The active Apple build selects the engine integration in r_jpeg.c and the ImageIO implementation in src/platform/macos_jpeg.c, as listed in cmake/macos-arm64.cmake. This documented native replacement remains in place. The seven bundled-JPEG allocation, error and destination callbacks are excluded from the active translation; their exact Mac symbols and retained legacy bodies were inspected rather than reconstructed.

The engine-side allocator was wrong. Mac R_LoadJpg allocates returned pixels through ri+0x10 at 0x1ea0c9, which is Hunk_AllocateTempMemoryInternal. Image_LoadRaw releases that pointer with Hunk_FreeTempMemory at 0xfd12b. Native ImageIO integration instead allocated it with Z_MallocInternal; it now uses the matching temporary-hunk allocator under COD2_X64.

The native provider continues to own JPEG codec memory, malformed-input/dimension handling and encoded output. RGB input/output and engine file ownership were compared with Mac 0x1e9d98/0x1e9f40. No claim is made that ImageIO produces identical compressed bytes or reproduces every bundled-codec error.

The arm64 Release integration object compiles. The existing native JPEG provider fixture passes under ASan/UBSan: encode, RGB roundtrip, maximum-dimension rejection and invalid input. That fixture calls the provider directly and does not exercise the corrected engine allocator or screenshot/raw-image commands. Bundled codec helpers are not compiled in the selected Apple path. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
