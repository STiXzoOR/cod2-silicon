# client/cl_cin_mp module reconciliation

Updated 2026-11-04.

**12/12 reconciled: 8 equal, 3 fixed, 1 intentional.** Every Mac 1.3 wrapper was compared with exact-symbol disassembly, including its arguments and return forwarding.

## Corrections

The skippable command now sets the cinematic skip byte, as Mac 0x16308b does. The missing unskippable command now clears it, matching 0x16309b. `SCR_StopCinematic` now returns while that byte is zero and forwards stop only when it is set (0x1630bf–0x1630cb).

The orchestrator authorized narrow dependency edits to `src/PC/client_mp/cl_main_mp.c` and `src/headers/PC/client_mp/cl_funcs.h`. These add guarded declarations and register `unskippablecinematic` immediately after `cinematic`, matching Mac 0x148fb1–0x148fcd. Mac 0x148fd2–0x148fe9 gives autocomplete only to `cinematic`; shutdown 0x14bcb3–0x14bcc6 removes `cinematic` then `logo` and leaves `unskippablecinematic` registered. Both quirks are preserved. This is not a full `cl_main_mp` reconciliation.

## Native boundary and verification

Apple cinematic playback remains the documented unsupported backend from `WS11-bringup.md`: play returns -1, command playback clears `nextmap` and marks `introPlayed`. Both command wrappers use that existing immediate-finish replacement. No QuickTime or RoQ backend reconstruction was attempted.

No new tests, full CI or replays were run. The arm64 Release dedicated target built; both affected client translation units compiled separately with the configured client flags. Five OFF preprocessing configurations match the base for all dependency files; none contains `__LINE__` expansions. Corrections are compiled and reviewed, not exercised. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.
