# cgame/cg_compassfriendlies_mp module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 2 equal, 1 fixed.** The full 0x18f13e–0x18fea4 drawing function, weapon-ping update and radius helper were read against Mac 1.3. The radius helper was restarted with exact-symbol disassembly after the whole-binary listing misaligned its entry. Typed actor, client, entity, snapshot and material storage was checked against the referenced field offsets.

Friendly firing fades ran in the wrong direction. Native now compares the same inclusive end time and fades after the ping start, as 0x18fb97–0x18fd98 do. It passes the computed alpha to the firing/flash color instead of leaving the original alpha there. The normalized-direction branch initializes that alpha from clamp saturation, as 0x18fda7–0x18fe28 do, removing an uninitialized native local.

Actor/entity traversal, local-client selection and packed friend data now read `nextSnap`, matching Mac cg+0x24 at 0x18f1b9/0x18f589/0x18f6f4/0x18f8a1. The old current-snapshot reads originated in the initial reconstruction; normal HUD painting already follows the next-snapshot gate in `CG_Draw2D`.

Team zero now takes the Mac enemy-only path at 0x18f28e–0x18f29c. The compass center multiplies size by each rectangle extent before multiplying by one half (0x18f206–0x18f24a), and the radius max/min checks retain their reference branch ordering. Packed friend coordinates/yaw, update expiry, three-second ping flash, range saturation, split-screen geometry and enemy fading were compared throughout both loops.

The actual arm64 Release client translation unit compiled with stock and CoD2x flags. The dedicated target also linked, but excludes cgame. These compass paths were compiled and reviewed, not exercised; live icon/fade rendering remains an integration check. No new tests, full CI or replays were run. All five inactive source configurations match the baseline, and this source has no `__LINE__` expansions.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-view.noindex/`.

## Integrated batch verification

Integrated source ed4d921 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh 819d282 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.

## Independent math-caller follow-up — 2026-11-05

The **two equal, one fixed** accounting is unchanged. Fresh review corrected
the distance-alpha lower clamp to retain unordered `dist` as Mac
0x18f980/0x18fd53–0x18fd77 does. Both actor-color alpha selections now use
`fade < color[3] ? fade : color[3]`, preserving the source-color tie/unordered
choice of `MINSS` at 0x18fbc0–0x18fbd0. The existing fixed owner evidence is
retained with these specific corrections appended.

Configured stock/CoD2x arm64 Release client objects compile. Dedicated builds
exclude cgame. No existing fixture executes the whole compass-friendlies
caller, so these changes are compiled and reviewed only. Five inactive
configurations and exact OFF source text match; there are no `__LINE__` uses.
No new tests, full CI or replay was run.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
