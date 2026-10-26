# Spectator vertical movement arithmetic — 2026-10-26

Base: `5ce6fb0`. This is a bounded continuation of the faithful-decompilation
plan, not completion of shared movement reconciliation or replay parity.

## Repair

Mac 1.3 `Pmove` inlines spectator vertical scaling at `0x71afb–0x71b7a`.
It multiplies the integer speed by `127.0f`, divides by `16129.0f`, applies the
walking/lean and spectator modifiers, then multiplies by `-127.0f` or `127.0f`
before adding to the requested vertical velocity. Those scalar SSE operations
round individually. Native `PM_FlyMove` had algebraically cancelled the factors,
changing the resulting binary32 value.

The repair restores the operation sequence inside the existing native guard.
For speed 400, spectator scale 1.3 and walking or lean, the upward request is
`434fffff` (207.99998474121094), rather than `43500000` (208). The ordinary
acceleration/normalization chain can amplify this to a two-ULP velocity
difference. Horizontal math, snapping and replay comparison remain unchanged.

The relevant original instructions and constants were checked against the
owned Mac binary. The complete exported instruction lists for `Pmove`,
`PM_Accelerate`, `PM_Friction`, `Vec3Normalize` and `AngleVectors` contain
2,746 instructions / 12,063 bytes, with zero byte mismatches. Source comparison
covered the spectator branch and those four helper bodies, not every Pmove
branch. No private original source or assembly was added to Git.

## Verification

The existing movement harness compiles the complete production `bg_pmove.c`
and links real `com_math.c`. A new included fixture exercises real
`PM_FlyMove`, friction, normalization and acceleration with exact literal
binary32 expectations derived independently from the Mac instruction sequence.
Its slide service performs a simple integration; it does not prove collision
or real-map behavior for vertical spectator motion.

Before the engine edit, stock and CoD2x each fail six of eleven vertical cases:
up/down with walking, positive/negative lean, and combined horizontal/vertical
commands. Five controls pass: no modifier, default spectator scale, opposing
vertical buttons, zero speed and a lower speed. Both unchanged horizontal
controls pass. After the repair, all thirteen cases pass in both configurations.
The full movement revision matrices also pass, including the existing collision,
stance, event and sway checks. No existing expectation or tolerance was weakened.

Commands run from the isolated worktree, with one compiler at a time:

```sh
taskpolicy -b nice -n 19 python3 tests/movement/run.py \
  --compile-db <integration>/build-macos/compile_commands.engine.json \
  --output <private>/full-stock --revision-matrix
taskpolicy -b nice -n 19 python3 tests/movement/run.py \
  --compile-db <integration>/build-macos-codx/compile_commands.engine.json \
  --output <private>/full-codx --revision-matrix
python3 tests/fixes13/legacy.py --base 5ce6fb0
git diff --check
```

Both matrices use revisions 0, 3, 4, 6 and the unknown-revision fallback. The
engine file's exact `unifdef -UCOD2_X64` bytes match the base; all five inactive
guard configurations and ten fixes13 configurations match. These checks do not
establish Linux object identity. The two new groups are part of the movement runner's default selection.
The current CI workflow does not invoke that runner directly; the integrator
should register its two focused spectator selections for both configurations.

Evidence is private under
`~/Library/Application Support/CoD2-native-decomp/F1-spectator-math.noindex/`:
`baseline-{stock,codx}.log`, `fixed-stock.log`, `full-{stock,codx}.log`,
`vertical-expectations.{py,json}`, and the exact OFF projections.

## Separate first-replay-difference finding

The saved first mismatching spectator commands have no vertical buttons, so
this repair cannot explain frame 102. A private calculation using the recorded
yaw/command values and source defaults reproduces both native velocity vectors
with binary32 rounding after every Mac-style operation. Widening only the
horizontal wish-vector expression's intermediates before its final float store
reproduces both original Linux velocity vectors. Current host float trig and
double trig rounded to float agree for these two angles.

This narrows the next diagnostic to the original wish-vector arithmetic, but
it is not an observation of original intermediates or an x87 emulator. Actual
first-command dvars, old velocity and collision inputs were not recorded.
No native horizontal precision, oracle, snap policy or comparator tolerance
was changed. Original guest access remains pending. The first normalized entity
mismatch at frame 116 and later damage disagreement remain unexplained.

No game, VM, commit or push was performed for this unit. The vertical correction
is separate from the accelerated timedemo playback the user noticed.

## Integration and completed review

Focused review `20261026-125550-4aa67339` and all five simplification rubrics
completed with no retained defect or edit. The independent reviewer verified
the Mac instruction sequence and constants, exact OFF source, and both focused
revision matrices. The Claude peer was confirmed as Opus 5.5; high effort was
requested, while actual effort was unverified.

Coverage remains bounded: the new vertical cases start with zero vertical
wish velocity and set walking/lean modifiers directly. Pitched forward input
and full Pmove reachability of those modifiers are not established by these
cases. No reviewed incorrect operation ordering was found.

The shared CI update registers both spectator selections in stock and CoD2x.

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
