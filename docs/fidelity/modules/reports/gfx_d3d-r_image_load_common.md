# gfx_d3d/r_image_load_common module reconciliation

Updated 2026-11-05.

**5/5 reconciled: 4 equal, 1 fixed.**

Hand comparison covered picmip lookup, all card-memory formats and mip accounting, setup clamps, and the three texture lock/copy/unlock paths. The current GL wrappers remain selected.

Mac 0x10998d/0x109a80 skips unsupported format 33. Native incorrectly included X8B8G8R8 among two-byte uploads; the native stride helper and 2D/cube dispatch now reject it. The original source is preserved behind the native guard.

The native volume path intentionally keeps RowPitch/SlicePitch copying, missing-resource guards and four-byte X8R8G8B8 BGRA source slices. Mac 0x1099f0 advances those slices by three bytes; current native producers expand them to four bytes, and the active GL volume contract requires that layout. This defined producer/consumer contract remains intact.

The arm64 Release object compiles. The existing `volume_image_upload` ASan/UBSan fixture passes padded rows/slices, shifted mips, DXT1/DXT5 and tiny dimensions. It exercises retained volume behavior, not rejection of format 33. Full rendering was not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
