# WS106 — Config Studio A1 model layer

Completed 2026-10-09 in `codex/config-checkup-a1`, based on integration
`6342c32b12b9e3d682f6c24b8121fca8ea8f7715`. This unit adds models, resources
and focused tests. It adds no views or engine behavior.

## Delivered behavior

`ConfigRules.swift` loads versioned, source-backed policy data, resolves
inheritance and returns `ok`, `warning`, `violation`, `unknown` or `conflict`
for a specific saved setting. `ok` means that the value matches the cited
setting constraints; it does not certify a player, a league, a mod installation
or a live connection. Missing values, unknown policy IDs, unresearched settings
and incompatible target modes return `unknown`.

`ConfigRulesets.json` version `2026-10-09.2` contains eight bounded policies and
133 authored rules (inherited rules are not counted twice):

| ID | Authored rules | Coverage |
| --- | ---: | --- |
| `stock-1.3` | 8 | Relevant stock registration domains |
| `public-cod2x` | 8 | Relevant CoD2x registration domains, including snaps 40 and packets 125 |
| `cod2x-competitive` | 9 | Seven competitive constraints, disabled wait and server ownership of g_competitive |
| `zpam-3.36` | 42 | Stock-client 125–250 band and client-pushed values, including snaps 30 and packets 100 |
| `zpam-4.08` | 44 | CoD2x competitive policy, pushed values and additional mod-owned helpers; includes pub |
| `esl-eu-2008` | 13 | Archived numeric client requirements, not the entire league rulebook |
| `wwcl-2007` | 8 | Selected V13.1 LAN numeric requirements, not the entire rulebook |
| `polish-2026` | 1 | The printed packet-100 recommendation, explicitly advisory and contradictory |

Every authored rule includes a source URL/URI, source date, quote reference,
excerpt and enforcement mechanism. Primary inputs were the fresh ruleset
research, its raw extracts, the F4 catalog and local integration engine source
at `048ff5e`. Seven additional mod-owned helper references were independently
checked against the pinned zPAM 4.08 source. Local `git:048ff5e:...` citations
identify actual local engine objects; they are not fabricated public URLs.

Known contradictions are retained as sourced records: WWCL/ESL fx_sort,
ESL/zPAM scoped damage icons and Polish packet 100 versus CoD2x packet 125.
Selecting contradictory policies returns `conflict`; `relevantConflicts`
also exposes related conflicts when only one target is selected. No generic
PAM, CAL or unknown-version policy was invented. Paper and advisory requirements
are distinguishable from engine clamps and mod pushes.

`ConfigCheckup.swift` and `ConfigOptimizer.json` provide eight authored rules:
competitive frame cap 250, optional unfiltered mouse input, optional zero
mouse acceleration, client-version rate/snapshot/packet ceilings, conservative
texture detail and full texture detail. Each suggestion contains exact old/new
values, area, title, explanation, observable effect, evidence and sources.
Tune-for modes are competitive, balanced and bestLooking. Input preferences
are not presented as league requirements or measured improvements.

Texture advice explicitly includes `r_picmip_manual 1` when needed. Primary
engine `R_SetPicmip` evidence shows that automatic mode can otherwise replace
the saved texture level. Both changes are reviewed and applied together;
a lock on either required change suppresses the card. The explanation also
discloses that manual mode retains saved normal-map and specular mip levels.

Advice never writes FOV, locks, metadata or server-owned settings. Unknown
policies, unresolved exec/vstr/toggle/reset dependencies and parser diagnostics
withhold suggestions. Direct wait commands and effective binds containing
wait are flagged; obsolete unbound bindings are ignored. Unresolved scripts
also prevent positive judgments about direct settings. This is a bounded
static review, not a transitive script execution engine or complete bind audit.

Four dial models expose p99 frame time, mouse polling, ping and an unmeasured
visibility state. Missing, invalid or unprovenanced measurements display
`Not measured`. There are no invented performance scores, GPU headroom estimates,
polling-rate rankings or packet-loss estimates. Suggestions use configuration
and code evidence, never label that evidence as a measurement. A2 can supply
explicit measurement values and provenance later.

## UI binding contract

The integration owner must include `ConfigRules.swift` and `ConfigCheckup.swift`
in the launcher build, and bundle `ConfigRulesets.json` and `ConfigOptimizer.json`
beside the existing catalog resource. Shared build lists were outside this
unit's scope and were not changed.

```swift
let checkup = ConfigCheckup(
    catalog: try ConfigCatalog.load(),
    rules: try ConfigRules.load(),
    optimizer: try ConfigOptimizer.load()
)
let snapshot = try profileStore.read(id: profileID)
let report = checkup.evaluate(
    profile: snapshot,
    rulesetIDs: ["zpam-4.08"],
    tuneFor: .competitive,
    targetMode: .cod2x,
    measurements: ConfigCheckupMeasurements(),
    dismissedSuggestionIDs: []
)
let receipt = try checkup.apply(
    report: report,
    suggestionIDs: Set(confirmedSuggestionIDs),
    store: profileStore,
    runningGame: runningGame
)
let undoEntry = try checkup.undo(
    receipt: receipt,
    rulesetIDs: currentRulesetIDs,
    store: profileStore,
    targetMode: currentTargetMode,
    runningGame: runningGame
)
```

Use `report.suggestions` for cards and Show me (`changes`, `why`, `notice`,
`evidence`, `sources`); `report.dials` for measured values/units/statusText;
`report.policy` and `report.limitations` for the bounded readiness result.
`ConfigRules.evaluate(setting:value:rulesetIDs:mode:)` is the reusable per-setting
API. `ruleset(id:)`, `resolvedRules(id:)`, `isServerOwned(_:rulesetIDs:)` and
`relevantConflicts(rulesetIDs:)` expose policy metadata and ownership.

Pass the intended target catalog mode explicitly: it is independent of the
document parser dialect. Stock target advice proposes snaps 30/packets 100;
CoD2x target advice proposes snaps 40/packets 125. The selected policy must
match that mode. Do not infer verified mod versions from a loose fs_game name.

Not now is a caller-owned set of suggestion IDs passed into a new review; it
writes nothing. Scope that set to the reviewed profile/version/context. Refresh
the report when profile, disk bytes, locks, target mode, policy or tune-for
selection changes. After Apply/Undo, reread the profile and regenerate cards.
Render local git citations as source references rather than attempting to open
an unsupported URL protocol.

Apply rederives selected suggestions from the current snapshot, checks that
they exactly match the reviewed objects and performs one `ConfigProfileStore`
update with history source `.checkup`. An empty, stale or invented selection
cannot publish. Undo performs one store restore of the before snapshot. It
restores exact original bytes, including pre-existing rule violations, and
requires the current policy so newly server-owned settings cannot be changed.
Both operations retain the store's game-running and config/metadata revision
guards. The UI must use the same F3-serialized store/process ownership boundary
as other profile mutations; this model does not own launch transitions.

## Verification and limits

These commands passed. Focused runners compile Swift 6 with complete strict
concurrency, warnings as errors and macOS 13 deployment. New runners use
`taskpolicy -b nice -n 19`, `-j 2`, synthetic inputs and private temporary homes.
All verification compilation was serialized across commands.

```sh
sh tests/launcher/config_rules.sh
sh tests/launcher/config_checkup.sh
sh tests/launcher/config_document.sh
sh tests/launcher/config_profiles.sh
sh tests/launcher/config_catalog.sh
sh tests/launcher/config_import.sh
```

Rules tests exercise every authored constraint's relevant boundary or exact
value, evidence metadata, client-version split, ownership and contradictions.
Check-up tests exercise every optimizer rule, exact disk changes and byte-exact
Undo; multi-setting texture changes; case-insensitive locks; protected/FOV
preservation; game guards; disk and metadata conflicts; unknown targets;
unresolved scripts; effective wait binds; measurement provenance and dismissal.
Expected red receipts precede the fixes for missing policy/suggestions, integer
scientific notation, unresolved-script judgments, explicit target mode and
automatic texture-mode replacement.

Launcher core regression tests also passed:

```sh
taskpolicy -b nice -n 19 sh scripts/build-launcher.sh "$WS106_CORE_OUTPUT" --test
taskpolicy -b nice -n 19 "$WS106_CORE_OUTPUT/LauncherTests" tests/launcher/fixtures
```

The actual private build directory was
`~/Library/Application Support/CoD2-native-current-readiness/WS106.noindex/core-build.noindex`
and was removed after testing. No app bundles, game windows or GUI captures
were created; no benchmark lease was needed. No performance benchmark was run.

Local preservation checks against the pre-unit integration commit passed:

```sh
python3 tools/abi/legacy.py --base 6342c32b12b9e3d682f6c24b8121fca8ea8f7715
python3 tests/cod2x/check_inactive_gates.py --base 6342c32b12b9e3d682f6c24b8121fca8ea8f7715
python3 tests/fixes13/legacy.py --base 6342c32b12b9e3d682f6c24b8121fca8ea8f7715
git diff --check
```

Each source preservation check had **zero changed engine files**, so these
passes are vacuous for engine preprocessing. No engine file requires an
`unifdef -UCOD2_X64`/`cmp` comparison in this unit. Engine source, native-OFF
definitions, generated typed data and `COD2X_HOST_REVISION=0` are unchanged.
This does not establish a green global engine ABI or binary parity gate.

The shared wave's known red gates remain visible and unweakened: legacy-abi
has 10 differences (generated data_native.c ×5, excluded by CMake OFF, and
test-only server_control.c ×5); inactive gates have 436 differences across
intentional stock/native repairs and synthetic x64-defined-zero configurations.
Those historical comparisons were not rerun here. The x86 Linux multilib
comparison is unavailable on this Mac. Full composed engine merge gates and
gameplay/runtime fidelity checks remain the integration owner's work. The full
launcher GUI/snapshot runner was not run for this model-only unit.

Evidence remains at
`~/Library/Application Support/CoD2-native-current-readiness/WS106.noindex/`:
red/green/final logs, per-run verification logs, `verification.json`, the
private verification driver, pinned zPAM tree/provenance receipts and
`engine-preservation.json`. All scratch build/test directories created outside
the worktree were removed. Licensed data and the user's real game home were
not written. Nothing was pushed, tagged, merged or sent to external people.

Code commits are `5057274` (rules/policy) and `7db134d` (Check-up/guarded writes).
All unit changes, including this report, are committed with DCO sign-off.
No uncommitted work remains.
