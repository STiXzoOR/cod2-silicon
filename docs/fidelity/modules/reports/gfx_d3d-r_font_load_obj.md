# gfx_d3d/r_font_load_obj module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 intentional.**

Exact-symbol disassembly at Mac 0xfb970 was used after the full-binary view misaligned its prologue. File open/errors, the sixteen-byte disk header, inserted four-byte glyph-pointer slot, payload at +20, base+4 string offsets and material-track registration were compared.

The native loader intentionally marshals this ILP32 disk image into a real LP64 Font, validates glyph/string ranges and rejects short reads. Reintroducing in-place thirty-two-bit pointer relocation would truncate native addresses. Current GL still consumes the resulting font; Metal U1 is documentation only. No code change was needed.

The arm64 Release object compiles. The existing font_disk ASan/UBSan fixture passes valid glyph/string relocation, invalid offsets/count and short reads. No new tests or cases were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
