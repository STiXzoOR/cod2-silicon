# xanim/xmodel_utils module reconciliation

Updated 2026-11-04.

**13/13 reconciled: 12 equal, 1 intentional.** Mac getters and LOD override/distance logic were compared with the native bodies. The Linux names `XModelGetBasePoseOffset` and `XModelSetLodOverride` map completely to Mac `XModelGetBasePoseBone` and `XModelSetTestLods`.

The comparison checked signed bone/LOD counts, material-name fallback, inline base-pose addressing, the logical half-offset shift, override storage and ordered distance comparisons. The LOD behavior for zero, negative zero and NaN matches Mac. Exact-symbol disassembly corrected the full-binary tool's misaligned `XModelGetName` entry.

`XModelGetSurfaces` at 0xb67d8 assumes a non-null surface object. Native retains an existing shutdown safeguard that returns zero and null outputs for partially/unloaded models. Its normal path matches the Mac pointer array, part-bit address and signed surface count, with native pointer-width adjustment. This is the intentional row; no source changes were needed.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. No tests or replays were added or run. Utility paths were reviewed, not individually exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.
