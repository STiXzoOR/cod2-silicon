# F1c live-player investigation — 2026-10-24

The apparent parser regression was a test-setup failure exposed by corrected
notification handling. Preserve F1c's lexer/compiler fixes. No additional
engine change or parser rollback was needed to run valid weapon selections.

## Diagnosis

The isolated branch starts at WIP `db07fc3` and incorporates integration
`3de7fcc` (including F1a and the held-grenade death fix). The original WIP
branch remains available. Private evidence is under
`~/Library/Application Support/CoD2-native-decomp/F1-parser-regression.noindex/`.

- Reproduced the original menu-helper failure before and after incorporating
  current integration. The latter crashes after 930 recorded frames and
  3,646 script error reports. Its first error is giving bot0 an unavailable
  weapon; later errors concern `self.pers`, followed by a network-send crash.
- Restoring only keyword string interning did not help. Restoring only the
  old notify/waittillmatch token mapping eliminated errors but also eliminated
  every live player. The inherited full-header rollback has the same flaw.
  All diagnostic source edits were removed.
- The installed `_teams.gsc` dormant helper requests `m1carbine_mp` for every
  Allied team. The installed `_weapons.gsc` precaches that weapon for Americans,
  but not British Toujane. The Thompson is precached for both American
  Carentan and British Toujane.
- Mac `G_GetWeaponIndexForName` at `0x1c7e0c` checks initialization at
  `0x1c7e1a` and tail-calls `BG_FindWeaponIndexForName` at `0x1c7e22` once
  initialization is over. Runtime lookup does not load an absent weapon.
  This agrees with the native implementation.
- The already-integrated callback bootstrap selects a Thompson and passes a
  400-frame live Toujane run. A private diagnostic recorder retains the
  original menu-notification helper and changes only its M1 Carbine selection
  to a Thompson. That path also completes 2,000 stock Toujane frames, with
  2,927 live player records, first live at frame 112, and zero script errors.

The canonical recorder and engine sources were not modified for this
investigation. The diagnostic recorder derives scripts from the user's
licensed install into its private home directory. No game script text or
reference source was copied into Git.

## Verification

Both stock/CoD2x client and dedicated targets build. All 24 integrated suite
commands pass: existing script checks, dedicated compiler checks, F1 compiler,
variable, services, VM, full runtime, nested-array compiler/VM, identity methods,
variable/VM integration, fixes13 and game fixtures, in each mode. Sanitizer
fixtures retain their ASan/UBSan settings and assertions are enabled.
The separate CoD2x dedicated full-VM callback fixture also passes with UBSan.

The four private menu-path smokes are DM, with seed 12345 and 50 ms fixed ticks.
Each contains 2,000 frames (4–2003), both clients become live at frame 112, and
both console and qconsole logs are checked for script errors:

| Build | Map | Live player records | Script errors |
| --- | --- | ---: | ---: |
| Stock | Toujane | 2,927 | 0 |
| CoD2x | Toujane | 2,983 | 0 |
| Stock | Carentan | 2,961 | 0 |
| CoD2x | Carentan | 2,944 | 0 |

Tooling: 49 tests pass; fidelity tooling: 35 pass. The successful synthetic
process test initially exceeded its 0.3-second launch allowance, twice. Its
fixture now allows three seconds for ordinary runs, consistent with the
neighboring oracle tests, and still explicitly uses 0.3 seconds for the
deliberate ten-second timeout case. Recorder behavior and validation are
unchanged. All three raw-offset unit tests pass.
The integrated raw-offset inventories have zero unreviewed/unsafe sites:
stock 400 sources / 1,212 sites, CoD2x 413 sources / 1,218 sites. The scanner
was run through its existing inventory/check functions with one preprocessing
worker, honoring the shared-machine job limit.

The legacy source check reports ten changed engine files, ten configurations,
zero mismatches against `3de7fcc`. For each of those ten files, separate
`unifdef -UCOD2_X64` outputs compare byte-identical with `cmp`. This does not
replace the unavailable Linux object/binary comparison.

Reproduce the suites (choose fresh output directories):

```sh
E="$HOME/Library/Application Support/CoD2-native-decomp/F1-parser-regression.noindex"
for B in build-macos build-macos-codx; do
    taskpolicy -b nice -n 19 cmake --build "$B" \
        --target cod2_macos cod2_macos_ded --parallel 2
    # Preserve compile_commands.engine.json; enable assertions in the fixture DB.
    taskpolicy -b nice -n 19 python3 tests/lp64/script/run.py \
        --build "$B" --output "$E/new-script-$B"
    taskpolicy -b nice -n 19 python3 tests/lp64/script/compiler_checks.py \
        --compile-db "$B/compile_commands.engine.json" --target cod2_macos_ded \
        --output "$E/new-compiler-ded-$B"
    for F in f1_compiler f1_variable f1_services f1_vm f1_runtime f1_compiler_arrays; do
        taskpolicy -b nice -n 19 python3 "tests/lp64/script/$F.py" \
            --compile-db "$B/compile_commands.engine.json" --output "$E/new-$F-$B"
    done
    taskpolicy -b nice -n 19 python3 tests/lp64/script/identity_methods.py \
        --parser-ubsan --compile-db "$B/compile_commands.engine.json" \
        --output "$E/new-identity-$B"
    taskpolicy -b nice -n 19 python3 tests/lp64/script/f1_variable_vm.py \
        --compile-db "$B/compile_commands.engine.json" \
        --fixture-dir "$E/new-f1_runtime-$B" --output "$E/new-variable-vm-$B"
    taskpolicy -b nice -n 19 python3 tests/fixes13/run.py --build "$B"
    taskpolicy -b nice -n 19 python3 tests/lp64/game/run.py --build "$B"
done
python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/fidelity -v
python3 tools/abi/test_raw_offsets.py
python3 tests/fixes13/legacy.py --base 3de7fcc
git diff --check
```

The exact commands, exits and durations are retained in `integrated-checks.json`;
map counts are in `integrated-smokes.json` and the stock Toujane trace. To
repeat the menu diagnostic, use the retained private `record-menu-valid.py`
with `PYTHONPATH="$PWD/tools/parity"` and the recorder's usual arguments.
It is derived from the canonical recorder by leaving the installed TestClient
entry point intact and replacing its M1 Carbine choice with a Thompson.

## Saved reference-command replay

Both stock and CoD2x complete the saved F0 2,000-frame command stream without
script errors. Each has 3,776 live player records, first live at frame 116.
Each matches its corresponding pre-F1c `3de7fcc` native replay on every
recorded frame, relative time, active entity count, quantized entity hash and
player field. This establishes no change to that recorded native workload;
it is not original-server agreement. Observed actions include movement,
jump, crouch, prone, fire, grenades and damage, with no death or respawn.

The reference comparison still fails at frame 4: 168 reference active entities
versus 205 native. That known world-initialization mismatch remains the next
engine investigation.

```sh
F0="$HOME/Library/Application Support/CoD2-native-decomp/F0.noindex"
# Repeat for each build with a fresh output and distinct port.
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
    --binary build-macos/cod2_macos_ded --data "$HOME/Games/CoD2" \
    --output "$E/replay-new" --port 29420 --dedicated --players --clients 2 \
    --frames 2000 --timeout 360 --map mp_toujane \
    --replay "$F0/original-f0c-live-record/usercmds.txt"
python3 tools/parity/compare.py \
    "$F0/original-f0c-live-a/statehash.txt" "$E/replay-new/statehash.txt" \
    --players "$F0/original-f0c-live-a/players.txt" "$E/replay-new/players.txt" \
    --require-clients 2
```

## Limits

The unmodified dormant helper still produces a script error storm and a
network-send crash on Toujane. This investigation identifies its invalid
weapon selection; it does not establish faithful recovery from that invalid
script or fix the later crash. F1c's existing report already limits claims
about invalid-script continuation. This remains a separate robustness issue.

Live smoke completion does not establish original-server parity, exhaustive
script coverage, TDM behavior, or renderer correctness. The original F1a spectator-only
smokes and the parser rollback must not be cited as live gameplay evidence.
The Linux OFF-object comparison remains unavailable; inherited legacy ABI,
inactive-CoD2x and Metal stub gates remain red.
This is integrated script/game verification, not a fresh run of the entire
repository's unrelated merge matrix.
