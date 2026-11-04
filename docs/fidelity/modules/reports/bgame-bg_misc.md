# bgame/bg_misc module reconciliation

Updated 2026-11-04.

**13/13 reconciled: 13 equal.** Mac 1.3 is the authority; Linux 1.3 was read only. No source changed.

All 44 player dvars, the trajectory jump tables and `singleClientEvents` were compared with the binary by script.

## Verification and limits

No new per-fix tests were written. The local CI steps 02 to 13 passed, and both builds compile.

Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the Linux `original` recording, no logic field differs in any of the 3,806 player records: `pm_type`, `pm_flags`, `pm_time`, `weapon`, `weaponstate` and `health` are all identical, and so are the view angles. As a temporary experiment, not committed, `Sys_SnapVector` was switched to the Linux `rint`. The largest origin difference fell from 14.5 to 0.118 units and the largest velocity difference from 153 to 2, with 3,738 of the 3,806 records within 0.001 units. That shows nearly all the drift comes from that one platform difference. Native keeps the Mac truncation.

One independent review compared the five changed paths with the Mac disassembly, and spot-checked `PM_CheckLadderMove` and the `PM_ViewHeightAdjust` restart logic, and found no mismatches. It did not check the code around the changed lines in `PM_Footsteps`, `PM_CheckDuck` and `PM_FoliageSounds`. The corrections themselves are compiled and reviewed but not run on their own. `bg_misc` and `bg_mantle` had no changes, so that review did not cover them.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.
