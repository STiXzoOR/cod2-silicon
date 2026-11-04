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
