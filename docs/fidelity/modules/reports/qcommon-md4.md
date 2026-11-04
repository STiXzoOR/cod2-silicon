# qcommon/md4 module reconciliation

Updated 2026-11-04.

**8/8 reconciled: 8 equal.** Mac 1.3 i386 is the authority. No source changes were needed.

The five standalone functions were read from exact-symbol disassembly. `MD4Transform` at 0xb5f9e matches the little-endian input decode, all three Boolean rounds, their constants and word order, rotations, state accumulation and 64-byte scratch clear. A script also checked all 48 word-index/rotation pairs against the source.

The Linux-only `Decode`, `Encode` and `MD4Init` entries were checked in their Mac callers. Decode is inlined at 0xb5fe7-0xb6021. Encode is inlined twice in `MD4Final` at 0xb65b2-0xb65e7 and 0xb6630-0xb6662. Initialization is inlined into both checksum wrappers at 0xb6698-0xb66c2 and 0xb6720-0xb674a. Those ranges provide an identity and behavior match despite the missing standalone bodies.

`MD4Update` at 0xb6462 matches the 32-bit bit-count carry, 64-byte block loop and remainder copy. `MD4Final` at 0xb659e matches count serialization, the 56/120 padding boundary, digest serialization and 88-byte context clear. The wrappers at 0xb668e and 0xb6716 match initialization, the optional four key bytes before the message, and the XOR of four digest words. The padding data at 0x348720 is 0x80 followed by 63 zero bytes.

## Verification and limits

The arm64 Release `cod2_macos_ded` build completed with exit 0 and compiled this file. The existing `tests/online/md4.c` fixture was inspected but not run. No tests were added or changed, and no full CI or replay was run; the parent owns runtime validation. This reconciliation establishes a source/disassembly comparison, not exercised checksum behavior.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-utils.noindex/`.
