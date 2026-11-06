# cgame/cg_consolecmds_mp module reconciliation

Updated 2026-11-04.

**13/13 reconciled: 11 equal, 1 fixed, 1 intentional.** All assigned Mac bodies were read, including the exact entry of `CG_SizeUp_f`, which the full-binary disassembly misaligned. The legacy source is unchanged.

Mac command data at 0x36e740 contains 18 entries. Native had extra `tcmd` and `tell_target` dispatch entries and a different order. The native table now matches all Mac names and handlers, including `mp_QuickMessage` and the null-handler `mr`. `CG_InitConsoleCommands` at 0x1d3180 adds 23 further completions; native's extra `tell` completion was removed. The server/game `tell` command remains registered by its own owners.

The two extra client dispatch entries came from initial commit 63de35a. No native consumer or CoD2x implementation uses either name; both names are absent from the Mac and Windows binaries. WS47 explicitly left these research-only differences unresolved. This reconciliation closes that gap. The existing native current-snapshot allowance in `CG_ConsoleCommand` is retained: a local server can temporarily have no future snapshot. Pointer-size table strides and the optional rumble initialization are retained.

The score request timeout/reset, popup flag, voice-chat eligibility, shellshock arguments and float conversion order agree with Mac. Scripted table comparison checks every command name/handler, and the call/string triage and constant checks were inspected.

All four assigned client translation units compile with actual arm64 Release commands in stock and CoD2x configurations. The dedicated Release target builds. Ten legacy configurations report zero source mismatches; `git diff --check` passes. No new tests, replays or full CI were run. The corrections are compiled and reviewed, not exercised at runtime.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-interface.noindex/`.

## Integrated batch verification

Integrated source a90c5b7 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh d873794 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
