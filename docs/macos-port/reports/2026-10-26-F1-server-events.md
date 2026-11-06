# F1 — Mac 1.3 server player-event dispatch

The native `ClientEvents` dispatcher used several incorrect event selectors.
Offhand-use event `0xa6` was ignored, while selection event `0xa7` threw the
offhand weapon. Melee, binocular notifications and two weapon-fire selectors
were also wrong. Falling damage used current health instead of maximum health.
This unit repairs those choices inside `ClientEvents`; no downstream weapon,
script or damage implementation changes.

## Reference and changes

Authority: owned Mac MP 1.3 i386 `ClientEvents` at `0x1a30ec`. All 136 exported
instructions (634 bytes) were compared with the actual Mach-O bytes, with zero
mismatches. The 40-entry jump table was read directly from virtual address
`0x33be00`; the dispatcher subtracts `0x9e`, bounds the result at `0x27`, then
loads that table. Named destination symbols were checked against `nm`.

| Behavior | Mac event IDs | Previous native event IDs |
| --- | --- | --- |
| Fire weapon | `9e`, `9f`, `a0`, `af` | `9e`, `9f`, `a0`, `a1`, `b0` |
| Melee | `a4` | `a5` |
| Use offhand | `a6` | `a7` |
| Binocular enter/exit/fire/release/drop | `a8`–`ac` | `a9`–`ad` |
| Forced death | `c5` | `c5` (unchanged) |

All other IDs in the table continue without action. Binocular destination
blocks load `scr_const` offsets `0x62`, `0x64`, `0x66`, `0x68`, `0x6a`, matching
the five named notification fields. Fire dispatch retains the original
anti-lag choice: `lastServerTime` when enabled, otherwise `level.time`.

The falling branch covers event IDs `0x74`–`0x8a`. At `0x1a3168`, Mac loads
`playerState` offset `0x134`, maximum-health `stats[2]`; the native code loaded
current-health `stats[0]`. Both percentage and greater-than-99 branches now
use the correct field. The constants, multiply/truncate order, zero-damage
skip, and nonplayer early exit are unchanged. Forced death still clears
current health (`stats[0]`, Mac offset `0x12c`) and entity health before
calling `player_die`, unless entity flag bits 0 or 1 prohibit it.

No `ClientEvents`, offhand or melee dispatcher override was found in the
CoD2x `src/shared` and `src/mss32` implementation sources. Both build modes
use this Mac event contract. Changes are guarded by `COD2_X64`.

## Proof-first fixture

`tests/lp64/game/client_events.py` compiles the full production
`g_active_mp.c` translation unit using the selected engine compile database,
plus the new fixture, with assertions, ASan and UBSan enabled. Only external
weapon/script/damage services are replaced by recording sinks. The sinks
check entity identity and all passed arguments; forced-death checks observe
health already cleared at the service boundary.

Independent literal inputs cover every selector in the 40-entry table,
out-of-range controls, all 23 falling-event IDs, both anti-lag clock choices,
five distinct binocular notification names, protected/unprotected forced
death, ring clamping and wrapping, an already-consumed ring, and an old
sequence beyond the current sequence. Falling cases deliberately use current
health 37 and maximum health 200, expecting 50 at 25 percent and 220 for the
fatal branch. Zero fall damage permits the following event; a nonplayer fall
terminates processing before that event.

Before production edits, revision `18cf588` failed eight groups in each
build: fire, melee, offhand, binocular, inactive, fall, fall-fatal and ring.
Suicide and fall-controls passed. The proof fixtures were then held unchanged.
After the repair all ten groups pass in both stock and CoD2x with ASan/UBSan.

Commands from this worktree (the runner rebases database source paths):

```sh
integration=/Users/stix/.codex/worktrees/metal-u1-integration/cod2-native
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F1-server-events.noindex"
for mode in stock codx; do
  build=build-macos
  if [ "$mode" = codx ]; then build=build-macos-codx; fi
  taskpolicy -b nice -n 19 python3 tests/lp64/game/client_events.py \
    --compile-db "$integration/$build/compile_commands.engine.json" \
    --output "$evidence/baseline-$mode" --baseline 18cf588
  # Baseline intentionally exits nonzero; run the following command separately.
  taskpolicy -b nice -n 19 python3 tests/lp64/game/client_events.py \
    --compile-db "$integration/$build/compile_commands.engine.json" \
    --output "$evidence/final-$mode"
done
```

The runner requires fresh output directories; the named evidence already
exists. For repeat runs select new output paths. Compilation was serialized
under `taskpolicy -b nice -n 19`, using one shared compiler slot.

## Preservation and limits

- Exact `unifdef -UCOD2_X64` output compared by `cmp` against `18cf588`:
  45,738 bytes unchanged for `g_active_mp.c`.
- Five inactive legacy preprocessing configurations: zero mismatches.
- `python3 tests/fixes13/legacy.py --base 18cf588`: ten configurations,
  zero mismatches.
- `python3 tests/lp64/game/check_legacy.py --base 18cf588`: ten inactive
  patch configurations, zero mismatches.
- `git diff --check`: passed.

These are source-preservation checks, not i386 object-parity proof. Existing
legacy ABI differences, inactive CoD2x differences and unavailable upstream
x86 object comparison are not changed or relabeled by this unit.

No game, VM, full engine build, or replay was run by this worker. The fixtures
verify dispatch and damage arguments, not actual grenade trajectories,
script execution or health loss downstream of the damage service. They do
not establish that this repair explains the replay's 54-point health gap,
and do not establish replay parity. Event mutation by service callbacks and
extreme overflowing event-sequence arithmetic are not exercised.

Private evidence lives under `F1-server-events.noindex`. No original binary,
disassembly, game assets or reference source is added to Git. Integration,
CI registration and shipping review belong to the orchestrator.

## Integrated evidence

Completed local and independent Claude review `20261026-132535-5910f087`
retained no actionable findings. The reviewer independently checked the
original instructions, full jump table, native field offsets and both
sanitized fixtures. All five simplification rubrics completed without edits;
two proof-fixture comments were intentionally retained. Claude Opus 5.5 was
confirmed, high effort requested and actual effort unverified. Direct tests
do not cover the unchanged 99-percent boundary or negative event parameters.

The integration owner reran all ten fixture groups in each configuration and
linked both clients and both dedicated servers. Function ABI checks passed
634 stock and 651 CoD2x translation units with zero errors or mismatches. Exact
OFF source remains 45,738 bytes. General legacy, fixes13 and game inactive
checks pass (five, ten and ten configurations respectively) against `dfc0ddc`.
An earlier aggregate legacy invocation against `18cf588` reported five
differences solely in the separately committed gamma diagnostic fixture; no
engine projection differed. Its failed log remains in private evidence.

The shared CI update registers this fixture for stock and CoD2x. No oracle,
comparator tolerance, reference trace or recording workload was changed.

A fresh stock baseline at `18cf588` and the repaired executable each completed
the same original 2,000-frame command stream. The baseline reproduced extra
entities at frame 280 (172 versus the reference's 170), followed by client 0
health 46 versus 100 at frame 349. With this repair, active-entity counts,
health, weapon and weaponstate fields match the reference throughout all
2,000 records. Frame 349 health is 100 and the unwanted impulse is absent.
The first before/after entity-count difference is frame 280; the first
before/after recorded player-field difference is frame 349.

This establishes that the bounded ClientEvents repair removes the observed
premature-entity and health divergence for this corpus. It does not establish
all grenade trajectories or full multiplayer parity. The unchanged comparator
still fails: raw player velocity first differs from reference at frame 102,
and normalized entity hash at frame 116. Other remaining first differences
include pm_type at frame 499 (reference 6, native 7), pm_flags at 652,
viewangles at 776 and pm_time at 1803. These remain open, not hidden by the
improved entity-count and health agreement.

Private integration commands, build/ABI logs, complete traces and
`comparison.json` are in `F1-server-events-integration.noindex`.

A second repaired stock replay matches the first on all 2,000 records using
the unchanged comparator (frame, simulation time, active-entity count, q hash
and every emitted player field). CoD2x also completed 2,000 valid frames with
two clients at the end and no script-runtime-error marker. Stock before/after
and reference metadata agree on all 28 IWD identities and seed 12345. The
complete records span frames 4–2003. Each run used a private home/port, held
the exclusive lease and restored the VM to its prior started state.
