# cgame/cg_event_mp module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 1 equal, 1 intentional.** No source changes.

Compared the full CG_CheckEvents body at 0x1e614a, including event-only entities, sequence wrap, four-event cap and restoration of the prior low-byte event parameter.

Compared the full CG_EntityEvent body at 0x1e4650 and its native helpers. Decoded all 60 dispatch entries at 0x33c900 and the obituary icon table at 0x33c9f0. Checked first/third-person footstep and reload fallbacks, camera arithmetic, offhand/fire cases, sound origins, impact table indices, FX time warp/bolts/orientation, and obituary name/team/message branches. The unusual snapshot-weapon comparison in the weapon-ping path matches Mac.

The existing Apple arm64 combat-event trace is intentional and remains controlled by COD2_NATIVE_TRACE_ENABLED. Existing native sound, renderer and oriented-effect providers are preserved. No platform-specific player behavior requiring a Windows substitution was found. The pending cg_main sound-origin correction belongs to its separate owner and was not duplicated here.

## Verification and limits

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. It excludes cgame, so all five assigned client translation units were also compiled individually with this worktree's actual `cod2_macos` compile commands. OFF preprocessing matches the base with real diagnostic line expansions preserved. No new tests, full CI or replays were run. Changes are compiled and reviewed, not exercised in a running client.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-world.noindex/`.
