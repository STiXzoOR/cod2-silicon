# cgame/cg_ents_mp module reconciliation

Updated 2026-11-04.

**27/27 reconciled: 23 equal, 4 fixed.** Compared full Mac bodies and the native storage and callback contracts. This includes both callers routed from universal/com_vector. Legacy source is unchanged under COD2_X64=OFF.

## Corrections

MG42 animation selected the root instead of the chosen animation. Mac 0x16335d-0x16336c passes the animation index before root index zero. The native xanim implementation confirms that argument order. The caller now uses the matching XAnimTree pointer type.

Brush entities were identified through the light color field. Mac 0x163de5 and 0x164a14 test `solid == 0xffffff`. Corrected both the loop-sound midpoint and script-mover brush-model discriminator. Dynamic lights also reversed red and blue; Mac 0x163e43-0x163e65 sends the low color byte as red and the high byte as blue.

Missile trails used tag_flash. Mac 0x1648b9 reads scr_const+0xa2, which the active native layout identifies as tag_origin.

## Comparison examples and boundaries

Decoded the ten-way CG_ProcessEntity jump table at 0x33b2f0 and checked its inlined looped-FX and turret bodies. Compared complete mover adjustment and lerp callers, including the signed range gates, SSE arithmetic order, predicted-player exclusion and corpse-info displacement at 0x163da3.

Generated data owns native entity/item/weapon pointers and arrays. Existing renderer submission, delayed skinning/bounds, audio and oriented-FX interfaces remain in place. No platform-specific player behavior requiring a Windows substitution was found.

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
