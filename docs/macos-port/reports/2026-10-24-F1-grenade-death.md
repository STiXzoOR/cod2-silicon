# F1 — held grenade on player death, 2026-10-24

Starting integration commit: `a6bc43a`. This is one focused gameplay fix;
it does not complete the game_mp reconciliation.

## Cause and change

The F0 native crash reaches `player_die` from grenade radius damage, then
faults in `fire_grenade` while reading the weapon definition. The caller
passed remaining fuse time as the weapon index and the offhand index as the
fuse. Correct that argument order for `COD2_X64` only.

Independently checked the Mac 1.3 `player_die` symbol at `0x1c4a7c`:
instructions `0x1c4dd3–0x1c4de0` place client+`0x3c` (remaining fuse) in the
fifth argument and client+`0xd0` (offhand index) in the fourth before the
`fire_grenade` call at `0x1c4df5`. The existing weapon throw caller and
`fire_grenade` signature agree. No reference code was copied.

## Regression and checks

Added `tests/lp64/game/grenade_death.c` to the existing game fixture runner.
It executes production `player_die` with synthetic services and independently
chosen weapon index 3 / remaining fuse 2750, checks the dropped grenade's
arguments and launch vectors, and checks both held and unarmed death paths.
Assertions are explicitly enabled. Before the fix it aborts at the
weapon/fuse assertion; afterwards it passes in stock and CoD2x under ASan.

Both client/dedicated builds and all 13 game fixtures pass in both modes.
The fixture compile databases retain `-UNDEBUG`. The wave-relative legacy
check reports one source file, ten configurations, zero mismatches. Separate
`unifdef -UCOD2_X64` output for the changed engine file compares byte-identical
with `a6bc43a` using `cmp`; this is not a Linux object/binary comparison.

```sh
for B in build-macos build-macos-codx; do
    taskpolicy -b nice -n 19 cmake --build "$B" \
        --target cod2_macos cod2_macos_ded --parallel 2
    taskpolicy -b nice -n 19 python3 tests/lp64/game/run.py --build "$B"
done
python3 tests/fixes13/legacy.py --base a6bc43a
git diff --check
```

Private evidence: `~/Library/Application Support/CoD2-native-decomp/F1-grenade-death.noindex/`.
The before/after focused checks, complete game suites, builds and legacy
source-view comparison are retained there. No full unrelated merge-matrix
rerun was performed; inherited red gates remain described in the integration
and unit reports.

## Replay limits

Two stock replays of the saved F0 reference commands complete all
2,000 frames (4–2003), and match on frame, relative time, active entities,
quantized entity hash and all emitted player fields. Neither has script
runtime errors. A CoD2x replay also completes all 2,000 frames without script
runtime errors; CoD2x repeatability was not separately measured. Their observed states
include movement, jump, crouch, prone, firing, grenades and damage, but no
death or respawn. Consequently this run establishes workload completion,
not live coverage of the corrected held-death call. The synthetic regression
directly exercises that call. F1a changes precede this patch, so improved
replay completion is not attributed to this patch alone.

Comparison with the reference still fails at frame 4: 205 native active
entities versus 168 reference entities, before any active clients. Reference
parity and the differing combat outcomes remain unresolved.

Replay reproduction (choose a new output and free port for each run):

```sh
E="$HOME/Library/Application Support/CoD2-native-decomp/F1-grenade-death.noindex"
F0="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
    --binary build-macos/cod2_macos_ded --data "$HOME/Games/CoD2" \
    --output "$E/replay-stock-new" --port 29375 --dedicated --players \
    --clients 2 --frames 2000 --timeout 360 \
    --replay "$F0/original-f0c-live-record/usercmds.txt"
python3 tools/parity/compare.py \
    "$E/replay-stock-a/statehash.txt" "$E/replay-stock-b/statehash.txt" \
    --players "$E/replay-stock-a/players.txt" "$E/replay-stock-b/players.txt" \
    --require-clients 2
```
