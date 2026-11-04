# client/cl_cin_mp module reconciliation

Updated 2026-11-04.

**12/12 reconciled: 8 equal, 3 fixed, 1 intentional.** Every Mac 1.3 wrapper was compared with exact-symbol disassembly, including its arguments and return forwarding.

## Corrections

The native skip byte starts at one, matching Mac initialized data at 0x34b220. Independent review caught an initial zero default; the initializer was corrected and the client object rebuilt before integration. The skippable command now sets the cinematic skip byte, as Mac 0x16308b does. The missing unskippable command now clears it, matching 0x16309b. `SCR_StopCinematic` now returns while that byte is zero and forwards stop only when it is set (0x1630bf–0x1630cb).

The orchestrator authorized narrow dependency edits to `src/PC/client_mp/cl_main_mp.c` and `src/headers/PC/client_mp/cl_funcs.h`. These add guarded declarations and register `unskippablecinematic` immediately after `cinematic`, matching Mac 0x148fb1–0x148fcd. Mac 0x148fd2–0x148fe9 gives autocomplete only to `cinematic`; shutdown 0x14bcb3–0x14bcc6 removes `cinematic` then `logo` and leaves `unskippablecinematic` registered. Both quirks are preserved. This is not a full `cl_main_mp` reconciliation.

## Native boundary and verification

Apple cinematic playback remains the documented unsupported backend from `WS11-bringup.md`: play returns -1, command playback clears `nextmap` and marks `introPlayed`. Both command wrappers use that existing immediate-finish replacement. No QuickTime or RoQ backend reconstruction was attempted.

No new tests, full CI or replays were run. The arm64 Release dedicated target built; both affected client translation units compiled separately with the configured client flags. Five OFF preprocessing configurations match the base for all dependency files; none contains `__LINE__` expansions. Corrections are compiled and reviewed, not exercised. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.

## Integrated batch verification

Integrated source `3ee60be` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
