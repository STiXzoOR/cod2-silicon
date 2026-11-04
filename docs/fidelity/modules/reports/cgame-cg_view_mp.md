# cgame/cg_view_mp module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 7 equal, 2 fixed.** Every full function was compared with Mac 1.3. The seven-entry cubemap table at 0x33c150 was decoded; all six native axes already match. Exact-symbol disassembly was restarted at `CG_FxRestart` because whole-binary disassembly misaligned its entry.

`CG_CalcViewValues` used a float cubemap degree factor instead of the double at 0x1d4215, and skipped the intermediate int-to-float conversions. It omitted the player `deltaTime` adjustment in the view-state clock and damage time (0x1d3db0–0x1d3de0), applied horizontal bob along the forward vector instead of the right vector (0x1d3f3f–0x1d3fae), and omitted the negative landing-time repair at 0x1d49a2. Those native branches now follow the inspected operations.

The whole routed third-person caller at 0x1d3722 was audited. Its `atan2` result must round to float before multiplication by double degrees (0x1d39dd–0x1d39f7). Its focus-pitch cap also preserves unordered input as the Mac compare does. Collision lift/retrace, focus distance, yaw and height behavior match. The historical CoD2x orbit path remains intact.

History was inspected before preserving the camera attachment and frame work from 12f78f3 and 27b983a. Mounted `tag_player`, required-tag errors, firing jitter, weighted view-weapon `tag_camera`, snapshot entity preparation, recoil integration and delayed drawing were compared as parts of their whole Mac callers. No existing camera or native backend adaptation was removed.

All three assigned client translation units compiled as arm64 Release in stock and CoD2x modes. The dedicated target linked but does not compile cgame. Seven existing camera groups and five active-frame groups passed with ASan/UBSan; the specific arithmetic corrections were compiled and reviewed, not directly exercised. No new tests, full CI or replays were run. All five inactive source configurations match the baseline; no `__LINE__` expansions occur in this source.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-view.noindex/`.
