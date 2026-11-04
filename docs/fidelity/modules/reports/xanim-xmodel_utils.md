# xanim/xmodel_utils module reconciliation

Updated 2026-11-04.

**13/13 reconciled: 12 equal, 1 intentional.** Mac getters and LOD override/distance logic were compared with the native bodies. The Linux names `XModelGetBasePoseOffset` and `XModelSetLodOverride` map completely to Mac `XModelGetBasePoseBone` and `XModelSetTestLods`.

The comparison checked signed bone/LOD counts, material-name fallback, inline base-pose addressing, the logical half-offset shift, override storage and ordered distance comparisons. The LOD behavior for zero, negative zero and NaN matches Mac. Exact-symbol disassembly corrected the full-binary tool's misaligned `XModelGetName` entry.

`XModelGetSurfaces` at 0xb67d8 assumes a non-null surface object. Native retains an existing shutdown safeguard that returns zero and null outputs for partially/unloaded models. Its normal path matches the Mac pointer array, part-bit address and signed surface count, with native pointer-width adjustment. This is the intentional row; no source changes were needed.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. No tests or replays were added or run. Utility paths were reviewed, not individually exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
