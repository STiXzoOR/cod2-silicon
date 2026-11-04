# cgame/cg_compassfriendlies_mp module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 2 equal, 1 fixed.** The full 0x18f13e–0x18fea4 drawing function, weapon-ping update and radius helper were read against Mac 1.3. The radius helper was restarted with exact-symbol disassembly after the whole-binary listing misaligned its entry. Typed actor, client, entity, snapshot and material storage was checked against the referenced field offsets.

Friendly firing fades ran in the wrong direction. Native now compares the same inclusive end time and fades after the ping start, as 0x18fb97–0x18fd98 do. It passes the computed alpha to the firing/flash color instead of leaving the original alpha there. The normalized-direction branch initializes that alpha from clamp saturation, as 0x18fda7–0x18fe28 do, removing an uninitialized native local.

Actor/entity traversal, local-client selection and packed friend data now read `nextSnap`, matching Mac cg+0x24 at 0x18f1b9/0x18f589/0x18f6f4/0x18f8a1. The old current-snapshot reads originated in the initial reconstruction; normal HUD painting already follows the next-snapshot gate in `CG_Draw2D`.

Team zero now takes the Mac enemy-only path at 0x18f28e–0x18f29c. The compass center multiplies size by each rectangle extent before multiplying by one half (0x18f206–0x18f24a), and the radius max/min checks retain their reference branch ordering. Packed friend coordinates/yaw, update expiry, three-second ping flash, range saturation, split-screen geometry and enemy fading were compared throughout both loops.

The actual arm64 Release client translation unit compiled with stock and CoD2x flags. The dedicated target also linked, but excludes cgame. These compass paths were compiled and reviewed, not exercised; live icon/fade rendering remains an integration check. No new tests, full CI or replays were run. All five inactive source configurations match the baseline, and this source has no `__LINE__` expansions.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-view.noindex/`.
