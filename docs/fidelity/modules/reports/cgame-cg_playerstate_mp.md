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

## Integrated batch verification

Integrated source ed4d921 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh 819d282 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
