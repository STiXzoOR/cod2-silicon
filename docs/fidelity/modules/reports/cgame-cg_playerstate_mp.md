# cgame/cg_playerstate_mp module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 2 equal, 1 intentional.** Every exported Mac body was
compared with native by hand, supported by triage and constant checks. No
source corrections were needed.

`CG_Respawn` at 0x1e3c56 preserves the snapshot state copy, weapon/offhand
selection, timers, sway/kick/gun/damage/shake/error resets and ADS/offhand/
breath/low-health calls. The full listing misdecoded its prologue, so the
function was also disassembled from its exact symbol.

`CG_TransitionPlayerState` at 0x1e4088 preserves damage feedback and the
last-four event loop. Native already has the Mac `<= oldSequence-4`
exclusion, changed-event comparison and nextState.eventParm destination.

`CG_DamageFeedback` at 0x1e3e10 matches the damage scale/clamp, nondirectional
case, byte-angle arithmetic, directional dot products, oldest-slot selection,
jitter and notification timers. Its intentional native adaptation is the
existing 2000ms fallback when the damage-icon-time dvar is null. Mac directly
dereferences the registered dvar. Keeping the fallback preserves defined
native bootstrap behavior.

## Verification and limits

The arm64 Release dedicated target built successfully with `COD2_X64=ON` at
background priority and two jobs. The four assigned client translation units
were separately compiled using the actual `cod2_macos` compiler commands,
since the dedicated target excludes cgame. They also compiled with the same
commands plus `-DCOD2_CODX=1`. The player source retains its existing warning
for passing the byte-backed render scratch to the typed weapon renderer.

The COD2_X64-off comparison passed all five inactive configurations. Exact
SDK preprocessing with COD2_X64 removed also matched, including real line
expansions. These sources contain no direct `__LINE__` uses after the edits.
No new tests, full CI, replays or runtime checks were run. All corrections are
compiled and reviewed, not exercised in a client session. The orchestrator
handles integrated runtime validation.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-cgame-player.noindex/`.
