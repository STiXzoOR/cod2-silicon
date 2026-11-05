# gfx_d3d/r_font module reconciliation

Updated 2026-11-05.

**13/13 reconciled: 10 equal, 3 intentional.**

Hand comparison covered all thirteen rows, including glyph fallback/binary search, registered-font limits/copies, normalized scale, character widths, console parsing and icon drawing. The two icon dispatch tables at 0x32b340/0x32b35c were decoded; color, dimensions, material registration and flip branches match.

Preserve the documented WS12 callback ABI adaptations: direct void draw submission followed by deterministic zero from reconstructed int interfaces, and widened sizeof(Font_s) allocation/copying. These explicit native choices do not retire any current GL text behavior. No code change was needed.

The arm64 Release object compiles. All six existing font-icon ASan/UBSan cases pass: material, width, height, color, flip and measurement. They exercise parsing, drawing and width accumulation; font duplication and full frontend rendering were not exercised. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.
