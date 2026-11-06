# bgame/bg_pmove module reconciliation

Updated 2026-11-06.

**55/55 reconciled: 48 equal, 7 fixed.** Mac 1.3 is the authority; Linux 1.3 was read only. The legacy source is unchanged.

Mac inlines `PmoveSingle` and every move type into `Pmove` at 0x70c7e. The dispatch table at 0x329210 was decoded, and each
block was compared with its native function.

## Corrections

**Foliage sounds sent the wrong event.** Mac 0x7263b sends event 0x8b, which is `EV_FOLIAGE_SOUND` in Linux and `case 0x8b`
in the client. Native sent 0x8e, a stance event.

**Going prone while moving cleared the wrong flag.** Mac 0x6e65b clears 0x800, the aim-down-sight override. Native cleared
0x8.

**The prone yaw cap used a fresh delta.** Mac 0x6c77d uses the delta it already has, even after 0x6c775 moved
`proneDirection`. Native recomputed it against the new direction.

**The ground trace retried to the wrong point.** After all-solid recovery, Mac 0x702fb still traces to the first end point.
Native had overwritten it.

**A developer print was missing.** `PM_Footsteps` now prints the turn animation end time, as Mac 0x6f918 does.

## Verification and limits

No new per-fix tests were written. The local CI steps 02 to 13 passed, and both builds compile.

Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the Linux `original` recording, no logic field differs in any of the 3,806 player records: `pm_type`, `pm_flags`, `pm_time`, `weapon`, `weaponstate` and `health` are all identical, and so are the view angles. As a temporary experiment, not committed, `Sys_SnapVector` was switched to the Linux `rint`. The largest origin difference fell from 14.5 to 0.118 units and the largest velocity difference from 153 to 2, with 3,738 of the 3,806 records within 0.001 units. That shows nearly all the drift comes from that one platform difference. At that commit native kept the Mac truncation; it now rounds to nearest, as Windows does (see the module README).

One independent review compared the five changed paths with the Mac disassembly, and spot-checked `PM_CheckLadderMove` and the `PM_ViewHeightAdjust` restart logic, and found no mismatches. It did not check the code around the changed lines in `PM_Footsteps`, `PM_CheckDuck` and `PM_FoliageSounds`. The corrections themselves are compiled and reviewed but not run on their own. `bg_misc` and `bg_mantle` had no changes, so that review did not cover them.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-bgame-20261104.noindex/`.

## Original CrashLand integer-selector follow-up

The original Linux caller at 0x80e30c4 reaches I_clamp at 0x80e3263. That supplies
identity; the complete Mac CrashLand block in PM_GroundTrace at
0x70678–0x70c78 supplies behavior. The earlier body review missed exceptional
arithmetic paths: Mac accepts an unordered discriminant at 0x706c6, rejects
damage only for an ordered minimum-height comparison at 0x7072c, and accepts
an unordered view dip at 0x70770. Native ordered comparisons skipped those paths.
Mac conversions at 0x70974 and 0x70a50 return INT_MIN for NaN or out-of-range
input; an ordinary C cast does not define that result. The damage clamp then
uses wrapped value-minus-100 and negation of the original converted value.

The native path now reproduces those comparisons and conversions explicitly,
and implements the clamp with unsigned arithmetic and sign-bit selection. The
view-dip upper cap still preserves a negative integer-indefinite result. Existing
surface exclusions, animation call, velocity scaling and event arguments remain.
All reads use fully backed playerState and pml fields; no indeterminate memory
is introduced. Finite extreme previous velocity can overflow the squared height
and reach an otherwise undefined native view-dip conversion. This static finding
does not establish that normal gameplay produces those inputs.

PM_GroundTrace remains fixed with expanded evidence. Manual compilation of the
changed translation unit passed for stock/CODX client and dedicated modes.
COD2_X64=OFF source is byte-identical; there are no source __LINE__ expansions.
No tests, CI, replay, full build or runtime exercise was performed. Private
packet: recon-crashland5.noindex; compile logs/objects:
manual-crashland-compile-dfd2eacbaf09.noindex.

## Unordered inertia selectors (accel49)

`PM_Accelerate` at 0x6b970 now preserves three Mac unordered branches. The inertiaMax/accelspeed comparison at 0x6ba22–25 skips only ordered greater-or-equal; the oldVelocity square-sum threshold at 0x6ba47–55 admits unordered as well as greater-or-equal; and the normalized old/new direction dot comparison at 0x6bbbf–c8 skips the clamp only for ordered greater-or-equal. Native used ordered comparisons that excluded these unordered paths. The COD2_X64 branches now use the corresponding negated ordered predicates, preserving float operation order, the double threshold, both local vector copies/normalizers, debug prints, and the final three-coordinate velocity update.

The complete primary body and actual inputs/results were read. A static valid typed-input witness uses quiet-NaN oldVelocity with finite current velocity, direction, wishspeed100, accel9, frametime0.1, stopspeed100, inertiaMax50 and inertiaAngle0: Mac reaches the dot clamp and applies50, while the old native gate applied about90. This is a source/binary input-domain proof with ordinarily returning relevant FP operations, not an engine execution or a demonstrated gameplay trigger. Raw snapshot velocity fields copy full float representations; prediction preserves oldVelocity across its snapshot copy, and Pmove smooths oldVelocity before snapping velocity. Those actual storage roles establish no finite-only invariant. No engine-wide FP exception behavior, whole prediction/movement audit, or runtime reachability is claimed.

The existing MAXSS source correction and approved snap policy are preserved. Both MINSS sites were inspected but left unchanged: a distinct source-only-NaN numerical result was not established under the actual fixed-acceleration/clamped-frametime caller constraints. The ledger still marked this primary equal despite the earlier MAXSS source correction; the exact previous evidence is preserved in the override receipt, and this entry now becomes fixed for one additional three-selector defect group. No unchecked entry closes.

All four actual stock/CoD2x client/dedicated compilations of the changed translation unit passed, at low priority. Exact legacy text and all five actual inactive preprocessing outputs, including assertion-enabled processing, are unchanged. Source hash `f0b3fbd4ff9aff4871c61d8f8e0cb6e2df87fe82bf5238d8a2d41bdbd25f5d68`; receipts in `units/provenance-accel49.compile.json`, independent review in `units/review-provenance-accel49.md`, private evidence in `recon-provenance-accel49.noindex`. Compiled and reviewed, not runtime exercised; no tests, CI, replay, full build, or new tests.
