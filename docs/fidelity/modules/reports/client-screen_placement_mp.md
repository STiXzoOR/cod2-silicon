# client/screen_placement_mp module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 8 equal, 1 fixed.** Complete Mac 1.3 bodies and SSE arithmetic order were compared. Four alignment tables at 0x33bc40–0x33bca0 were decoded. Exact-symbol disassembly confirmed the four single-multiply converters and the empty split-screen text offset helper.

## Correction

`SetScreenScaling` selected the opposite safe-area bound for unordered float differences. Mac 0x18ebfa, 0x18ec19, 0x18ec3b and 0x18ec5d use `cmpnltss`, which accepts unordered values; native used `>= 0`. Native now uses `!(difference < 0)` for the same four selections. Finite aspect, safe-area and scale arithmetic already matched.

## Verification and limits

No new tests, full CI or replays were run. Existing input/platform probes were inspected; they do not cover unordered safe-area values. The arm64 Release dedicated target built, and the client translation unit compiled separately with the configured client flags. All five OFF preprocessing configurations match the base; this file has no `__LINE__` expansions. The correction is compiled and reviewed, not exercised. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.

## Integrated batch verification

Integrated source `3ee60be` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
