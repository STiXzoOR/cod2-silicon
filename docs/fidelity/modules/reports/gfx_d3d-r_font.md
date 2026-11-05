# gfx_d3d/r_font module reconciliation

Updated 2026-11-05.

**13/13 reconciled: 10 equal, 3 intentional.**

Hand comparison covered all thirteen rows, including glyph fallback/binary search, registered-font limits/copies, normalized scale, character widths, console parsing and icon drawing. The two icon dispatch tables at 0x32b340/0x32b35c were decoded; color, dimensions, material registration and flip branches match.

Preserve the documented WS12 callback ABI adaptations: direct void draw submission followed by deterministic zero from reconstructed int interfaces, and widened sizeof(Font_s) allocation/copying. These explicit native choices do not retire any current GL text behavior. No code change was needed.

The arm64 Release object compiles. All six existing font-icon ASan/UBSan cases pass: material, width, height, color, flip and measurement. They exercise parsing, drawing and width accumulation; font duplication and full frontend rendering were not exercised. No new tests were written.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-images.noindex/`.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
