# cgame/cg_info_mp module reconciliation

Updated 2026-11-04.

**2/2 reconciled: 1 equal, 1 fixed.** Both complete Mac bodies were read, with `CG_LoadingString` restarted at its exact symbol because the full-binary disassembly misaligned its prologue. The legacy source is unchanged.

The waiting-for-server message used y=452 in native. Mac 0x1e4597 passes float bits 0x43db8000, or y=439. Windows 0x4d3645 passes the same value, so this is not a Mac-specific placement. Native now uses 439 and the Mac unlimited-width argument 0 at 0x1e4547. Centering retains the Mac subtract-then-half operation order.

The menu response gate, repeated script-menu close, sound-stop boundary, loading flags, map-name fallback, signed-byte lowercase loop, hunk-usage early return and four-phase dots were compared. `CG_LoadingString` agrees on its 1024-byte copy, optional diagnostic and screen update. Existing native sound and UI backend owners remain in use.

All four assigned client translation units compile with actual arm64 Release commands in stock and CoD2x configurations. The dedicated Release target builds. Ten legacy configurations report zero source mismatches; `git diff --check` passes. No new tests, replays or full CI were run. The correction is compiled and reviewed, not exercised at runtime.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-interface.noindex/`.

## Integrated batch verification

Integrated source ed4d921 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh 819d282 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
