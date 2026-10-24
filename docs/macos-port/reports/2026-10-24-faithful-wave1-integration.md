# Faithful decompilation wave 1 integration — 2026-10-24

## F1a

Integrated and pushed `4397bbe` (F1a `11a9e6e` over `e8d863d`). The test
worktree was clean at that merge and its reflog confirms that the inherited
verification ran after selecting it. Existing evidence under private
`~/Library/Application Support/CoD2-native-decomp/orchestrator/f1a-integrated/`
records both client/dedicated builds, movement and five collision suites in
both modes, 35 fidelity tests, and the legacy source check (13 files, ten
configurations, zero mismatches). All four stock/CoD2x Toujane/Carentan runs
contain 2,000 frames and no script runtime errors in console or qconsole logs.

Two inherited animation commands had invalid arguments; the tooling suite
also failed its benchmark lifecycle fixture with `Operation not permitted`.
The corrected runs at the same source revision passed:

```sh
E="$HOME/Library/Application Support/CoD2-native-decomp/orchestrator/f1a-resume"
for B in build-macos build-macos-codx; do
    taskpolicy -b nice -n 19 python3 tests/xanim/run.py \
        --compile-db "$B/compile_commands.engine.json" --output "$E/$B"
done
python3 -m unittest discover -s tools/tests -v
```

The actual retained animation outputs are `xanim-stock` and `xanim-codx`;
both pass under ASan/UBSan. All 47 tooling tests pass without code changes.
This closes the interrupted integration checks, not the remaining original
server replay scenarios or the inherited red gates in the F1a report.

## F0b/F0c oracle integration

Merged `a32ffbe` and `09f6a38` onto `4397bbe`. These change tooling and
documentation only. Reviewed the bootstrap, container adapter, rebuild path,
instrumentation workload and coverage checks. Retained reference evidence
reports successful toolchain/check/verify/coverage steps and two matching
2,000-frame reference replays. The native replay remains blocked as described
in the [F0 report](2026-10-23-F0-server-replay-oracle.md).

Integrated checks pass: 49 tooling tests, 35 fidelity tests, shell syntax,
AppleScript compilation, and whitespace checks. Two new 300-frame Toujane
dedicated runs used the F1a stock/CoD2x binaries and merged recorder:

```sh
python3 -m unittest discover -s tools/tests -v
python3 -m unittest discover -s tools/fidelity -v
sh -n tools/oracle/guest-setup.sh tools/oracle/container_server.sh
bash -n tools/oracle/rebuild-reference.sh tools/oracle/finish-toolchain.sh
# Compile vm.applescript with osacompile to a private evidence destination.
# For each stock/CoD2x build, using distinct ports and new output directories:
taskpolicy -b nice -n 19 python3 tools/parity/record.py \
    --binary build-macos/cod2_macos_ded --data "$HOME/Games/CoD2" \
    --output "$E/new-smoke" --dedicated --players --clients 2 \
    --frames 300 --port 29371 --timeout 90 --map mp_toujane
```

Private evidence: `.../orchestrator/f0-integrated/`. Both clients first become
live at frame 116; stock has 298 live player records and CoD2x has 318. Neither
run contains script runtime errors. These short smokes validate integrated
bootstrap behavior, not the grenade/death corpus or cross-engine parity.
The VM stayed stopped; its completed reference rebuild/replays were not rerun.

## F1c rollback result (corrected after the live-player investigation)

At the start of this investigation, `decomp/f1-script` was clean at WIP
`db07fc3` and none of it was integrated. Reviewed the single-file rollback experiment under private
`.../orchestrator/f1c-bisect/`. Reverting `yyparse_impl.h` alone produced
2,000 Toujane frames with no script runtime errors. Reverting
`scr_compiler.c` also completed but retained script errors, so it is not a
passing candidate. Other rollbacks crashed, timed out, or failed to link.

The later investigation found that both rollback runs had **zero live players**.
The parser rollback restores swapped notify/waittillmatch tokens and prevents
the menu helper from spawning players. It therefore masks the failure rather
than locating a faulty parser change. The four original F1a map smokes above
also had zero live players; their completion proves spectator-only stability.
The later F0 helper runs did have live players, as their counts record.

The original dormant helper selects an American M1 Carbine on British Toujane,
where the installed scripts do not precache it. With the original notification
path retained and only that selection changed to a precached Thompson, the
integrated F1c stock run completes 2,000 frames with live players and no script
errors. See the [F1c investigation](2026-10-24-F1-script-integration.md) for the
full verification and remaining invalid-script failure.

F1c integration verification now passes: both client/dedicated builds,
24 script/game suite runs, the dedicated callback VM fixture, four 2,000-frame
live-player DM map smokes, and both saved-command native replays. The latter
match the corresponding pre-F1c native results. No parser rollback is included.

## Remaining oracle blockers

The reversed weapon-index/grenade-time arguments in native `player_die` were
fixed in `3de7fcc`; see the [focused report](2026-10-24-F1-grenade-death.md).
Native corpus completion is verified. Original/native replay agreement is
still missing: the frame-4 entity-count mismatch and spectator one-ULP difference
remain unresolved. Repository legacy ABI/inactive-CoD2x and Metal stub reds remain
as recorded in the unit reports; Linux OFF-object comparison is unavailable.
