# F2: snapshot processing during cubemap capture

The native `CG_ProcessSnapshots` now follows the Mac 1.3 capture branch and
its surrounding snapshot-read control flow. The original renderer's
`renderScreen == 0` behavior is unchanged. This unit does not make cubemap
commands render the world.

## Mac evidence and changes

Read all 485 instructions of `CG_ProcessSnapshots` at `0x1df762–0x1dfe92`
and compared their bytes with the owned Mac binary. The private instruction
export matches all 1,841 bytes. Symbol addresses were checked with `nm`.
The changes were independently written from those instructions; no reference
source or decompiler text was copied.

- At `0x1dfa37–0x1dfa41`, distinct `nextSnap` and `snap` enter the ordinary
  timing branch only when `cg + 0xc` (`cubemapShot`) is zero. Capture frames
  instead attempt to read another snapshot. With no unread snapshots, they
  retain the current snapshot even when its pending successor is due.
- A successful read followed by `CG_SetNextSnap` jumps directly from
  `0x1dfcb8` to the timing block at `0x1dfaf4`. A newly read snapshot still
  undergoes timing checks during capture; adding only a capture condition to
  the old loop would incorrectly suppress this transition.
- The active read path at `0x1dfc86–0x1dfc9f` compares restart flags and server
  time against `nextSnap`, not `snap`. It does not filter `snapFlags & 2`.
  The initial-snapshot path retains its separate inactive-snapshot filter at
  `0x1df86a`. Native processing now preserves that distinction.
- Transition continuations at `0x1dfc61`, `0x1dfc71`, `0x1dfe4b` and
  `0x1dfe58` go to `0x1dfa32`, below the frame-interpolation call at
  `0x1dfa22`. Native processing no longer repeats that call after ordinary
  or restart transitions. Repeating it with equal snapshot pointers can
  overwrite the previously computed interpolation with zero.

The two active snapshot buffers matter here: a capture read can overwrite
`nextSnap` before the restart/time comparison. The fixture uses those real
buffers and preserves this original behavior, including an older timestamp
or changed restart bit in the replacement buffer. It does not add a new
snapshot-validation policy.

The surrounding initial-snapshot setup, latest-number update/error,
lagometer/read retry, no-read time clamp, entity/client transition loops and
return paths were inspected in the same function. No additional change was
identified for ordinary valid snapshot states. This is a bounded source
reconciliation, not a claim of complete cgame or renderer parity.

## Verification

The existing snapshot runner compiles the production snapshot and playerstate
translation units with real engine flags and ASan/UBSan. Thirteen new process
cases cover normal timing, all six capture faces, incoming due/future
snapshots, active inactive-bit acceptance, restart behavior, backwards-time
errors and replacement-buffer aliasing. They assert snapshot identity,
processed number, reads, interpolation calls, solid-list updates and times.
The existing twelve transition/corpse cases remain included.

Both stock and CoD2x configurations pass **25/25 case groups**. Running the
same final fixtures against baseline `7e1e610` fails **10 groups** and passes
15 controls/previous groups in each configuration. Before the wider
whole-function reconciliation, the initial capture-only fixture also failed
both due/before-current capture cases on the baseline and passed after the
capture gate correction; those logs remain private.

Commands (run from this worktree; `integration` is the canonical integration
checkout, and `evidence` is the private directory below):

```sh
taskpolicy -b nice -n 19 python3 tests/camera/run_snapshot.py \
  --compile-db "$integration/build-macos/compile_commands.engine.json" \
  --output "$evidence/final-stock"
taskpolicy -b nice -n 19 python3 tests/camera/run_snapshot.py \
  --compile-db "$integration/build-macos-codx/compile_commands.engine.json" \
  --output "$evidence/final-codx"
# Repeat each with --baseline 7e1e610 and a distinct output directory.
taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base 7e1e610
git diff --check
```

For the one changed engine file, `unifdef -UCOD2_X64` projections of the
baseline and current source compare byte-for-byte with `cmp`. All ten legacy
preprocessor configurations also match. These are source-preservation checks;
they do not establish Linux i386 object identity.

Private evidence:
`~/Library/Application Support/CoD2-native-decomp/F2-snapshot-cubemap.noindex/`
contains instruction verification, baseline/final logs, per-case sanitizer
logs, `results.json`, OFF projections and the legacy check.

No game, VM, network, full linked build, SDK ABI audit or original-runtime
comparison was run by this unit. Rendering, dropped-snapshot retries and
initial activation are not newly exercised end to end. Corrupt pointers and
signed-time overflow are not covered by the synthetic cases. Root integration
owns the remaining build/review checks. No benchmark or oracle was changed.

## Integration and review

All five independent simplification lenses found no worthwhile change; proof
tests stayed unchanged. The completed local correctness and Claude review
reported zero actionable findings. The local pass checked the private Mac
instructions; the external peer could not access them. Synthetic coverage
permits only one successful read per invocation and counts interpolation calls,
so consecutive due snapshots, dropped-read retries and numeric interpolation
remain outside that fixture's proof.

Root integration reran all 25 sanitizer groups in both configurations and
built both clients and dedicated servers successfully. Fresh ordinary demo
playback completed 684 stock and 685 CoD2x frames and exited normally. Fresh
live cubemap commands in both clients produced six reflection and six lighting
files, exited normally, and allowed subsequent world rendering. Reflection
faces remain magenta because the original command passes `renderScreen = 0`,
as documented in the cubemap report; these runs do not establish original
renderer pixel parity. Runs used private homes, the exclusive game lease and
a suspended VM, which was restored afterward.

Private integration logs are `integration-results.json`, `live-demo-results.json`
and `live-results.json` under the evidence directory above. No replay oracle
or comparator was changed.
