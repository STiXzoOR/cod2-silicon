# xanim/xmodel module reconciliation

Updated 2026-11-04.

**25/30 reconciled: 24 equal, 1 intentional; 5 unchecked.** Complete Mac functions were compared with native getters, cache wrappers, string cleanup, default-model fallback and collision tracing. Linux-only default helpers were matched to their complete inline blocks in `XModelPrecache` at 0x458b0.

`Com_ValidXModelName` now accepts both slash and backslash after the case-insensitive `xmodel` prefix. Mac 0x45ae0 accepts only slash. Windows 0x490950-0x490977 explicitly accepts both, with separator comparisons at 0x490967 and 0x49096b. This follows the requested Windows precedence for player-visible platform differences.

`XModelTraceLine` at 0x45bc4 was compared through both quaternion transforms, broadphase, triangle tests, 0.125 trace bias, barycentric tolerances and final normal rotation. Its existing native SSE grouping matches Mac. `AxisTransformVector` belongs to `universal/com_math.c`; its three column products and addition order match Mac 0x2bc62. No shared source was edited. The transpose helper was compared in the Mac trace caller.

The five unchecked rows are `XModelGetCollLod`, `XModelReplace`, `XModelSetUnusedValue`, `XModelReturnZero` and `XModelIgnoreChar`. Their Linux definitions were read and native sources searched, but no native body/caller or unique complete Mac identity was established. Linux-only definitions were not used to invent native implementations.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. The separator change is compiled and reviewed, not exercised. No tests or replays were added or run. Preprocessed output with `COD2_X64` undefined is identical to the base source. Exact-symbol disassembly corrected the full-binary tool's misaligned `XModelBad` entry.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
