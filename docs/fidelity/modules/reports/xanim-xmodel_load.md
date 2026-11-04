# xanim/xmodel_load module reconciliation

Updated 2026-11-04.

**15/15 reconciled: 5 equal, 5 fixed, 5 intentional.** The complete Mac `XModelLoad` body at 0xbb616 was compared with native configuration, collision, LOD/material, parts, base-pose and bone-bounds loading. Linux split helpers were checked in those inline blocks.

The native collision-surface file check used `sizeof(XModelCollSurf)`, including a native pointer, against serialized bytes. Mac 0xbbc55-0xbbc75 establishes a 36-byte bounds/index/contents/flags tail; with the triangle count, a surface occupies at least 40 disk bytes before triangle data. The native check now uses that disk minimum while retaining allocation overflow validation. This prevents valid compact records from being rejected because LP64 structures are larger. Independent review found that relaxing the initial check could expose a short record accepted by the older triangle-only check. Each surface now requires 40 bytes before reading its count, and triangle validation reserves the following 36 metadata bytes. Existing malformed-count rejection remains a native adaptation; no claim is made that the entire loader validates every malformed field.

Version diagnostics now retain the Mac newlines for models, parts and surfaces, corresponding to Mac calls at 0xbb69d, 0xbc0b3 and 0xbc033. Independent review caught a wrong reading of the surface-conflict diagnostic at 0xbc11a: Mac string 0x24b4c8 and Windows string 0x5b0d58 contain both a newline after the conflict sentence and a following repair hint. The native diagnostic now preserves both lines. The first worker commit's missing-newline claim was incorrect and was corrected before integration.

Base-pose comparison covered root initialization, quaternion reconstruction/scaling/multiplication, length/weight calculation, parent transforms and both part-bit masks. Collision comparison covered all triangle floats, the 0.001 expansion, contents mask 0xdffffffb and contents accumulation. Allocation sizes and memory accounting intentionally grow for native pointers. The native 127-bone rejection precedes allocation of names, avoiding consumption of hunk storage for rejected data.

The five intentional rows describe renderer capability helpers and surface wrappers: Apple dedicated models keep CPU collision, parts and bounds, then skip GPU surfaces and skins. The native non-dedicated branch retains the Mac capability gate and surface/skin paths. `XModelReadSurface` lives in `gfx_d3d/r_xsurface_load_obj.c`; its complete Mac body at 0xc7318 matches after pointer-size adaptation, including blend records and odd-triangle duplication. That source was read only.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. Changes are compiled and reviewed, not exercised; its dedicated path skips rendering surfaces. No tests or replays were added or run. Preprocessed output with `COD2_X64` undefined is identical to the base source. The non-dedicated renderer branch was read but was not compiled by this target.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.
