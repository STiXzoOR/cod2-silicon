# WS109 — Config Studio A3 server readiness model

2026-10-09. Worktree `config-ready-a3`, branch `codex/config-ready-a3`, base `e0f49b25d1c108cfb95b421a2425cc8eeb502271` (`codex/config-studio-int`). Implements the model portion of A3 from `docs/superpowers/plans/2026-10-07-config-studio-remaining.md`, spec §14 and Appendix B. No views, presentation files, engine source, licensed data or release lineage changed. The ready-check sheet and Servers UI remain the UI unit's work.

## Behavior and policy boundaries

`ConfigReadyCheck` consumes F2's immutable profile read, F4's catalog, A1's `ConfigRules`/`ConfigRulesets.json`, a queried `GameServer`, the client mode and the source loadout's optional official advanced chain. It returns `.matchesKnownRules`, `.needsChanges` or `.unknown`. A match means only that directly readable values match the listed cited rules; it never certifies a league, the whole config, actual enforcement or live compatibility. Policy data is reused unchanged. No new league limits or inferred fixes were authored.

Readiness requires a valid canonical endpoint and an actual `statusResponse` observed within the last 60 seconds. Old/cached data lacking the new optional observation date remains unknown. An `infoResponse` alone can populate the browser but cannot establish readiness. Different values repeated in a status packet are rejected. Conflicts between status and info, or differently cased keys, are retained and suppress positive judgments when they concern identity or evaluated rules. A clean info fallback after rejected/missing status explicitly loses any status timestamp.

Identity mapping is conservative:

| Advertisement | Policy selection or result |
| --- | --- |
| Protocol 118, explicit empty `fs_game` or stock `mod=0`, no contradictory CoD2x identity | `stock-1.3`, only in A1's compatible mode. |
| Protocol 120 or `shortversion` starting `1.4.`, explicit stock mod identity and `g_competitive=0` | `public-cod2x`. |
| Same CoD2x identity and `g_competitive=1` | `cod2x-competitive`. |
| Exact `_zpam=3.36` or exact `fs_game=zpam336`, compatible stock identity | `zpam-3.36`. |
| Exact `_zpam=4.08` or exact `fs_game=zpam408`, compatible CoD2x identity and no explicit competitive-off contradiction | `zpam-4.08`. |
| Generic `fs_game=zpam` with exact supported `_zpam` version | The corresponding policy above. |
| `sv_ruleset` containing an additional known A1 ID in the same policy mode | Adds that cited policy to the base selection. |
| Unknown/missing protocol, unidentified or arbitrary mod, unversioned zPAM, conflicting versions, unknown ruleset or unsupported mode | Unknown; no fixes. |

The `_zpam` identity key was checked against pinned upstream [3.36 default_mp.cfg](https://github.com/eyza-cod2/zpam3/blob/0ed8a460cddaffb7de0c1b54ce115c4c8dcdcbe6/source/default_mp.cfg) and [4.08 default_mp.cfg](https://github.com/eyza-cod2/zpam3/blob/1cfd9fb873206c35054e880ccb1d9eedaf1ec52c/source/default_mp.cfg). `zpam_version` and `_pam` aliases are deliberately unverified. `zpam-provenance.json` records source revisions, paths and SHA-256 values. These are unauthenticated server claims; identifying a policy does not prove enforcement. The root's current app is CoD2x-only, as in F3a. A stock policy incompatible with A1's CoD2x mode remains unknown rather than inventing a cross-mode interpretation. A future stock wrapper must pass `.stock`.

Advertised `sv_fps`, `sv_minrate`, `sv_maxrate` and rule variables are retained as facts. Malformed numeric facts, contradictory rate bounds or cited rule contradictions make readiness unknown. A rate outside advertised bounds produces a warning, not a claim about effective runtime rate. Custom `sv_maxfps`/`g_maxfps` and enabled or malformed legacy `com_maxfps_limit` have no verified policy mapping and remain unknown. Missing client values needed by non-server-owned rules remain unknown. Server-owned values are explained without offering edits. Competitive `com_maxfps=333` explains that 333 is unavailable and offers A1's validated 250 replacement when eligible.

Effective binds are parsed through F1; actual `wait` commands trigger the disabled-wait warning, while text such as `say "wait"` does not. External configs, dynamic commands, parser diagnostics and an executable official advanced chain make the review unknown. Empty/comment-only chains can be assessed and their exact bytes enter the stale-review proof. The model explicitly reports frame pacing, mouse polling and packet loss as **Not measured**. Status ping is only a query round trip. There is no invented Appendix B measurement or green performance score.

Fixes are offered only for known policy violations that are unlocked, catalog-editable, numeric, client-owned and supported by the small startup override allowlist. Proposed values must pass both the catalog's literal bounds/type and A1 policy evaluation. Protected settings, credentials, paths, server-owned rules and wait binds are never silently fixed. Unknown reports expose an empty fix dictionary.

## One-connection ownership and lifecycle

`ConfigServerConnection` stages an endpoint, optional alternative saved loadout and reviewed fixes entirely in memory. Endpoint normalization lowercases hosts and supplies the existing default port. A different endpoint, a different chosen loadout, explicit clear or a successful cold launch clears the staged choices. A failed launch retains the in-memory choice for retry while cancelling its reservation and removing the temporary disk state. Fresh review must match the same endpoint, profile ID, exact config/sidecar version, client mode, ruleset IDs, advertised facts, advanced-chain bytes and proposed fixes before a launch can consume overrides.

An alternative loadout without fixes is selected for this connection through F3a; its ordinary in-game edits continue to get history. The previous `active.txt` bytes are durably archived and restored after cancellation or the final exit, including launcher restart. A later game/user selection wins and is never overwritten. If the previous profile was deleted, it is not recreated and cannot permanently block cleanup or a later launch.

**Archived startup values are written back by the engine.** Merely appending `+set com_maxfps 250` would change the saved profile. For a fixed connection, F3a instead duplicates the reviewed source into a private disposable `csconn` + 32 lowercase UUID characters profile, archives the exact original selection before publication, and selects that copy as the game's write owner. F2's expected-version and flock checks guard the copy; F3 also verifies the reviewed version and exact chain bytes before allocation. The source config and sidecar remain byte-for-byte unchanged. Disposable profiles are hidden from normal Studio profiles/current/history; current still presents the source loadout. The existing source chain naming contract is retained.

**All in-game edits in a fixed disposable connection are discarded along with its fixes.** The source saved loadout is preserved rather than attempting to merge engine-serialized values back. This is the deliberate model behavior the ready-check UI must communicate when offering “Fix for this server only.” Ordinary sessions and alternatives without fixes retain normal F3a history. Final exit/cancellation restores the original selection only if our exact selection still owns it, deletes only the disposable copy with matching copied-source provenance, and removes the durable session archive. If the original profile was deleted while the disposable remained active, cleanup removes only our exact active selection and allows F3a's existing next-launch fallback; it does not guess another saved loadout.

`GameLaunchPlan` accepts connection values only with a saved Config Studio launch, a matching server link, no demo and an isolated write owner for nonempty fixes. Numeric allowlisted `+set` values are appended after the official chain and forwarded arguments, then pass the existing byte/31-command gate. Passwords continue through the existing Apple-event link delivery and never enter argv. Direct-connect and deep-link startup share this path. Joining while an owned/attached game runs uses the existing link path and clears staged startup fixes, without mutating saved configs or starting another engine. F3a's running/discovery/reservation guards, undisposed-draft guard and migration failure behavior remain in force.

## Exact non-view API for the UI unit

All root/session/connection methods are `@MainActor`. Pure report/evaluation and launch values are `Sendable`. Bind to `LauncherModel.serverConnection` as an observed object; the root also emits its existing change notification after root staging gestures. Bind profile state to the existing observed `configStudio` session. A nil ready-check result means the checker/session/loadout is unavailable and must be shown as unknown, never ready.

| Surface | API and intended use |
| --- | --- |
| Report | `ConfigReadyReport`: `address: String?`, `profileID: String`, `expectedVersion: ConfigProfileVersion`, `advancedChainBytes: Data?`, `mode: ConfigCatalogMode`, `rulesetIDs: [String]`, `status: ConfigReadyStatus`, `summary: String`, `details: [ConfigRuleEvaluation]`, `facts: [String:String]`, `fixes: [String:String]`, `limitations: [String]`. Preserve the scope/limitations and rule citations in presentation. |
| Status | `ConfigReadyStatus.matchesKnownRules`, `.needsChanges`, `.unknown`. A positive state is scoped to listed saved-value rules. |
| Pure evaluation | `ConfigReadyCheck(catalog:rules:)`; `evaluate(server: GameServer?, profile: ConfigProfileRead, mode: ConfigCatalogMode, advancedChain: ConfigDocument? = nil) -> ConfigReadyReport`. |
| Root review | `readyCheck(for address: String) throws -> ConfigReadyReport?`; reviews the chosen connection loadout or current saved loadout, with its official chain. |
| Single-endpoint refresh | `refreshReadyCheck(for address: String) async throws -> ConfigReadyReport?`; re-queries even a direct endpoint absent from the master list, replaces its browser result, does not choose a loadout. Display refresh failures as unknown. |
| One-connection loadout | `useLoadoutForServer(_ id: String, address: String) throws`; validates the saved profile without selecting it globally. |
| Temporary fix gesture | `fixForServerOnly(_ address: String) throws`; stages the current verified fix set. Existing `connect`, `deploy`, `openLink` and `play` consume the staged choice; views should not allocate disk copies or build argv. |
| Connection read state | Published private-set `address: String?`, `profileID: String?`, `overrides: [String:String]`. `profileID=nil` means use the current saved loadout. |
| Connection model | `select(address: String?) throws`, `clear()`, `useProfile(_ id: String) throws`, `fix(report: ConfigReadyReport) throws`, `launch(for: LaunchLink, profile: ConfigProfileRead, report: ConfigReadyReport) throws -> ConfigConnectionLaunch?`. `clear()` keeps the selected endpoint and clears staged choices. |
| Review errors | `ConfigReadyCheckError.noVerifiedFix` and `.staleReview`, both localized. Refused/stale review requires refresh/review, not fallback application of old values. |
| F3 read additions | `readProfile(_ id: String) throws -> ConfigProfileRead`, `readAdvancedChain(profileID: String) throws -> ConfigDocument?`; do not write selection or discard drafts. |
| F3 root-only extension | `prepareLaunch(settings:forwarded:connectionProfileID:connection:connectionExpectedVersion:connectionChainBytes:) throws -> ConfigStudioLaunch?`; all new arguments default to nil. Nonempty fixes require the reviewed source version and chain proof. Existing callers remain compatible. |
| Core launch extension | `ConfigConnectionLaunch(address: String, values: [String:String])`; `GameLaunchPlan(..., connection: ConfigConnectionLaunch? = nil)`. `ConfigStudioLaunch.connectionIsolated` defaults false; only F3 marks its disposable owner true. |
| Network facts | `GameServer.observedAt: Date?`, `queryConflicts: [String]?` are optional/backward-compatible Codable additions; `mergeInfo(_:)` retains contradictions. Only fresh status parsing sets the observation date. |

The root selects/clears connection state when existing `selectedServer` or `directAddress` changes. UI loadout pickers should use `useLoadoutForServer`, not F3a's global `selectProfile`. Existing direct-address/password fields remain authoritative for connection delivery. No ready-check view, Servers view code, Root/Settings view, Theme or presentation source was changed. Build lists and app resources include the A1/F4 model policy resources so production readiness does not silently depend on a source checkout.

## Red-first evidence and independent review

Evidence: `~/Library/Application Support/CoD2-native-current-readiness/WS109.noindex/`. The initial new runner failed for the absent model file (`red-ready.log`); this is API-absence evidence, not a runtime red for every behavior. Subsequent meaningful behavioral reds were retained:

- `red-engine-write.log`: simulated engine serialization changed Main's saved config before a disposable write owner was introduced, exit 133.
- `red-unobserved.log`: status lacking an observation was incorrectly judged positive, exit 133.
- `red-legacy-fps.log`: the unverified legacy FPS flag was incorrectly judged positive, exit 133.
- `red-info-fallback.log`: the real local UDP regression trapped before contradictory/rejected status stopped a clean info fallback being promoted; the optimized precondition prints no descriptive assertion text (SIGTRAP).
- The first `full-launcher.log` passed the preceding focused suites, then trapped in the new deleted-prior-profile cleanup regression with `missingProfile`, exit 133. It never reached snapshots.

An early engine-write fixture was mistakenly run against a still-reserved session and correctly received `gameRunning`; a later chain-staleness fixture reused cleared connection state. Both fixtures were corrected. Intermediate compilation/fixture failures are not passes. The final focused runner covers the corrected behavior, not just these initial diagnostics.

A read-only internal `review_ws109` agent was invoked by the applicable requesting-code-review skill. It found four P2 problems: deleted-selection cleanup, unreviewed source chain, unverified legacy FPS flag, and contradictory status erased by info fallback. Follow-up review also required passing the reviewed source revision into F3 publication. All findings were fixed and reviewed closed; no remaining blocking source-level finding was reported. `review.json` retains that verdict and its limits. The reviewer performed no independent compile/runtime test and did not change files.

## Verification

Every compile/heavy runner was executed under `taskpolicy -b nice -n 19`, sequentially, at most two compiler jobs. The new ready runner uses Swift 6, complete strict concurrency, warnings as errors, arm64 macOS 13, `-j 1`, and a trapped task-private scratch directory. Existing focused runners are wired into `tests/launcher/run.sh` next to readiness; all existing model policies/fixtures remain intact. Tests use only private synthetic config homes and a local ephemeral UDP peer. No real game process or engine window was started and no human playtest was requested.

```sh
evidence="$HOME/Library/Application Support/CoD2-native-current-readiness/WS109.noindex"
mkdir -p "$evidence/scratch"
TMPDIR="$evidence/scratch" taskpolicy -b nice -n 19 sh tests/launcher/config_ready_check.sh
# PASS, 11 behavior groups; final-ready.log and the integrated full run.

TMPDIR="$evidence/scratch" taskpolicy -b nice -n 19 python3 tests/launcher/network.py
# PASS: real local Network.framework UDP, fixture/parser, RTT, cancellation,
# contradictory status/info, rejected status plus clean fallback; green-info-fallback.log.

TMPDIR="$evidence/scratch" COD2_LAUNCHER_TEST_OUTPUT="$evidence/scratch/final-full" \
  taskpolicy -b nice -n 19 sh tests/launcher/run.sh
# Final outcome recorded below; do not infer a green full suite from focused passes.

TMPDIR="$evidence/scratch" taskpolicy -b nice -n 19 sh tests/launcher/input_devices.sh
# Final standalone input-device outcome recorded below.

taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base e0f49b25d1c108cfb95b421a2425cc8eeb502271
taskpolicy -b nice -n 19 python3 tests/cod2x/check_inactive_gates.py --base e0f49b25d1c108cfb95b421a2425cc8eeb502271
taskpolicy -b nice -n 19 python3 tools/abi/legacy.py --base e0f49b25d1c108cfb95b421a2425cc8eeb502271
git diff --check
test -z "$(git diff --name-only e0f49b25d1c108cfb95b421a2425cc8eeb502271 -- src build/lp64_gen CMakeLists.txt)"
```

The ready runner's 11 groups cover cited competitive limits and absent measurements; the existing `status.bin` and synthetic exact/missing/conflicting identities; waits/scripts/locks/ownership; password/direct/deep-link argv boundaries; stale/unsafe fixes; alternative selection/restart/read-only reservations; simulated engine writes that preserve source bytes; later selections and failed-start cleanup; deleted original selection and future launch; command-limit cleanup; and advanced-chain/version review before isolated publication. These are model/transport/filesystem contracts, not licensed-engine or actual Apple-event acceptance. Existing bindings tests exercise InputDevices as well; the standalone runner is also recorded for the full focused-runner requirement.

`final-input-devices.log` passes the standalone runner, exit 0: 1,029 production SDL conversions, 98 engine key names, ABC/Greek/AZERTY/QWERTZ/Dvorak/JIS/unsupported-key fixtures, labelled device guesses, overrides and preserved binding identity. The integrated bindings runner additionally passes capture cleanup and profile/history/lock/conflict contracts.

**The final full launcher suite is red, exit 1.** `final-full-launcher.log` passes Core, the real local UDP peer, artwork, F1, all 14 F2 cases, F3a, F4 (733 settings/235 commands), imports, A1 (8 policies/133 rules), checkup, bindings/input devices, all 11 ready groups, fonts, button-style checks and the complete Swift 6/macOS 13 launcher typecheck with warnings as errors. The unchanged snapshot harness compiles, produces all 26 captures and reports 200 shape checks with zero findings, then `snapshots.py:47` fails because home-dark and home-light are byte-identical. Their dimensions are the accepted 2880×1800, but the image is blank/black-looking when inspected at original resolution. The fallback capture stage does not run. This is not visual acceptance and is not a green full suite. Read-only `ioreg` reports `CGSSessionScreenIsLocked=true`, consistent with a capture-environment limitation; the precise compositor cause is not established by this unit. No snapshot assertion, view code, capture session or privacy setting was changed. A working capture environment must rerun that existing gate. `snapshot-home-dark.png`, `snapshot-home-light.png`, `snapshot-receipt.json` and the read-only session-state probe preserve the evidence.

The base-relative preservation scripts pass with zero changed engine files: fixes13 checks ten configurations, inactive checks seventeen and legacy ABI checks five, all with zero mismatches. These are bounded no-engine-diff checks, not a green global release gate. No engine file was touched, so the per-file `unifdef -UCOD2_X64` plus `cmp` proof has an empty file set. Shell syntax and Python `ast.parse` checks pass; the standalone test build's copied catalog/rules JSON exactly match the source resources. An actual release/QA app bundle was not built by this unit. `verification.json` and `source-preservation.json` retain commands, exit codes, scope and limitations.

## Composition, remaining gates and cleanup

Compose this branch on the specified integration base: `942706e` (readiness model, launch isolation and focused tests), then `d041961` (root/UDP/build-resource wiring and real local-peer regression), then this report's commit. All commits use the requested author email and DCO sign-off. No pushes, tags, PRs, releases, remotes, merges or pre-existing commit rewrites were performed. The UI unit should consume the API above and disclose the disposable connection's discarded in-game edits.

**WS102 is an engine integration dependency.** As recorded by F3a's `WS101-config-launch.md`, the base's released CoD2x engine still flattens profile config writes into `main/config_mp.cfg`. The disposable-profile guarantee requires the repaired per-profile engine contract. This model branch cannot establish persistence isolation against that unrepaired engine; do not ship per-profile Config Studio/fixed connections before WS102 and the integrated engine checks. No live engine serialization, cold/live Apple-event or Config Studio view test was performed in this unit. Optional chain files are rechecked before preparation but are not frozen against an uncooperative external writer after preparation. Restart tests prove durable intent handling at the filesystem API level, not physical power-loss durability.

The supplied wave-wide red gates remain visible and unchanged: legacy ABI **10 differences** (`build/lp64_gen/data_native.c` ×5, CMake OFF-excluded, and test-only `tests/platform/server_control.c` ×5); inactive gates **436 differences** across intentional repairs and synthetic x64-defined-zero configurations; x86 Linux comparison requires unavailable x86_64 Linux multilib on this Mac. Those counts are user-provided baseline facts, not this branch's reruns. `COD2X_HOST_REVISION=0` remains locked. Native-OFF preservation and the engine merge gates remain integration checks. No Metal or other 0.3 engine feature was started.

Cleanup is finalized and receipted before the report commit. All owned external scratch builds, private test homes and downloaded primary-source scratch are removed, retaining logs, review/provenance/verification receipts and any required capture failure evidence under `WS109.noindex`. No game lease was needed or taken. No QA `.app` bundle was created or registered, no process not started by this unit was killed, and no real user data folder, game data, TCC/privacy/global setting or global dependency was changed. No files are intentionally left uncommitted.
