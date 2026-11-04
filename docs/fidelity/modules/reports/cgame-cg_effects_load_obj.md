# cgame/cg_effects_load_obj module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 1 equal, 2 fixed.** Compared full Mac bodies and all eleven effect-type strings. Legacy source is unchanged under COD2_X64=OFF.

## Corrections

A malformed CSV continued into later files in its directory. Mac 0x1e7c9d returns after reporting that parse error. Restored that exit.

Mac 0x1e7ded allocates one 65536-byte low-temp file-list buffer and shares it across the global and map directories. Native used separate stack buffers and had an unused caller stack buffer. Restored the shared allocation and argument. Per-file CSV contents still use the high-temp arena and effect-name copies use low-temp memory.

The successful table name is an empty string at Mac 0x1e7f4e; native used a fabricated name. The missing-entry diagnostic had an extra newline, and its path cleared low-temp memory despite Mac returning before the clear at 0x1e7f9f. Both now follow Mac, including that error-path lifetime quirk.

## Comparison examples and storage

Compared file caps, case-insensitive sorting, open/read/close, CSV comments and row errors, all 11x23 registrations, and success/error returns. All eleven names match the Mac table at 0x36e7e0. The comparator and table retain full native pointer widths, and existing FX registration providers remain in place. The client compile retains the existing strlen-to-int warning.

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
