# cgame/cg_view_mp module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 7 equal, 2 fixed.** Every full function was compared with Mac 1.3. The seven-entry cubemap table at 0x33c150 was decoded; all six native axes already match. Exact-symbol disassembly was restarted at `CG_FxRestart` because whole-binary disassembly misaligned its entry.

`CG_CalcViewValues` used a float cubemap degree factor instead of the double at 0x1d4215, and skipped the intermediate int-to-float conversions. It omitted the player `deltaTime` adjustment in the view-state clock and damage time (0x1d3db0–0x1d3de0), applied horizontal bob along the forward vector instead of the right vector (0x1d3f3f–0x1d3fae), and omitted the negative landing-time repair at 0x1d49a2. Those native branches now follow the inspected operations.

The whole routed third-person caller at 0x1d3722 was audited. Its `atan2` result must round to float before multiplication by double degrees (0x1d39dd–0x1d39f7). Its focus-pitch cap also preserves unordered input as the Mac compare does. Collision lift/retrace, focus distance, yaw and height behavior match. The historical CoD2x orbit path remains intact.

History was inspected before preserving the camera attachment and frame work from 12f78f3 and 071a710. Mounted `tag_player`, required-tag errors, firing jitter, weighted view-weapon `tag_camera`, snapshot entity preparation, recoil integration and delayed drawing were compared as parts of their whole Mac callers. No existing camera or native backend adaptation was removed.

All three assigned client translation units compiled as arm64 Release in stock and CoD2x modes. The dedicated target linked but does not compile cgame. Seven existing camera groups and five active-frame groups passed with ASan/UBSan; the specific arithmetic corrections were compiled and reviewed, not directly exercised. No new tests, full CI or replays were run. All five inactive source configurations match the baseline; no `__LINE__` expansions occur in this source.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-view.noindex/`.

## Integrated batch verification

Integrated source ed4d921 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh 819d282 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
