# WS113 — Restore exact native-OFF source bytes

Date: 2026-10-09. Branch: `codex/native-off-bytes`. Starting head:
`c515fc59575cf95b5e61b513413a3ad1444826a4`; release comparison base: `664f1a1`.

Private evidence:
`~/Library/Application Support/CoD2-native-current-readiness/WS113-native-off-bytes.noindex/`.
Below, `E` denotes that directory. No evidence containing game content is committed.

## Result and scope

Restored the exact `unifdef -UCOD2_X64` bytes in the nineteen existing engine
files named by WS111. Fourteen files move separating blank lines inside the
adjacent native guard, preserving physical line counts. The other five restore
original inactive spelling while keeping the existing native branch:

- `bg_animation_mp.c`: native mounted-mask alias, original inactive `0x3` expressions.
- `r_dvars.c`: native lighting flags and registration block, original inactive literals and spacing.
- `dvar_cmds.c`: native external-dvar flag and combined-string capacity, original inactive literals.
- `stringed_hooks.c`: native fifteen-language bounds, original inactive fourteen-language bounds.
- `sv_main_mp.c`: native CoD2x exclusion surrounding the original net-hardening condition.

`unifdef` removes the known native branches but does not expand aliases. A
shared alias definition therefore leaves different inactive source bytes even
when compilation would produce the same value. The affected inactive branches
use the original literal expressions; their native alternatives keep the
existing aliases and intentional fixes.

There is no behavior-bearing change or new test. Existing fixtures remain
unchanged. Native Release artifact comparison supplies the primary preservation
proof; token guards and subsystem fixtures supplement it. `COD2X_HOST_REVISION=0`
is unchanged. No engine feature, launcher behavior or game data changes.

## Exact inactive-source proof

`native_off.original.py` preserves WS111's checker. The task-local copy sets this
worktree as `ROOT`, keeps the path inventory at the supplied starting head, and
reads current worktree bytes. It checks `src` only, as the brief excludes generated
`build/lp64_gen/data_native.c`; added files are inventoried but cannot be compared
with an existing release file. It uses the original `unifdef` and `cmp` operations
without whitespace, token, newline or other normalization.

```sh
E="$HOME/Library/Application Support/CoD2-native-current-readiness/WS113-native-off-bytes.noindex"
python3 "$E/native_off.py"
```

Result: **95 inventoried paths; 88 existing modified source files; zero existing-file
mismatches**. All nineteen touched files have `cmp_exit=0` and matching byte hashes
in `native-off-results.json`. Seven added source paths are inventoried; six have
nonempty inactive text and remain explicitly out of scope. The initial broad inventory had nineteen existing source
mismatches plus the known, excluded generated-data mismatch; it is retained in
`native-off-before.log`.

## Native object preservation

The baseline compilation ran before editing, with a clean tree at `c515fc5`.
All nineteen saved source files are independently verified against `git show
c515fc5:<path>` in `baseline-source-receipt.json`. The actual engine Release
compile databases are retained as `compile_commands.stock.engine.json` and
`compile_commands.codx.engine.json`.

`compare_objects.py` preserves every engine flag except dependency-output options
and debug information, replacing the latter with `-g0`; only the object output
path changes. Before/after sources use the same original absolute path and build
working directory. It compiles all sixty active client/dedicated database entries
for the touched C files, with at most two workers. The changed animation header
is consumed by `bg_animation_mp.c` in all four native variants.

Stock CMake excludes the CoD2x-only `cod2x_runtime.c`. `compare_excluded.py` adds
client and dedicated stock probes using the corresponding `cmd.c` engine flags;
old/current source is passed through the same stdin input path. These probes
confirm its empty stock translation unit is unchanged.

| Mode | Client pairs | Dedicated pairs | Nonidentical pairs |
| --- | ---: | ---: | ---: |
| Stock | 18, including one excluded-source probe | 13, including one excluded-source probe | 0 |
| CoD2x | 18 | 13 | 0 |

**All 62 object pairs compare byte-for-byte. No `__LINE__` exception, object
normalization or ignored object section was needed.** The receipts include actual
commands, compiler exits, `cmp` exits and SHA-256 hashes; the paired objects remain
in the evidence directory.

```sh
# The before command was executed at c515fc5, before any source edits.
taskpolicy -b nice -n 19 python3 "$E/compare_objects.py" before
# The after command can be repeated against this branch and the retained baseline.
taskpolicy -b nice -n 19 python3 "$E/compare_objects.py" after
taskpolicy -b nice -n 19 python3 "$E/compare_excluded.py"
```

An initial edit-script typo split one native animation `#if` across two lines;
four animation compilations failed. The condition was restored to one line and
the full sixty-entry comparison was rerun successfully. The failed summary is
retained in `object-after-first-pass.log`.

## Builds and validation

Every compile and heavy check used `taskpolicy -b nice -n 19`, with at most two
compiler jobs. Engine compile databases retain Release `-DNDEBUG`; fixture
copies enable assertions with `-UNDEBUG` after the full engine builds. A private
Python startup hook caps the full-audit executor pools at two workers without
changing the audited flags, inputs or result criteria.

```sh
taskpolicy -b nice -n 19 cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
taskpolicy -b nice -n 19 cmake --build build-macos \
  --target cod2_macos cod2_macos_ded --parallel 2
taskpolicy -b nice -n 19 cmake --build build-macos-codx \
  --target cod2_macos cod2_macos_ded --parallel 2
```

All four native Release client/dedicated builds pass, including verification of
the committed typed-data snapshot. `build-binaries.json` records their hashes.

The following unchanged fixtures pass in both stock and CoD2x modes. Exact
commands, exit codes and log paths are retained in `check-plan.json` and
`checks.json`; `run_checks.py` is the task-local serial driver.

| Subsystem | Checks in each mode | Result |
| --- | --- | --- |
| Fixes13 | `tests/fixes13/run.py` | Pass |
| Dvars and language/stringed | Full `tests/fidelity/run.py` | Six fixtures pass |
| Movement and animation | Movement `run.py`, `weapons.py`, `animation_stance.py`, all with revision matrices | Pass |
| Host lifecycle/protocol | Listen/dedicated lifecycle and stop-gametype, protocol, player broadcast, shotgun spread, UDP operations | Pass |
| HUD and UI | HUD, ownerdraw, scoreboard and full LP64 UI | Pass |
| Script | Client/dedicated compiler checks, identity methods with parser UBSan | Pass |
| Renderer water | `tests/lp64/renderer/run_water.py` | Pass |
| Audio and voice | Audio, voice codec with sanitizers, playback-init rate contract | Pass; 55 codec contracts and three playback observations per mode |

`tests/fixes13/legacy.py --base 664f1a1` passes across 88 changed source files and
ten configurations. All three token guards pass against this unit's starting
head `c515fc5`: fixes13 (19 files, ten configurations), ABI (19 files, five), and
inactive CoD2x (19 files, seventeen). `extra-checks.json` records the two extra
unit-scoped guards. Fidelity unit tests pass 35/35; datagen unit tests pass 12/12;
trace guards pass.

Both full native ABI gates pass, including the private Mac 1.3 retail-import
check. These are full compile-database audits, beyond the touched-file object
comparison.

| Audit | Stock | CoD2x |
| --- | ---: | ---: |
| Function translation units | 624; zero errors/mismatches/new findings | 641; zero errors/mismatches/new findings |
| Reviewed raw-offset sources/sites | 391 / 1,197; zero unsafe/unreviewed | 404 / 1,203; zero unsafe/unreviewed |
| Renderer bindings | 218; zero mismatches/new findings | 218; zero mismatches/new findings |
| Bit conversions | Zero violations | Zero violations |
| Retail import symbols/native sites | 514 / 1,779; zero extra/missing dereferences | 514 / 1,782; zero extra/missing dereferences |

Both import audits retain 185 existing placeholder observations. Their error
lists are empty; passing this audit does not imply that the placeholder
inventory is empty. Full receipts remain in `abi-stock/` and `abi-codx/`.

```sh
taskpolicy -b nice -n 19 env PYTHONPATH="$E/python-pool-cap" \
  PYTHONDONTWRITEBYTECODE=1 sh tools/abi/check.sh \
  build-macos/compile_commands.engine.json "$E/abi-stock"
taskpolicy -b nice -n 19 env PYTHONPATH="$E/python-pool-cap" \
  PYTHONDONTWRITEBYTECODE=1 sh tools/abi/check.sh \
  build-macos-codx/compile_commands.engine.json "$E/abi-codx"
```

The CoD2x callback VM fixtures pass with UBSan for listen and dedicated modes.

`COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh`,
`sh tests/cod2x/run_native.sh`, `python3 tests/lp64/script/run.py`, and
`python3 tests/rendering/shader_setup.py` all pass. The native suite includes its
watchdog/crash-report fixture; the full suite includes synthetic URL and local
HTTPS/TLS fixtures. The shader manifest suite passes 3/3.

The original serial runner completes all 67 planned checks: 62 exit zero, two
inherited guards exit 1, the initial tools run exits 1, and the two cancelled
inventories exit -9. Its final exit remains 1. The unchanged tools rerun and two
additional unit-scoped token guards each exit zero.

`final-verification.json` records a fresh retained-object `cmp` for all 62 pairs,
zero source-hash changes across the nineteen compiled files, zero existing
inactive-source mismatches, and all required unit checks complete. It explicitly
records `overall_repository_merge_gate_green: false`.

```sh
taskpolicy -b nice -n 19 python3 "$E/final_verify.py"
```

## Inherited red gates and limits

- `python3 tools/abi/legacy.py --base 664f1a1`: **10 mismatches**, exactly five
  configurations for generated `build/lp64_gen/data_native.c` (excluded from
  CMake OFF), and five for test-only `tests/platform/server_control.c`.
- `python3 tests/cod2x/check_inactive_gates.py --base 664f1a1`: **465 mismatches**
  across 96 files and 17 configurations, matching the actual WS112 integration
  result. The brief's historical 436 is 29 below this starting-head count.
  Intentional native/stock repairs and synthetic `COD2_X64=0` configurations
  remain visible; this unit adds **zero** differences against `c515fc5`.
- The Linux i386 object/binary comparison remains unavailable on this arm64 Mac.
  Neither source comparison nor native object comparison is claimed as a Linux
  multilib binary proof.

The initial tools suite was **31/32**: `test_record_success` exceeded its unchanged
0.3-second synthetic recorder deadline. It runs a Python fake engine. The task's
first invocation inherited the audit-pool startup hook through `PYTHONPATH`; the
unchanged rerun omitted that hook and passed **32/32**, while retaining background
priority and serial fixture compilation. The exact scheduling/startup cause is
unlocalized. No test, timeout or expectation was changed. Both receipts are kept.

The additional whole-engine new-stub inventories were deliberately cancelled
in both modes. This unit adds no functions and all 62 native object pairs are
identical; the full inventories would add another long AST scan beyond the
requested touched-subsystem checks. Their `-9` exits remain in the runner receipt
and are **cancelled, not passing**. This is a partial overall repository merge
gate, even after the required unit checks pass. No baseline or check expectation
was weakened. The serial runner retains the original tools failure and these
two cancellations, and therefore exits 1.

Fixtures and source/artifact identity are bounded checks. No live game, GPU
screenshot, online round, hardware audio or human playtest was used or claimed.

## Cleanup and handoff

Source commits, in order:

1. `4298a58` — separating blank lines in fourteen engine files.
2. `94a2164` — inactive dvar and language spelling in three files.
3. `6f04317` — inactive animation spelling and server condition in two files.

A fourth commit records this report. All commits carry the configured DCO
sign-off. Compose this branch on the supplied release integration head; no
old-upstream `main` lineage was used. Nothing was pushed, tagged, released,
merged, or sent to a remote.

Task-private fixture build roots, temporary QA directories and AST caches
outside the worktree were deleted after archiving their logs and structured
receipts under `E/fixture-receipts/`; `cleanup.json` records the removals. No QA
`.app` remains. The synthetic app drivers unregister their URL probe bundles
with `lsregister -u` before deleting them. No game window was run, so no GUI bench
lease was taken. No process owned by another agent was terminated.

The intentional evidence directory remains, including paired before/after
objects, baseline source snapshots, build/fixture logs, exact commands and JSON
receipts. The two ignored engine build directories inside this worktree remain.
There are no uncommitted source/report changes after the report commit. The
user's real data folder, licensed data and global configuration were untouched.

The copied checker fixes its cumulative source inventory at the supplied
`c515fc5` starting head and reads this worktree. For a later composition that adds
peer changes, update its root and inventory head before treating it as a
composition-wide gate. This unit's nineteen files are all covered.

The source repair is ready to compose with its exact byte/object receipts.
The inherited red gates, cancelled extra inventories and unavailable Linux
comparison remain integration limitations; they are not converted into passes.
