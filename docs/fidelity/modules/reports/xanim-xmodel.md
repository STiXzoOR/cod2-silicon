# xanim/xmodel module reconciliation

Updated 2026-11-04.

**25/30 reconciled: 24 equal, 1 intentional; 5 unchecked.** Complete Mac functions were compared with native getters, cache wrappers, string cleanup, default-model fallback and collision tracing. Linux-only default helpers were matched to their complete inline blocks in `XModelPrecache` at 0x458b0.

`Com_ValidXModelName` now accepts both slash and backslash after the case-insensitive `xmodel` prefix. Mac 0x45ae0 accepts only slash. Windows 0x490950-0x490977 explicitly accepts both, with separator comparisons at 0x490967 and 0x49096b. This follows the requested Windows precedence for player-visible platform differences.

`XModelTraceLine` at 0x45bc4 was compared through both quaternion transforms, broadphase, triangle tests, 0.125 trace bias, barycentric tolerances and final normal rotation. Its existing native SSE grouping matches Mac. `AxisTransformVector` belongs to `universal/com_math.c`; its three column products and addition order match Mac 0x2bc62. No shared source was edited. The transpose helper was compared in the Mac trace caller.

The five unchecked rows are `XModelGetCollLod`, `XModelReplace`, `XModelSetUnusedValue`, `XModelReturnZero` and `XModelIgnoreChar`. Their Linux definitions were read and native sources searched, but no native body/caller or unique complete Mac identity was established. Linux-only definitions were not used to invent native implementations.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. The separator change is compiled and reviewed, not exercised. No tests or replays were added or run. Preprocessed output with `COD2_X64` undefined is identical to the base source. Exact-symbol disassembly corrected the full-binary tool's misaligned `XModelBad` entry.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.
