# Animation clocks and synchronized script notifications

A bounded audit of downstream animation clocks found no additional global speed
multiplier in the inspected paths. It did find a separate notification defect:
synchronized immediate children were advanced to the correct animation time,
but their script notifications were checked against their previous time. The
native implementation now supplies the synchronized parent's newly advanced
time to the existing notification consumer. This is not an explanation for
accelerated timedemo playback, and does not change animation speed.

## Original evidence and scope

Authority is the owned Mac 1.3 i386 binary. `XAnimUpdateInfoInternal` at
`0x419a2` first stores the parent's new normalized time at `0x41c34`. In its
synchronized-child branch, `0x41e17` loads that parent time from `[ESI]` into
XMM0, then `0x41e23` calls `XAnimProcessServerNotify`. The child is advanced
only afterward, at `0x41d05`. The reconstruction instead passed the child's
still-old time. `XAnimProcessServerNotify` at `0x3ebbe` reads the old time from
the child state and compares it with the supplied new time to detect authored
notetrack crossings, loop wraps and animation completion. Passing identical
old/new times suppresses those crossings.

The real server route is `G_RunFrame` -> `SV_DObjInitServerTime` ->
`DObjInitServerTime`, followed by `SV_DObjUpdateServerTime` ->
`DObjUpdateServerInfo`. The latter finds the next notification boundary and
may advance multiple slices before completing the frame. The repair changes
only the argument at the synchronized-child notification call. It is guarded
by `COD2_X64`; the legacy statement remains exact. No layouts, shared headers,
clock dvars, animation rates or notification scheduling code change.

The following bounded comparisons did not find another speed-factor defect:

- `XAnimUpdateInfoInternal`: primitive advancement is rate times frequency,
  then elapsed seconds; unsynchronized parents multiply elapsed seconds by
  their rate once; synchronized parents average weighted child frequencies,
  apply their own rate, then elapsed seconds. Loop/clamp and current-cycle
  behavior in the inspected paths correspond to the original instructions.
- `XAnimUpdateOldTime` at `0x3b1b0`: valid finite blend timing uses elapsed
  seconds, preserves the original multiply/divide order, and saves the old
  time once before later advancement. `XAnimGetAverageRateFrequency` at
  `0x3da06` recursively combines nonzero child weights/frequencies and rates.
- `BG_RunLerpFrameRate` at `0x183492`: the locomotion rate uses displacement
  over snapshot elapsed seconds divided by animation move speed. It holds
  the rate between snapshots and retains the original speed-dependent caps.
- `CG_UpdateViewWeaponAnim` at `0x1dd3fe` advances its DObj once using signed
  `cg->frametime * 0.001f`. The inspected rate assignments within
  `CG_RegisterWeapon` at `0x1d85cc` use animation length in milliseconds divided
  by the corresponding weapon duration, with zero for nonpositive durations.
  `XAnimGetLength` and `XAnimGetLengthMsec` use frame count divided by framerate;
  the latter multiplies by 1000 and truncates.

Ten referenced complete exported bodies were checked against the actual Mach-O
instruction bytes: 3,606 instructions / 14,992 bytes, zero mismatches. This
validates the inspected exports, not a claim that every instruction or every
animation function has been reconciled. The semantic inspection is bounded to
clock advancement, rate inputs and the notification handoff; unrelated weapon
registration/rendering, malformed data and nonfinite values remain outside it.
The existing active-frame/body-wrapper audit was reused rather than repeated.
Private evidence is under
`~/Library/Application Support/CoD2-native-decomp/F2-animation-clock.noindex/`.
No original disassembly or reference implementation is included in this change.

## Verification

The existing `tests/xanim/fidelity.c` covers direct notification consumption,
notification fractions and leaf timing, but does not execute synchronized
immediate-child advancement through the server DObj entry points. The new
`tests/xanim/sync_notify.c` includes the complete production `xanim.c` and
executes real `DObjInitServerTime`, `DObjUpdateServerInfo`, old-time preparation,
rate aggregation, boundary selection and the real notification consumer.
Synthetic trees/clips supply inputs; only string services and script emission
are controlled boundaries. Assertions inspect emitted script names and final
animation times/cycles, not calls to the repaired helper.

All seven groups pass in both stock and CoD2x with assertions, ASan and UBSan:

- crossing a notification between normalized times 0.25 and 0.75;
- crossing the loop tail, end and prefix in order, ending at 0.25/cycle 1;
- reaching a non-looping endpoint, including its authored notification and the
  separate script end event from the final server slice;
- notification-disabled, no server entity and zero-goal-weight controls;
- unsynchronized children and a nested synchronized descendant as controls;
- non-unit parent rate, child rate and clip frequency, with the expected final
  time and one script notification.

Before production was edited, both configurations failed crossing, wrapping,
ending and rate, while the three controls passed. The first endpoint assertion
omitted the separate script end event produced when the final slice starts at
time 1. A private diagnostic observed the authored note followed by `g_end`;
the original consumer's time-1 branch confirms that contract. That single proof
expectation was corrected before freezing the fixture. With the final fixture
and `--baseline 5ce6fb0`, the same four failures and three passing controls are
reproduced in both modes. The failed first expectation and private diagnostic
remain in evidence; no production change was added to accommodate it.

Both existing full xanim suites also pass unchanged. Three existing narrowing
warnings in unrelated xanim string allocation remain visible in compile logs.
The new fixture does not disable shift sanitization; the older suite retains
its existing shift-sanitizer exception. Exact `unifdef -UCOD2_X64` output is
identical to `5ce6fb0` using `cmp`; the legacy checker reports one engine file,
five configurations, zero mismatches. This is source preservation, not the
unavailable original Linux i386 object-equivalence check.

Commands, run serially from the isolated worktree:

```sh
integration=/Users/stix/.codex/worktrees/metal-u1-integration/cod2-native
evidence="$HOME/Library/Application Support/CoD2-native-decomp/F2-animation-clock.noindex"
for build in build-macos build-macos-codx; do
  taskpolicy -b nice -n 19 python3 tests/xanim/run_sync_notify.py \
    --compile-db "$integration/$build/compile_commands.engine.json" \
    --output "$evidence/reproduce-$build"
  taskpolicy -b nice -n 19 python3 tests/xanim/run_sync_notify.py \
    --compile-db "$integration/$build/compile_commands.engine.json" \
    --output "$evidence/reproduce-historical-$build" --baseline 5ce6fb0
  taskpolicy -b nice -n 19 python3 tests/xanim/run.py \
    --compile-db "$integration/$build/compile_commands.engine.json" \
    --output "$evidence/reproduce-existing-$build"
done
python3 tools/abi/legacy.py --base 5ce6fb0
git diff --check
```

Historical commands intentionally exit 1. Actual command results are in
`verification-results.json`, with case-level results in each output directory.
No game, VM, network replay, compile of the entire engine, commit or push was
performed by this worker. The integration owner owns CI registration, linked
builds and review. Synthetic script sinks do not prove script-VM effects or
asset-backed end-to-end animation parity. Full F0/F1/F2 acceptance remains open.

## Integration and completed review

Focused review `20261026-125600-422bb8a2` and all five simplification rubrics
completed with no retained defect or edit. The reviewer independently checked
590 original caller/consumer instructions and ran the stock fixture. The
Claude peer was confirmed as Opus 5.5; high effort was requested, while actual
effort was unverified.

The nested case is an unchanged-path control. Immediate child blend nodes
using notifyChild, and parent/child old-time reset interactions, are not
covered by the new cases. Actual script-VM effects and full asset-backed
animation parity remain open.

The shared CI update registers this runner in stock and CoD2x, together with
the preceding spectator regression selections. All four registered command
forms were executed locally with private output paths and passed.

The integration owner reran the focused fixtures in both configurations using
the engine compile databases; all passed with ASan/UBSan. Stock and CoD2x
clients and dedicated servers linked successfully. Exact OFF projections for
both engine files match `5ce6fb0`; the integrated legacy checks report zero
mismatches across five general and ten fixes13 configurations.

Fresh ordinary live-map runs walked, fired and reloaded, produced three captures
each, and exited cleanly. Stock advanced 8.6 simulation seconds over 8.608239
wall seconds; CoD2x advanced 8.9 over 8.883682. Both measured horizontal speed
189. These runs check live stability and clock pace, not original-server replay
parity or authored notification semantics. Evidence is under
`~/Library/Application Support/CoD2-native-decomp/F2-wave14.noindex/`.

The completed integrated function ABI audits report 634 stock and 651 CoD2x
translation units, with zero errors, mismatches or new baseline entries.
Unchanged import/callback/raw-offset checks retain the preceding wave13
evidence; these two repairs change neither declarations nor import/layout use.
