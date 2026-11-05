# Function reconciliation ledger

`functions.json` contains a function list for every indexed source module.
`modules.csv` gives its four status counts. Independent modules are reconciled in isolated worker worktrees and integrated serially.
`game/g_active_mp`, `game/g_client_mp` and `game/g_combat_mp` are fully reconciled;
their reports preserve the outstanding
whole-project replay differences.

Each row retains the Mac address/symbol and Linux source/line when available.
Only unique names are paired automatically. Cross-module name pairs are
explicitly provisional, and overloads or unresolved identities stay separate.
Every input definition remains represented. Row counts are reconciliation
work items, not a claim that unmatched rows are distinct runtime functions.

The starting index accounts for 7,477 exported Mac functions and four further
STABS functions. The Linux source index contains 4,442 AST-confirmed definitions
after resolving paths and removing repeated header inclusions. All 194 source
and header files were parsed. Diagnostics occurred in 158 files; an independent
ctags name comparison leaves seven conditional zlib candidates, which are
retained in the ledger. Inactive preprocessor variants and overload completeness
still require checking during each module's reconciliation. This is a tracked
baseline, not a claim that parsing proved the complete source universe.

Statuses mean:

- `equal`: the complete function has been reconciled with the authoritative
  behavior, with no required correction.
- `fixed`: the complete function has been reconciled and its required fixes
  implemented and validated.
- `intentional`: a documented Mac/CoD2x/platform difference, or a bundled-library
  or compiler-runtime exclusion explicitly permitted by the plan. An excluded
  upstream function is not being claimed semantically equal.
- `unchecked`: reconciliation or required fixes remain. A known partial fix
  belongs in its evidence and does not by itself close the function.

The initial metadata import does not automatically accept older partial audit
claims. Existing reports and private function tables remain evidence to consume
while completing the active module. This avoids silently treating missing
coverage as completed work, but makes the initial unchecked count conservative.

Use one report per module under `reports/`. Record completed functions, unresolved
differences, the batch's changes, tests and one review. Follow the current plan:
no cross-module sweeps, five-lens cleanup, or separate Claude review per fix.

`tools/fidelity/module_inventory.py` joins private Mac/Linux indexes without
copying code, assembly or assets. It writes a fresh baseline; supply reviewed
status overrides through `--decisions` when regenerating. Generate into a
temporary directory and inspect the diff before replacing the maintained ledger.
Private extraction inputs and scripts are in the machine-local
`CoD2-native-decomp/F-inventory.noindex` evidence directory.

An ETA must state the unchecked count, the measured completion rate and its
coverage limits. The first module reuses previously untimed work, so the
throughput estimate remains provisional; timings for a few small helpers do not establish a
reliable whole-project completion date.

## Current workload and estimate

All of `game/` is reconciled. The ledger currently has 3,585 unchecked entries: 2,808 assigned engine entries and 777 unassigned entries. There are 3,537 equal, 666 fixed and 1,669 intentional entries. Intentional entries include the 1,360 bundled-library exclusions and documented platform or native choices. Module reports describe the evidence and runtime limits.

The original planning assumption of 20–40 entries per active hour still gives
approximately **200–400 active hours**, before final cross-module runtime
acceptance. This remains low confidence, not a calendar promise. The first
module reused substantial previously untimed comparison work, and its latest
batch also needed a local SDK configuration repair, so it does not establish a
clean whole-module throughput measurement. The earlier five-helper sample
(380 seconds) must not be extrapolated to larger state machines. Update this
range as additional complete modules supply meaningful timings and uncertain
identities are resolved.

One local inventory-batch review found two classification defects: botlib had
been excluded despite the plan, and SDK-header provenance incorrectly excluded
game functions. Both are corrected. All botlib rows are engine/unchecked; SDK
header candidates remain unchecked until their symbol identity is established.
The review confirmed source-record accounting and matching CSV/JSON counts,
not parser completeness or all function semantics. No external peer or
five-lens cleanup was run.

The client module completed 11 entries in 0.23 elapsed hours
(47.7 entries per elapsed hour), including parallel comparison,
fixtures, builds, review and replay. This single small module is not a
representative sample of the remaining engine; the planning range above remains
provisional, and elapsed throughput must not be confused with total agent hours.

The combat module completed 13 entries in 0.13 elapsed hours
(101.0 entries per elapsed hour), with two corrections and full
batch acceptance. This extends the small-module sample but does not establish
throughput for the remaining large state machines or unresolved identities.

All five `game/g_debug_mp` entries are intentional isolated Linux debug-helper
omissions. This classification-only batch is excluded from semantic correction
throughput estimates; unchanged engine acceptance was reused.

All 55 `game/g_hudelem_mp` entries are reconciled: 41 equal and 14 fixed.
Four shared corrections, both-mode production fixtures, integrated checks and
one review completed in 0.13 elapsed hours (411.7 entries/hour).
Many entries are short wrappers or share inline helpers; this rate is not
representative of the remaining large engine modules. Its report records the
unchanged replay differences and absence of visual HUD acceptance.

All 29 `game/g_items_mp` entries are reconciled: 21 equal and 8 fixed, in
0.18 elapsed hours (157.6 entries/hour). Both native replay modes agree.
Player records remain unchanged; corpse/item settling changes state hashes from
frame 499, documented with final entity evidence in the module report. Original
parity differences remain. The growing sample still mixes small helpers and
large inlined functions; indexing ambiguity and final runtime acceptance remain
outside a reliable calendar estimate.

All five `game/g_load_utils` entries are reconciled: three equal and two fixed.
Only fatal-error diagnostics changed; both-mode fixtures/builds/ABI/legacy checks
and one review pass. The preceding item replay remains the explicitly historical
runtime baseline, with no duplicate replay for unchanged successful behavior.

All 34 `game/g_main_mp` entries are reconciled: 20 equal, 12 fixed and two
intentional differences. Lifecycle, objective, voting, callback and metadata
corrections pass 106 focused outcomes and 15 live transition cases. Both 2,000-frame
replays match the prior native trace; original parity differences remain.
This module took 0.27 elapsed hours (125.8 entries/hour), including parallel
comparison and one review. Its large initializer and frame bodies provide a
broader sample, but the overall planning range remains provisional.

All 28 `game/g_misc_mp` entries are reconciled: 21 equal, six fixed and one
intentional Linux helper omission. Seventy focused outcomes, both builds and
scoped checks pass; both final 2,000-frame replays match the prior native trace.
The batch took 0.18 elapsed hours (158.8 entries/hour). Visual turret alignment
and original cross-module parity remain open; the planning estimate stays provisional.

All 12 `game/g_missile_mp` entries are reconciled: six equal and six fixed. Six new
regressions fail the baseline and pass both builds; both 2,000-frame replays match a
fresh baseline trace, but that recording exercises none of the corrected impact paths.
The batch took 0.55 elapsed hours (21.8 entries/hour), including the local CI run and
replays. The planning estimate stays provisional.

All 12 `game/g_mover_mp` entries are reconciled: ten equal and two fixed. The only new
source change is one fatal diagnostic, so no replay was rerun.

All 242 `game/g_scr_main_mp` entries are reconciled: 225 equal, eleven fixed and six
intentional. Seven regression groups fail the baseline and pass both builds.
Both 2,000-frame replays match the previous trace, so that recording reaches none of the corrected
paths. Time on this batch overlapped other modules, so it gives no throughput figure.

Seven further modules were reconciled in one batch: `game/g_scr_mover_mp`, `game/g_spawn_mp`,
`game/g_trigger_mp`, `game/g_svcmds_mp`, `game/g_team_mp`, `game/scr_const_mp` and `game/player_use_mp`.
Both 2,000-frame replays agree with each other and move closer to the original Mac recording. Against the original, the differences in `viewangles`, `pm_flags` and `pm_time` fell from 1,228, 80 and 104 to zero, `velocity` fell from 1,313 to 1,257, and `origin` fell from 3,786 to 3,781. The first player difference stays at frame 102, a last-bit float in `velocity[1]`, and the first state difference stays at frame 116. The trace first differs from the previous commit at frame 648. Which fix moved these counts was not isolated.
The batch's time overlapped other modules, so it gives no throughput figure.

From this batch on, new per-fix regression tests are no longer written. Each batch is compared with the
binary, built, run through the existing fixtures and the replay, and reviewed once. A fix on a path the
replay does not reach is then only compiled and reviewed, and its report says so.

The last six `game/` modules were reconciled in one batch: `game/g_client_fields_mp`, `game/g_weapon_load_obj`,
`game/g_weapon_mp`, `game/g_cmds_mp`, `game/g_utils_mp` and `game/g_client_script_cmd_mp`.
Both 2,000-frame replays match the trace from the previous commit, and stock matches CoD2x, so that recording reaches none of the corrected paths. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, with the first player difference at frame 102 and the first state difference at frame 116.
Reconciling a module does not make it verified at run time: most fixes in this batch were only compiled and reviewed.

`bgame/bg_slidemove` and `bgame/bg_jump` are reconciled: 16 equal and two fixed. `bgame/bg_pmove`, `bgame/bg_misc` and `bgame/bg_mantle` are also
reconciled: 81 equal and six fixed. Both 2,000-frame replays match the trace from the previous commit, so that recording reaches neither correction. Original parity is unchanged: 3,781 `origin` and 1,257 `velocity` differences, first at frame 102.

`qcommon/cm_mesh`, `qcommon/cm_test`, `qcommon/cm_tracebox` and `qcommon/cm_trace` are reconciled: 56 equal and four
fixed. Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the `original` recording, the counts stay at 3,781 `origin` and 1,257 `velocity` differences, first at frame 102. That recording was made by the Linux dedicated server in the oracle container, not by the Mac binary, and its first differences are platform differences rather than defects. A model of frame 102 using x87 80-bit arithmetic reproduces the recording bit for bit on both clients, and a model using the SSE steps in the Mac code reproduces the native trace bit for bit. At frame 116, Linux `Sys_SnapVector` uses `rint` where Mac 0xaed28 truncates. Native follows Mac, and the velocity differs by exactly 1.0 on those axes.

Both 2,000-frame replays match the trace from the previous commit, so that recording reaches none of these corrections. Against the Linux `original` recording, no logic field differs in any of the 3,806 player records: `pm_type`, `pm_flags`, `pm_time`, `weapon`, `weaponstate` and `health` are all identical, and so are the view angles. As a temporary experiment, not committed, `Sys_SnapVector` was switched to the Linux `rint`. The largest origin difference fell from 14.5 to 0.118 units and the largest velocity difference from 153 to 2, with 3,738 of the 3,806 records within 0.001 units. That shows nearly all the drift comes from that one platform difference. Native keeps the Mac truncation.

`Sys_SnapVector` deliberately differs from Mac. Mac truncates velocity, while Windows and Linux round to nearest. Native
follows Windows, by the project owner's decision, because most players are there. This is the one place so far where
Mac is not the authority: when Mac behaviour looks platform-specific and players would notice, check the Windows binary.

The first orchestrated batch reconciled 193 further entries across weapon loading, animation and weapons. Six animation identities remain unchecked. Integrated source `e92ca5f` passed both dedicated builds, local macOS CI steps 02–13, the scoped legacy guard check and independent review. Both 2,000-frame replays are unchanged from the fresh `d873794` baseline; no logic-field regressions were observed. The module reports distinguish static review from exercised behavior.

The second orchestrated batch closed 302 further entries across collision loading/world, commands, startup, messages and networking. Source `12d554b` passed both dedicated builds, local macOS CI 02–13, scoped legacy checks and independent reviews. Both replays exactly match the fresh baseline. Hosted ILP32 byte comparison remains unavailable because its unchanged `410342a` reference fails to compile; see the workflow's existing non-blocking exception.

The third orchestrated batch closed 261 further entries across map support, profiles/file commands and server modules. Engine source `868a08f`, with recording prerequisite `b275adb` and existing fixture correction `b1f110d`, passed both dedicated builds, local macOS CI 02–13, scoped legacy checks and independent reviews. Both final replays exactly match the fresh baseline; the initial missing-profile recording failure and source-fixture failure are preserved. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile.

The fourth orchestrated batch closed 563 entries across utilities and core script modules. Source `dd6a421` passed both dedicated builds, all local macOS CI 02–13, native-only source guards and independent reviews. Both replays exactly match the fresh baseline. The review-discovered MSVC temporary-allocation issue was corrected before integration; generic VM recovery and other unresolved identities remain unchecked. Hosted ILP32 byte comparison remains unavailable because its unchanged reference does not compile.

The fifth orchestrated batch closed 494 entries across parser/lexer, DObj, animation, model loading, filesystem and memory/helpers. Source `8f74202` with evidence `03ebea4` passed both dedicated builds, all local macOS CI 02–13, native-only source guards and independent reviews. Both replays exactly match the fresh baseline. Model diagnostic/truncated-record findings and legacy allocator line-number drift were corrected and re-reviewed before integration; actual OFF line expansions were checked separately. The initial source-fixture run failed on a missing download-fixture dependency; fixture-only67e46a9 was reviewed and the complete source suite passed on rerun. Hosted ILP32 byte comparison remains unavailable because its unchanged reference does not compile.

Thirty-two whole-caller rows previously attributed to universal headers were routed to their actual engine modules. IDs, original Mac source-module provenance and unchecked verdicts are preserved; global status counts are unchanged. Shared-helper comparison does not certify these larger callers.

Five routed Mac rows were subsequently resolved as exact identities of complete functions already audited under their Linux-indexed rows: four collision-tree traversals and Dvar_StringToColor. Both IDs are retained. This closes five work items without claiming five newly audited implementations.

The sixth orchestrated batch closed 548 work items: 543 from math/dvars/text/memory-file/sound/input audits and five exact duplicate identities after caller routing. Source `7e8d818` with evidence `2ee4eb9` passed both dedicated builds, full local CI 02–13, nine-file native-only source guards and independent reviews. Both replays match both baselines exactly. An MSVC portability finding was corrected before integration; MSVC itself and unreached client paths remain unexercised. Existing source fixture coverage passed, with no new tests.

The seventh orchestrated batch closed 308 client entries. Source `3ee60be` passed both dedicated builds, full local CI 02–13, native-only source guards, independent reviews and both exact-baseline replays. Reviews corrected cinematic initial state and console autocomplete bounds; full client links verified the cross-unit cinematic callback. The catalog coverage failure was corrected by regenerating command metadata; its complete CI step and existing Swift decoder fixture passed after updating the revision expectation. Original failed logs remain preserved. Existing fixtures were corrected without adding tests. GPU synchronization and its whole frame caller remain unchecked for the renderer audit; unreached client paths remain compiled and reviewed.

The eighth orchestrated batch closed 173 sound/cgame entries. Source a90c5b7 passed both dedicated builds, full local CI 02–13, native-only production source guards, independent reviews and both exact-baseline replays. Reviews corrected Windows fallback-label handling and scoreboard arithmetic overflow. Catalog regeneration reflects removed cgame commands/settings while retaining server tell; deterministic output, coverage and the existing decoder checks passed. Existing audio/HUD fixture setup was corrected without adding tests. Unreached client-rendering paths remain compiled and reviewed; 31 synthetic sound identities remain unchecked.

The ninth orchestrated batch closed 150 cgame/effects entries. Source cef44cb passed both dedicated builds, full local CI 02–13, native-only production source guards, parent/independent reviews and both exact-baseline replays. Reviews corrected HUD fading and a weapon diagnostic; nineteen effects identities or malformed-input boundaries remain unchecked. Existing owner-draw fixture expectations were corrected without adding cases. Unreached client rendering remains compiled and reviewed.

The tenth orchestrated batch closed 566 effects/UI-support entries. Source 5bf663a passed both dedicated builds, full local CI 02–13, native-only production guards with actual assertion line maps, parent/independent reviews and exact-baseline replays. Combined existing effects sanitizer/parser fixtures also passed. The final stage initially failed an existing fixture logging dependency; its one-line stub correction passed both modes, independent review and full stage retry. Twenty-five effects entries remain unchecked; archive streams and unreached client rendering remain compiled and reviewed.
