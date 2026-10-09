# WS98 — Config Studio F4 catalog

2026-10-09. Branch `codex/config-catalog-f4`, foundation base `1a2c0da`.

## Result and composition

F4 adds a source-root-aware generator and coverage gate, authored presentation
copy, the JSON resource, and an immutable Codable/Sendable Swift loader with a
focused strict-concurrency runner. The resource describes readiness integration
**`048ff5e`**, not this branch's older 0.2.0 engine. It contains 733 settings
(728 actual registrations and five supplemental references) and 235 commands.

Compose all WS98 commits after `1a2c0da`; no F2 interfaces are required. F3 must
add `ConfigCatalog.swift` to launcher compilation and copy
`launcher/Resources/ConfigCatalog.json` into the bundle. This unit does not wire
views, settings transactions, live values, or launcher build scripts. It does
not implement Metal or change the engine.

Existing focused commits:

- `2cdcab3`: source metadata, resource, authored copy and coverage tests.
- `ba1725c`: Swift models, loader and focused runner.
- `fb5e4a1`: reject unknown constructors and source-selection appends.
- `09e4c5e`: validate command-site evidence when decoding catalogs.
- `4439e39`: implementation headers, exact table selection, C arithmetic,
  checked globs, corrected diagnostic copy and regenerated integration facts.
- `b27e053`: fail coverage when all claimed command-site evidence is removed.

The checklist/report form the final documentation commit. All commits have DCO
sign-off with `23358296+STiXzoOR@users.noreply.github.com`.
No push, tag, PR, remote change or merge was performed.

## Catalog contract

- Eight purpose groups follow the spec/canvas: Frame & movement, Aim & input,
  Network, Visibility, HUD & crosshair, Audio, Graphics, Other. Titles and
  explanations are original plain-language copy. Raw names are separate fields
  for Expert/text consumers. There is authored copy for 99 settings and 66
  commands, prioritizing actual menu-exposed and competitive/network settings.
  All actual menu-exposed entries identified in the supplied research have
  authored copy; remaining Expert entries explicitly say their effect has not
  been independently documented.
- `variants.stock` and `variants.cod2x` retain every found constructor site,
  with source path/line, type, default, bounds, flags and enum choices. Origins
  distinguish stock, CoD2x and port registrations. Explicit command registration
  and game dispatch are separate mechanisms. Eight integration commands are
  dispatch-only; they are not described as explicit console registrations.
- Native-client CMake source selection includes hosted-server registrations.
  Dedicated-only and disabled optional feature sets are outside this catalog.
  The reader expands source-local headers before Clang preprocessing and
  deduplicates identical header sites across translation units. The three UDP
  query-limiter registrations come from
  `src/PC/server_mp/cod2x_udp_operations_impl.h:103`–`105`; this implementation
  header does not exist in the older worktree.
- Defaults are **constructor inputs**, not observed effective values or a
  startup-order prediction. Runtime paths, display choices, hardware identity,
  protocol selection and other expressions stay dynamic with no invented value.
  Enum defaults are indices; colour arguments are normalized constructor inputs.
  The Swift loader exposes ambiguous consensus when sites disagree. Numeric
  source values do not claim exact live single-precision serialization.
- ROM, NOWRITE and CHEAT entries cannot become editable controls. Conditional
  flags preserve expressions/candidate words and use a conservative union for
  restrictions. The resource has 275 protected stock entries and 303 protected
  CoD2x entries; each mode has 22 Guided entries. Swift rederives restrictions
  from site flags even if the serialized editable hint is wrong. Server-policy
  metadata is carried separately for F3's live context.
- Correct names are `r_swapInterval`, `r_anisotropy`, `mp_QuickMessage`.
  `r_vsync` and `r_anisotropic` are unavailable spelling hints. `m_enable` is
  reference-only (upstream Windows CoD2x registration, absent natively).
  `scr_sd_roundlimit` and `scr_dm_scorelimit` are illustrative script/mod entries
  with no claimed native defaults. Supplemental entries cannot satisfy coverage
  or shadow actual registrations. They are not an exhaustive mod inventory.
- Fifteen generic runtime-registration factory sites per mode are recorded in
  `dynamicRegistrations`, rather than inventing a finite set of names. Unknown
  wrappers, name indirections, CMake globs or source-selection appends fail
  closed. Constant table indices select only the indexed name; bounded reviewed
  loops select the corresponding slice. C integer division truncates toward
  zero; cast-sensitive arithmetic stays dynamic.
- `ConfigCatalog` provides case-insensitive name lookups and group lookups,
  per-mode variants, Guided filtering and bundle loading. Typed controls include
  numbers/vectors for Expert. Schema/groups, duplicate names, registration shape,
  default types and setting/command site evidence are validated on load.

## Verification

Evidence directory:
`~/Library/Application Support/CoD2-native-current-readiness/WS98.noindex/`.
No licensed data, private player configs or private extraction tools were used.
The generator is original Python standard-library code plus the installed Clang
preprocessor. It does not build or run the game.

Reproduce the requested source export, then the CLI checks:

```sh
E="$HOME/Library/Application Support/CoD2-native-current-readiness/WS98.noindex"
mkdir -p "$E/integration"
git -C /Users/stix/Projects/cod2-native archive 048ff5e | tar -x -C "$E/integration"

taskpolicy -b nice -n 19 python3 -m unittest discover -s tools/catalog -v
sh tests/launcher/config_catalog.sh
taskpolicy -b nice -n 19 python3 tools/catalog/catalog.py check \
  --source-root "$E/integration" --catalog launcher/Resources/ConfigCatalog.json --mode stock
taskpolicy -b nice -n 19 python3 tools/catalog/catalog.py check \
  --source-root "$E/integration" --catalog launcher/Resources/ConfigCatalog.json --mode cod2x
taskpolicy -b nice -n 19 python3 tools/catalog/catalog.py check \
  --source-root . --catalog launcher/Resources/ConfigCatalog.json --mode stock
taskpolicy -b nice -n 19 python3 tools/catalog/catalog.py check \
  --source-root . --catalog launcher/Resources/ConfigCatalog.json --mode cod2x
```

The last two commands are expected to exit 1. Final logs record each field/site
difference; counts are differences, not counts of affected registrations.

| Source / resource | Mode | Settings | Registered commands | Dispatch-only commands | Differences / exit |
| --- | --- | ---: | ---: | ---: | --- |
| Integration `048ff5e` / committed | stock | 672 | 223 | 8 | 0 / 0 |
| Integration `048ff5e` / committed | CoD2x | 728 | 227 | 8 | 0 / 0 |
| Older worktree / committed | stock | 670 | 222 | 9 | 36 / 1, expected red |
| Older worktree / committed | CoD2x | 720 | 226 | 9 | 47 / 1, expected red |
| Older worktree / scratch source-specific | stock | 670 | 222 | 9 | 0 / 0 |
| Older worktree / scratch source-specific | CoD2x | 720 | 226 | 9 | 0 / 0 |

Integration restores `motd` and `cl_profileTextHeight`, adds CoD2x client/server
registrations (`sensitivity_mg`, `g_shotgun_spread_fix`,
`sv_playerBroadcastLimit` and the three query-limiter settings), restores the
quick-message spelling and `mr` registration, and differs in flags/ranges,
including `cl_maxpackets`, `loc_language`, renderer tweaks and client ownership.
CoD2x also differs in competitive-policy flag expressions and protocol facts.
The gate reports these differences without weakening the committed resource.
A scratch copy file removes authored overrides for names absent from the older
source; its matching resource also passes both modes. Both files are evidence,
not alternate shipped resources.

The 24 Python tests include RED-to-GREEN evidence for missing and extra names,
wrong type/default, lost registration sites, header inclusion/macros/evidence
lines, cyclic includes, constant table selection, C integer division, unknown
constructors/source selection, dynamic values, conditional flags and separation
of supplemental/dispatch entries. Removing all command-site evidence also fails.
Line-only source shifts do not fail coverage.
Authored copy cannot replace source facts.

The Swift runner uses `-swift-version 6 -strict-concurrency=complete
-warnings-as-errors -target arm64-apple-macos13 -O` and the current SDK. It checks
the real resource, mode-specific facts, protected controls, dynamic and ambiguous
values, Codable round-trip, actor transfer, malformed inputs, group/name lookup
and real bundle-resource loading/missing-resource errors. Final result:
`PASS: ConfigCatalog Swift 6 tests (733 settings, 235 commands)`.

Determinism is verified by two independent CLI generations and byte comparisons:

```sh
taskpolicy -b nice -n 19 python3 tools/catalog/catalog.py generate \
  --source-root "$E/integration" --source-revision 048ff5e --output "$E/regenerated-a.json"
taskpolicy -b nice -n 19 python3 tools/catalog/catalog.py generate \
  --source-root "$E/integration" --source-revision 048ff5e --output "$E/regenerated-b.json"
cmp launcher/Resources/ConfigCatalog.json "$E/regenerated-a.json"
cmp "$E/regenerated-a.json" "$E/regenerated-b.json"
```

All three files are identical: 1,850,561 bytes, SHA-256
`66910464a2513932f66809dcf6f7986e04c4f512fb9e56c3c278439d0b301ba4`.
`verification.json` records the final command exits and checksum.
Additional regression checks pass:
`taskpolicy -b nice -n 19 python3 -m unittest discover -s tools/tests -v`
(19 tests) and `sh tests/launcher/config_document.sh` (F1 byte-preservation suite).
`git diff --check` passes. These are bounded source/model checks; they do not
prove runtime reachability, live effective defaults, UI acceptance or gameplay
fidelity. Rerun the catalog gate after integration registrations change.

## Review, preservation and remaining work

A fresh Sol 6.1 xhigh reviewer examined `1a2c0da..fb5e4a1`. There were no
Critical findings. Four Important findings were fixed with failing/passing
regressions: omitted implementation headers, overclaimed table names, Python/C
division disagreement and unchecked CMake glob producers. Smaller findings
about mouse-rate/HUD copy, source lines and command-site validation were fixed
as well. The fixed diff has not had a second independent review.
An additional final negative case caught the all-command-evidence omission
bypass; its regression failed before `b27e053` and passes afterward. The six
source/resource coverage commands were repeated after that gate-only change.

No engine, CMake, typed snapshot, host-revision or existing launcher behavior was
changed. `git diff 1a2c0da --name-only -- src build/lp64_gen/data_native.c
tests/platform/server_control.c cmake CMakeLists.txt` is empty. No touched engine
file needs `unifdef -UCOD2_X64` plus `cmp`; this is not a substitute for the x86
object/binary gate. The full engine/release merge gate was not rerun for this F4
unit. Inherited known-red wave gates remain visible:

- `legacy-abi`: 10 differences, generated `build/lp64_gen/data_native.c` ×5
  (excluded by CMake OFF) and test-only `tests/platform/server_control.c` ×5.
- `tests/cod2x/check_inactive_gates.py`: 436 differences across intentional
  stock/native repairs and synthetic x64-defined-zero configurations.
- x86 Linux byte comparison requires unavailable x86_64 Linux multilib.

No game/GUI run, `.app` bundle or LaunchServices registration was made. No lease
was needed. Swift scratch executables/bundles were removed by runner/test cleanup;
the temporary source export and obsolete preprocessing/fact scratch were removed.
Retained evidence consists of logs, synthetic RED/GREEN receipts, QA scripts and
comparison JSON. Compilation/heavy checks used background task policy and nice
19, with at most two concurrent source-check workers and one Swift compilation
at a time. The real CoD2 Silicon home and licensed install were untouched.

F3 remains responsible for build/resource integration, native Guided/Expert UI,
runtime effective values and policy enforcement, profile transactions and history.
No work is blocked on a human playtest. Completion leaves the worktree clean.
