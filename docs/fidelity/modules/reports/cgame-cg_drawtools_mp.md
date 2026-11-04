# cgame/cg_drawtools_mp module reconciliation

Updated 2026-11-04.

**13/13 reconciled: 11 equal, 1 fixed, 1 intentional.** Every assigned Mac body was read. The exact `CG_DrawStringExt` entry was used after the full-binary disassembly misaligned its prologue. The legacy source is unchanged.

`CG_DrawStringExt` now declares the native `colorWhite` symbol as a four-float array. Mac 0x1a191a loads the array address from the import slot when the supplied color is null. The old native pointer declaration instead loaded the first floats as an address. Active `q_shared.c` defines a `vec4_t`; the generated import wrapper points at that array. Both actual storage translation units compile. The shadow flag also follows the unsigned nonzero comparison at 0x1a1938, including negative integer inputs.

The remaining comparisons cover developer font/alignment wrappers, fade thresholds and division, north-yaw parsing, viewport reset, both complete compass spring loops, and both rotation loops. Mac's unusual ignored developer-text x coordinate and its width/height rotation arithmetic are preserved. The optional spring-pointer dvar keeps the established native null-slot/null-dvar fallback before initialization; registered-dvar behavior matches Mac 0x1a1ee0.

All four assigned client translation units compile with actual arm64 Release commands in stock and CoD2x configurations. The dedicated Release target builds. Ten legacy configurations report zero source mismatches; `git diff --check` passes. No new tests, replays or full CI were run. The corrections are compiled and reviewed, not exercised at runtime.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-interface.noindex/`.
