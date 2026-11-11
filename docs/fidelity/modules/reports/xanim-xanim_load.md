# xanim/xanim_load module reconciliation

Updated 2026-11-04.

**4/4 reconciled: 3 equal, 1 fixed.** Mac 1.3 is the authority. The native loader and both quaternion readers were compared with the complete Mac bodies and inline blocks, including notification loading. Linux supplied split helper names only.

The delta quaternion and translation frame counts now use unsigned comparisons, matching Mac 0xba4af and 0xbadd1. The previous signed comparisons selected the wrong index stream when a frame count crossed 32767.

Indexed channels now reserve `sizeof(channel) + 2 * (count - 1)`, including the Mac tail space. Mac uses bases 10 for quaternions and 14 for translations, while copying indices at offset 8. Native had retained only the index offset in its allocation sizes. The native formula accounts for pointer alignment and preserves the existing frame-zero allocation fixes. Both byte-index and short-index branches were checked.

The comparison also covered version 14, flags, loop-frame adjustment, frequency, bone names and bitsets, every static/animated quaternion and translation branch, quaternion sign continuity, notification types/times, the `end` sentinel and file cleanup. Native hunk callbacks zero their allocations; no indeterminate storage was introduced. Corrupt high-bit bone counts and overflowing notification counts remain outside the valid asset/allocation contract.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. Changes are compiled and reviewed, not exercised. No tests or replays were added or run. Preprocessed output with `COD2_X64` undefined is identical to the base source.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.

## Integrated batch verification

Integrated source `cf0fc8c` with cross-module evidence commit `e785f19` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `f763fd9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
