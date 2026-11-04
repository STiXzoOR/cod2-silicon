# cgame/cg_event_mp module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 1 equal, 1 intentional.** No source changes.

Compared the full CG_CheckEvents body at 0x1e614a, including event-only entities, sequence wrap, four-event cap and restoration of the prior low-byte event parameter.

Compared the full CG_EntityEvent body at 0x1e4650 and its native helpers. Decoded all 60 dispatch entries at 0x33c900 and the obituary icon table at 0x33c9f0. Checked first/third-person footstep and reload fallbacks, camera arithmetic, offhand/fire cases, sound origins, impact table indices, FX time warp/bolts/orientation, and obituary name/team/message branches. The unusual snapshot-weapon comparison in the weapon-ping path matches Mac.

The existing Apple arm64 combat-event trace is intentional and remains controlled by COD2_NATIVE_TRACE_ENABLED. Existing native sound, renderer and oriented-effect providers are preserved. No platform-specific player behavior requiring a Windows substitution was found. The pending cg_main sound-origin correction belongs to its separate owner and was not duplicated here.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. It excludes cgame, so all five assigned client translation units were also compiled individually with this worktree's actual `cod2_macos` compile commands. OFF preprocessing matches the base with real diagnostic line expansions preserved. No new tests, full CI or replays were run. Changes are compiled and reviewed, not exercised in a running client.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-world.noindex/`.

## Integrated batch verification

Integrated source a90c5b7 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh d873794 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
