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

## F1c remains isolated

`decomp/f1-script` is clean at WIP `db07fc3`; none of it is integrated.
Reviewed the completed single-file rollback experiment under private
`.../orchestrator/f1c-bisect/`. Reverting `yyparse_impl.h` alone produced
2,000 Toujane frames with no script runtime errors. Reverting
`scr_compiler.c` also completed but retained script errors, so it is not a
passing candidate. Other rollbacks crashed, timed out, or failed to link.

This localizes a regression to the parser-header changes and their consumers;
it does not establish which individual change is wrong. A full header rollback
would also discard scanner fixes with dedicated fixtures. Investigate the
specific parser regression before merging; do not describe this WIP as green.

## Remaining oracle blockers

The reference workload exposes a reversed weapon-index/grenade-time argument
pair in native `player_die`, corroborated by Mac 1.3 assembly in the F0 report.
The earlier frame-4 entity-count mismatch and spectator one-ULP difference
remain unresolved. Full native corpus completion and replay agreement are
still required. Repository legacy ABI/inactive-CoD2x and Metal stub reds remain
as recorded in the unit reports; Linux OFF-object comparison is unavailable.
