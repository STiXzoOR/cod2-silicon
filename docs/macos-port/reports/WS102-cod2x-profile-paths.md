# WS102 — Restore CoD2x per-profile config paths

Date: 2026-10-09. Branch: `codex/cod2x-profile-paths`. Worktree:
`/Users/stix/Projects/cod2-native-wt/cod2x-profile-paths`. Baseline: `048ff5e`.
Release lineage remains `port/main` at `664f1a1`.

**Prepared for a later Config Studio decision; do not compose this branch into
this release on its own.** This engine change must ship with the launcher's
migration of the released flattened `main/config_mp.cfg` and profile launch
wiring. The user has not selected the release that includes Config Studio.
Nothing was pushed, merged, tagged, released or published.

## Reference findings and scope

Read `CONTRIBUTING.md` and all of `docs/macos-port/PLAN.md` before editing.
Inspected `0dab850`'s WS47 report and path changes, and read the safe stash
`26c0a784251b7900f66f978992d665a1c6f8699f` without applying or dropping it.
Read-only launcher inspection used `codex/config-studio-int:launcher/ConfigProfiles.swift`.
No launcher changes, layouts, generated snapshots, gameplay, timing, rendering,
movement or host revision changes are part of this branch.

The authoritative Mac 1.3 i386 binary is
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
Checked its actual symbols and instructions with `nm -a` and
`otool -arch i386 -tV -p <symbol>`:

- `FS_BuildOSPath_Internal` at `0x331f2` accepts `base`, `game`, and `qpath`;
  `0x33207–0x3320e` selects the supplied nonempty game, otherwise `0x33265`
  uses `fs_gamedir`. `0x33282–0x332d4` copies all three path components. There
  is no config-name rewrite. Public `FS_BuildOSPath` at `0x3331a` delegates to it.
- `Com_BuildPlayerProfilePath` at `0x1116ca` uses the literal `players/%s/`
  at `0x25433c`; profile startup/change adds `config_mp.cfg` at `0x24178c`.
  Init at `0x111aa6` reads `players/active.txt` (`0x254350`), calls `Com_Parse`
  once at `0x111b34`, checks the named profile, then executes its config.
  Change at `0x111936` writes the selected name, disconnects, resets dvars,
  selects that profile and executes its config.
- Original Mac new/delete use `fs_basepath` and the current game directory.
  The already-integrated native adaptation uses writable `fs_homepath` for
  both operations. WS102 retains that adaptation and tests separate base/home
  roots; it is not presented as original Mac ownership behavior.

CoD2x reference checkout is tag `v1.4.6.8`, commit `d8c5469`.
`src/shared/iwd.cpp:710–751` compares the requested qpath case-insensitively
against the **active** profile's `players/<profile>/config_mp.cfg`. It changes
only `game` to `main`, preserving qpath for reads and writes. A bare
`config_mp.cfg` does not match. The independent `movie` rule routes non-IWD
paths to `main` and leaves IWD paths in `movie`.

Ported only WS47's small path correction: native CoD2x no longer rewrites qpath
to a bare filename, and its policy helper no longer treats a bare filename as
a profile config. Both gates are required (`COD2_X64=1` and `COD2_CODX=1`).
The older helper body is retained for every inactive configuration.

The existing native helper matches any `players/.../config_mp.cfg`, whereas
upstream compares only against the current profile's exact path. This branch
retains that broader cross-profile policy from WS47, including case-insensitive
recognition; it does not add active-profile state to the policy API or claim
exact equivalence for explicitly executing another profile's config. Ordinary
selected-profile reads/writes have the reference-supported contract.

## On-disk contract for the launcher

Let `H` be the private writable `fs_homepath`, `G` the resolved `fs_gamedir`
(`main` when no mod is selected), and `P` the selected profile token.

| Request or operation | Native stock | Native CoD2x |
| --- | --- | --- |
| Selected `players/P/config_mp.cfg` | `H/G/players/P/config_mp.cfg` | `H/main/players/P/config_mp.cfg`, independent of `fs_game` |
| Bare `config_mp.cfg`, game `pam` | `H/pam/config_mp.cfg` | `H/pam/config_mp.cfg` |
| Bare `config_mp.cfg`, game `main` | `H/main/config_mp.cfg` | `H/main/config_mp.cfg` |
| Bare config, NULL/empty game with `G=pam` | `H/pam/config_mp.cfg` | `H/pam/config_mp.cfg` |
| Bare config, game `movie` | `H/movie/config_mp.cfg` | `H/main/config_mp.cfg` (existing demo fallback) |
| Profile create/delete | `H/G/players/P/` | `H/G/players/P/` |
| Active selection write | `H/G/players/active.txt` | `H/G/players/active.txt` |
| Profile listing / active-file read | Current filesystem search paths | Current filesystem search paths |

The table defines canonical writable destinations and OS path construction.
Read operations retain the engine's normal search-path/base fallback behavior;
WS102 does not convert every read into an exclusive writable-home lookup.
Bare `exec config_mp.cfg` remains an explicit bare lookup and can read a file
at that name through the normal search paths. It is not the selected profile's
config path.

`Com_InitPlayerProfiles` selects the first parsed token of the active file,
not the whole file. Profile change still writes that token, disconnects, resets
dvars, and requests `players/P/config_mp.cfg`. Listing, active-file selection,
and profile directory create/delete are **not** redirected wholesale to main.
Consequently a mod-specific active file remains a distinct input from the
CoD2x main profile config. The fixture sets `fs_gamedir` to `pam`, the state
resolved from `fs_game`; it does not simulate a complete filesystem restart.

Config Studio's F2 store already owns `H/main/players/P/config_mp.cfg` and
`H/main/players/active.txt`. Its `.flattenedLegacy` migration explicitly reads
`H/main/config_mp.cfg`. WS101's launch wiring must honor the selection/search
contract above when a mod is selected; editing a main active file does not
itself override a mod-specific active file. This branch does not verify that
launcher wiring or mutate either launcher worktree.

Existing released native CoD2x users' settings reside in
`H/main/config_mp.cfg`. Normal selected-profile startup on this branch will
request `H/main/players/P/config_mp.cfg` instead, and will not automatically
import the shared file. **Those old settings are carried into the selected
profile only through the launcher's explicit `.flattenedLegacy` migration.**
The engine adds no migration, fallback to the shared filename, deletion, or
rewrite of existing stored data. Shipping this branch without migration would
cause existing settings to stop applying on normal profile startup. Manual
bare exec is still possible and is not a migration strategy.

## Test-first evidence

Expanded `tests/fidelity/profiles.c` before editing production. The fixture now
extracts the public production `FS_BuildOSPath` as well as its internal builder
and existing production profile functions. Its dvar double copies the selected
name, matching real dvar string ownership, so checking the state after a
profile change does not retain a pointer into a returned stack frame.

The same expectations run with stock and CoD2x flags. Both main and mod
lifecycles exercise create/delete, active-file first token, disconnect/reset,
profile init/change and resolved config destinations. Literal path cases cover
distinct profiles, nested mod directories, explicit/NULL/empty game values,
bare config, non-config profile files and the retained movie exception.

Before the production fix on `048ff5e`: stock passed; CoD2x failed **12**
assertions (four init/change config destinations, five profile path cases,
three bare mod config cases). This was an expected behavioral failure, with
no compiler or sanitizer failure. The post-fix historical `--baseline
048ff5e` run preserves that red result without changing the checkout.
Both current modes pass all six fidelity suites with assertions and ASan/UBSan.

## Verification and limits

Evidence is private and left at:
`~/Library/Application Support/CoD2-native-current-readiness/WS102.noindex/`.
Logs, command receipts, compile databases, reference facts and native-OFF
comparison outputs are there. Raw reference instructions stay outside git.
Every compile/heavy check runs under `taskpolicy -b nice -n 19`, with at most
two compiler jobs. No game window, GUI lease, real player data folder or
licensed game-data writes were used.

Results are bounded to these checks:

| Check | Observed result |
| --- | --- |
| Current fidelity suites, stock and CoD2x | All six in each mode pass; zero failed assertions/sanitizer reports |
| Historical profiles at `048ff5e` | Stock zero failures; CoD2x twelve failures, exit 1 as expected |
| Actual Release changed-TU commands | Six compile successfully: stock client/dedicated `com_files.c`; CoD2x client/dedicated both files |
| Focused function ABI | Stock two compile entries / CoD2x four; zero errors, mismatches or new findings |
| Focused raw offsets | Stock one / CoD2x two source files, eight sites in each mode, zero unsafe/unreviewed sites |
| Strict bit conversions | Zero violations in both focused databases |
| Import ABI, client and dedicated in both modes | Zero proven extra/missing dereferences; changed TUs have zero active import-access sites |
| Four ABI checker test scripts | All pass |
| `tests/fixes13/legacy.py --base 048ff5e` | Two engine files, ten configurations, zero mismatches |
| `unifdef` + `cmp` | Both engine files, six inactive/stock configurations each, twelve byte-identical comparisons |
| Actual SDK preprocessing with `COD2_X64` removed | Six engine compile entries across both modes/targets; zero mismatches/errors |
| `COD2X_SANITIZERS=1 sh tests/cod2x/run_full.sh` | Exit 0, including production policy/tokenizer fixture |
| `sh tests/cod2x/run_native.sh` | Exit 0, including native event/queue and watchdog/crash checks |

`com_files.c` still emits 21 reconstruction warnings per Release compile;
`cod2x_policy.c` emits none. The logs retain those warnings. The synthetic
native URL probes also print macOS sandbox-extension and missing-verified-shader
diagnostics; those runs pass their API/queue assertions and are not rendering
fidelity checks. They use private synthetic data/home roots. Their temporary
`.app` bundles are under `.noindex`, and both scripts unregister them before
cleanup.

The initial callback-check attempts failed with `StopIteration` because the
focused database lacks the checker's required renderer `r_init.c`. Rerunning
with each complete engine compile database fixes that input error. The
resulting check uses the focused declaration audit: 17 of 218 bindings have
compatible definitions; 201 definitions are absent from that scoped audit.
There are zero table/named-cast mismatches or unprototyped floating calls in
that bounded check. This is not the full renderer callback merge gate. Initial
failed attempts remain recorded in `verification-receipts.json` and their logs.

The adjacent tokenizer/OOB fuzz fixtures compile production `cod2x_policy.c`
in CoD2x mode (the stock engine excludes that TU). Tokenizer regression
`infostring-setkey-overflow` passes in both modes. Four 15-second campaigns,
seed 102, complete without a crash or sanitizer report: stock tokenizer
222,976 inputs, stock OOB 53,504, CoD2x tokenizer 169,984, CoD2x OOB 46,848.
These are bounded parser memory-safety checks, not profile disk round trips
or a proof of complete tokenizer grammar equivalence. Exact commands and
counts are in `adjacent-receipts.json` and the `*-fuzz.log` files.

Configuration and behavioral commands, run from this worktree:

```sh
E="$HOME/Library/Application Support/CoD2-native-current-readiness/WS102.noindex"
B=output/ws102.noindex
mkdir -p "$E/tmp.noindex"

taskpolicy -b nice -n 19 cmake -S . -B "$B/stock" -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake -S . -B "$B/codx" -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1

for mode in stock codx; do
  taskpolicy -b nice -n 19 python3 tests/fidelity/run.py --build "$B/$mode"
  taskpolicy -b nice -n 19 python3 tests/fidelity/run.py --build "$B/$mode" \
    --test profiles --baseline 048ff5e
  taskpolicy -b nice -n 19 python3 tools/abi/audit.py "$E/$mode-changed-tus.json" \
    --jobs 2 --baseline tools/abi/baseline.json --output "$E/$mode-abi-functions.json"
  taskpolicy -b nice -n 19 python3 tools/abi/raw_offsets.py \
    "$E/$mode-cod2_macos-changed-tus.json" --output "$E/$mode-abi-offsets.json"
  taskpolicy -b nice -n 19 python3 tools/abi/astdump_strict.py \
    "$E/$mode-changed-tus.json" --jobs 2
done

COD2X_SANITIZERS=1 TMPDIR="$E/tmp.noindex/" \
  taskpolicy -b nice -n 19 sh tests/cod2x/run_full.sh
TMPDIR="$E/tmp.noindex/" taskpolicy -b nice -n 19 sh tests/cod2x/run_native.sh
taskpolicy -b nice -n 19 python3 "$E/preserve.py"
taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base 048ff5e
git diff --check
```

The expected-red baseline command in the loop exits 1 only for CoD2x; it is
not a green gate. `verify.py` and the command receipts in the evidence directory
record every exact Release compile, import ABI, SDK preservation and checker
command. The compile databases retain actual engine flags (including
`-DNDEBUG`); the fidelity/fuzz runners independently enable their assertions.
The two CoD2x TUs' client/dedicated entries and stock `com_files.c` entries are
selected into the `*-changed-tus.json` databases without changing flags.

`preserve.py` reads the baseline file with `git show 048ff5e:<path>`, runs
`unifdef -UCOD2_X64` on both old and current bytes, and compares outputs with
`cmp`. It also checks native-OFF with CoD2x explicitly enabled/disabled,
native stock with CoD2x undefined/zero, and synthetic native-defined-zero with
CoD2x enabled. `unifdef` exit 1 means branches were removed, not a comparison
failure; all twelve `cmp` results are zero. This is exact source preservation,
not a substitute for original linked i386 object/binary parity.

The shared readiness brief reports these pre-WS102 red gates:
`legacy-abi` has ten differences (five in generated `build/lp64_gen/data_native.c`,
excluded by CMake OFF, plus five in test-only `tests/platform/server_control.c`);
`tests/cod2x/check_inactive_gates.py` has 436 differences across intentional
stock/native repairs and synthetic x64-defined-zero configurations. The x86
Linux byte-for-byte comparison requires x86_64 Linux multilib and is unavailable
on this Mac. None is weakened or described as green. `COD2X_HOST_REVISION=0`
remains locked.

Fresh full source-guard commands against release base `664f1a1` reproduce:

```sh
taskpolicy -b nice -n 19 python3 tools/abi/legacy.py --base 664f1a1
# exit 1: 92 files, five native-OFF configurations, ten differences

taskpolicy -b nice -n 19 python3 tests/cod2x/check_inactive_gates.py --base 664f1a1
# exit 1: 93 files, seventeen configurations, 457 differences

taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base 664f1a1
# exit 0: 85 changed source files, ten configurations, zero differences
```

The legacy ten are exactly the generated-data/server-control differences named
in the shared brief. The observed inactive total is **457**, not the supplied
436. The extra 21 are seven each in `src/PC/speex/{filters,ltp,nb_celp}.c`,
already changed by native voice commits `9a09b7b` and `048ff5e` before WS102.
The other entries total 436. Those branches use `defined(COD2_X64)` for
intentional stock/native repairs and remain active in synthetic defined-zero
and native-CoD2x-off configurations. `gate-accounting.json` records this
breakdown. Neither WS102 engine file appears in the full inactive failures.
No checker, gate configuration, baseline or allowlist was changed.

Focused guard commands with `--base 048ff5e` intentionally also see the updated
**test source**: `tools/abi/legacy.py` reports five differences, all in
`tests/fidelity/profiles.c`, while its actual SDK comparisons show zero engine
errors/differences. `tests/cod2x/check_inactive_gates.py` reports seventeen,
also all in that fixture. These commands remain red, not silently filtered or
called passing. The full release-base checker considers modified existing
files; the fidelity fixture was added after `664f1a1`, so it is absent from
that full comparison. The dedicated `unifdef` proof covers both touched
engine files without changing that broader gate's semantics.

No full linked engine build, game launch, disk round trip, launcher migration,
mod filesystem restart or complete composed merge gate was run by WS102.
The fixtures observe production path/profile behavior with filesystem/parser
services isolated as documented in `tests/fidelity/README.md`. They do not
establish whole-game fidelity, live startup or launcher selection/migration
correctness. No human playtest is a dependency for this prepared unit.

Removed all scratch build/fixture directories created for WS102, including
`output/ws102.noindex` in this worktree and `release-tus`, `fuzz-stock`,
`fuzz-codx`, and `tmp.noindex` under the evidence root. Object/fixture hashes
were saved first in `artifact-hashes.json`; cleanup is recorded in
`cleanup.json`. Logs, reference facts, receipts, compile databases and exact
source comparisons remain in the evidence directory. Reproduction starts
with the CMake configure commands above to recreate the deleted build paths.
No owned QA app or process remains. No user game/config directory was changed.
All requested source/doc changes are committed; nothing is left uncommitted.

## Composition handoff

Engine/fixture commit: `2147e5f` (`Restore native CoD2x per-profile config paths`).
The following focused commit adds this report. Both have DCO sign-off.

Only the two production path TUs, `tests/fidelity/{profiles.c,run.py,README.md}`
and this report change. Compose the focused engine/fixture commit together
with this report commit only after Config Studio migration and launch wiring
are selected for the same release. Do not cherry-pick all of `0dab850` or pop
its related stash. Stock behavior and the native-OFF engine source are preserved.
The integration owner still needs the full composed merge gate and launcher
migration/handoff checks; these bounded source fixtures do not establish them.
