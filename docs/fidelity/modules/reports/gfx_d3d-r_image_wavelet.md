# gfx_d3d/r_image_wavelet module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 equal.**

Hand comparison covered the Mac 0x10a114–0x10b818 small-dimension path, reservoir initialization/refill, bit advances, alpha prepass, Huffman escape biases, parity bits, color deltas and 2x2 reconstruction. No code change was needed.

A static binary-data comparison verified all 4,096 entries in each alpha, red-green and blue Huffman table at 0x32b9c0, 0x32f9c0 and 0x3339c0. This is a comparison of the tables, not a claim of executable-byte identity.

The arm64 Release object compiles. The existing image-loader fixture substitutes this codec, so the actual decompressor was not run in this unit. Lookahead padding and native byte-order decisions are reported in r_image_load_obj. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
