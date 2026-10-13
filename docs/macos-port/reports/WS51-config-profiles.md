# WS51 — Config Studio profiles, transactions, history and migration (F2)

**Status: RESUMED 2026-10-09; F2 complete and verified locally.** Config Studio has resumed under the current Readiness + Config Studio wave. This unit completes the parked profile store, independently reviews it against F2 and commits the implementation/tests separately from this report and the verified plan checklist.

Branch `codex/config-profiles` starts at `1a2c0da` (F1). `Core.swift`, `Model.swift`, launcher build lists and engine sources remain unchanged. This is store acceptance, not end-to-end Config Studio or release acceptance. No push, tag, PR, merge, game run or UI work was performed.

## Changes in the resumed pass

Six concrete defects were reproduced with failing disk fixtures before their fixes:

1. A running-game transition after staging config still allowed a sidecar write; migration could also mark completion after selection started a game. The central writer now checks the current callback immediately before every file write. History removals and transaction-journal removal are guarded, too.
2. An external sidecar edit during config publication was overwritten. Publication now rechecks the config/sidecar pair between file writes and before transaction completion; a conflict retains the external bytes and pending journal.
3. A symbolic-link ancestor of the supplied home redirected all writes into another tree. Path checks now traverse every ancestor. Only verified macOS `/var -> /private/var` and `/tmp -> /private/tmp` aliases are translated; arbitrary links still fail. Foundation retained these aliases in `resolvingSymlinksInPath()` and changed existing `/private/var` URLs back to `/var` in `standardizedFileURL`. The alias check therefore uses the actual link destination and applies the same normalization at every containment/ancestor check.
4. Invalid existing history was detected during pruning, after committing config bytes, leaving every recovery attempt blocked. History is now validated before beginning an update, before recovery publication and before adding a game-session snapshot. Malformed JSON reports `invalidHistory`; invalid files remain intact.
5. A damaged transaction could publish a snapshot whose stored revisions disagreed with its intended config/metadata. Recovery now validates both snapshot revisions against the journal before writing anything.
6. A raw home URL such as `/private/..` bypassed the root-folder rejection. The initializer now rejects `/` after normalization, before any store operation.

Additional fixtures verify that pending migration freezes the **initial** effective `GameSettings.dvars()` values/DPI even when a later input differs, and that completed migration preserves subsequent config edits and a different active selection. Restoring the migration's original revision also restores exact config bytes and the absence of a sidecar. The runner supports selecting a case and optional private corpus disk round-trips; test homes now live inside its private scratch directory and are removed even after a failing assertion.

A separate read-only reviewer found the ancestor/history defects independently and inspected their fixes; it did not run tests or provide a cross-model review. The final verification below is the primary agent's evidence.

## Consumer APIs

`ConfigProfileStore(homeURL:dialect:gameIsRunning:writeFile:)` requires an explicit writable home URL. The default writer uses the production `PrivateFile.write` convention; the injectable writer permits real disk-failure tests. All calls are synchronous and Foundation-based, and the store is `Sendable`. Separate instances serialize mutations with a private `flock` file. The running-game callback is checked inside the lock and immediately before every file write; mutation APIs also accept `runningGame: Bool = false`.

- `list()` returns actual `ConfigProfile` identifiers, display names and active state. Existing safe profile directories without a sidecar receive their identifier as a display name. Unsupported identifiers and symbolic-link directories are excluded from listing; explicit access to a link fails.
- `read(id:)` returns `ConfigProfileRead`: profile, `ConfigDocument`, decoded `ConfigStudioMetadata`, exact `data`, and `version`. `ConfigProfileVersion` contains the exact config bytes and exact optional metadata bytes. Pass this version back for conflict detection; do not reconstruct metadata JSON as an expected version.
- `create(id:displayName:document:metadata:runningGame:)` publishes a complete private staged directory atomically. `duplicate(id:as:displayName:expected:runningGame:)` preserves source bytes, copies metadata and records copied provenance. Neither implicitly changes selection.
- `activeSelection()` returns the first valid profile token and exact optional `originalBytes` from `active.txt`. `select(id:expectedActive:runningGame:)` requires those bytes to match; `nil` means the file must still be absent. It writes only the safe identifier, which the engine parses as its first token.
- `update(id:document:expected:metadata:source:summary:runningGame:)` commits one gesture. Optional replacement metadata is validated; omitting it preserves the original raw sidecar bytes. A stale config or stale metadata rejects the entire operation before journaling.
- `history(id:)` returns newest-first `ConfigHistoryEntry` values. Each includes its UUID, date, source, summary and both the previous/resulting config and sidecar bytes. Sources are You, Import, Check-up, Game session, Restore and Migration.
- `restore(id:snapshotID:expected:revision:runningGame:)` defaults to `.after`, restoring the selected result. `.before` supports undo or original migration recovery. Either revision restores its matching sidecar, including original absence, and creates a new `.restore` transaction.
- `recordGameSession(id:before:summary:runningGame:)` compares the observed result after exit against the pre-session version. Changed sessions create one snapshot without rewriting the game's config; unchanged sessions create none.

`ConfigStudioMetadata` version 1 holds display name, setting locks, notes, origin kind/source/credit, optional DPI and last-import comparison values. Locks are metadata here; import/check-up policy and UI enforcement belong to their later units. Profile identifiers have 1–41 ASCII bytes, start with a letter or number and contain letters, numbers, dots, underscores or hyphens. That fits the engine's 64-byte `players/<id>/config_mp.cfg` path buffer, including NUL, and avoids splitting the active token or future chain exec argument. The profile's display name is independent and supports Unicode.

## Disk and interruption behavior

New directories use mode 0700 and replaced files use mode 0600. Writes reject symbolic links in the explicit home, its ancestors and owned descendants (apart from the verified macOS system aliases described above). Files use `PrivateFile`'s exclusive temporary file, complete-write loop, file fsync and rename; this unit also fsyncs the containing directory. Create/copy stages config, metadata and the first snapshot privately before publishing the directory with Darwin `RENAME_EXCL`, so a raced-in empty target directory cannot be replaced. Interrupted unpublished staging directories remain invisible to listing and are excluded from empty-home migration source checks.

An update first persists `.studio-transaction.json`, then its snapshot, then config and metadata. Store reads and subsequent mutations recover pending transactions under the same lock, accepting only an original or intended state for each file and a matching snapshot in the journal. Existing history is validated before publication. Recovery reuses the same snapshot ID and does not duplicate history. A later unrelated external edit produces a conflict and retains the journal/snapshot for diagnosis; it is never overwritten automatically. This is recoverable multi-file publication, not a claim that two filesystem renames form one indivisible OS operation.

History lives beside each profile in `history/<uuid>.json`, retaining the newest 50 transactions. The original migration backup is separate from this cap. Invalid metadata, history and unsupported schema versions return errors and retain the original files.

## Explicit migration contract

`ConfigPersonalValues(effectiveLauncherDvars:dpi:)` takes the existing `GameSettings.dvars()` output before F3 changes launch semantics. It preserves personal defaults and advanced personal overrides, with case-insensitive last-wins matching. It excludes display/renderer machine settings, filesystem base/home paths, launcher housekeeping and credential names. DPI is positive finite metadata, not an invented game dvar.

`ConfigMigrationInput(source:expectedOriginal:personalValues:)` requires an explicit source:

- `.flattenedLegacy`: `<home>/main/config_mp.cfg`, the obsolete native CoD2x source. It is read only when the caller deliberately chooses it.
- `.profile(id)`: `<home>/main/players/<id>/config_mp.cfg`, the normal source. Its original config and existing sidecar are preserved.
- `.emptyHome`: fresh onboarding. The flattened config and every non-staging writable profile config must be absent, and `expectedOriginal` must be empty. Absence is rechecked after the initial backup and before publication/selection, so a raced-in source aborts migration.

`ConfigMigration.run(store:input:runningGame:)` returns a profile ID and `alreadyCompleted`. `ConfigMigration.originalBackup(store:)` returns the exact independent original bytes if a backup has been written. Migration snapshots the original to `main/players/.studio-migration/original.cfg` before editing, then persists the intended target and selection in `migration.json`. Parser rejection, disk failure or conflict cannot silently mark migration complete. A pending record resumes its frozen desired bytes; a completed record returns without reapplying settings or selecting the loadout again. Main is displayed as “Main”; an unrelated or reserved `Main` directory receives a distinct `Main_2`, `Main_3`, etc. target instead of being overwritten. Original sources are never deleted or renamed. Existing `fs_basepath` assignments are preserved byte-for-byte, and a launcher-supplied `fs_basepath` override is excluded.

## F3 and engine integration obligations

This store deliberately owns `main` configs and `main/players/active.txt`. The WS47 integration contract recorded in the parked report resolves native CoD2x profile config reads/writes under `<fs_homepath>/main`, with the full `players/<profile>/config_mp.cfg` path. Its engine implementation was not verified in this foundation branch. That contract does **not** redirect every profile-related operation: profile listing and active-file selection still need verification against the effective game-directory/search paths. F3 must establish the effective `fs_game` and select the appropriate active file before launch while preserving main ownership for CoD2x profile configs. Stock mod-specific config ownership is outside this main-only API. No flattening, base-path write or automatic legacy-source discovery is provided here.

F3 must serialize process launch/attach/exit transitions with mutations and supply a current guard covering every running engine, including attached processes. Advisory `flock` protects cooperating stores; it cannot lock a game or external editor that ignores it. Expected-byte checks happen before journaling and again after snapshot staging, but an arbitrary external writer can still race the final rename. Launcher lifecycle serialization is required for the game case. A pending transaction while a game runs returns the read-only error instead of replaying config writes. After an external conflict, F3 must keep the user's draft and provide an explicit recovery/disposition flow. A conflicting journal currently makes `read`, `list` and `history` return a conflict; a normal refresh does not clear it. This layer deliberately provides no silent journal discard or user-confirmed cancellation API. F3 must add that API with its recovery interaction.

F3 should retain old launch behavior until `ConfigMigration.run` actually succeeds. It should pass the entire legacy effective dvar list, preserve machine/housekeeping launch values separately, and avoid treating DPI as a game command. Archive the before-session version, permit live ordinary reads, and call `recordGameSession` only after the last attached/launched game exits. F3 must derive an exec-safe chain basename from the profile identifier: F2 identifiers permit dots and hyphens, while the approved chain-basename alphabet is letters, numbers and underscores. No chain-file editing, import classification, UI or runtime launch was added here.

## Design-board/API coverage

The 2026-10-09 read-only canvas snapshot was checked against the store contract:

| Board need | F2 capability | Deferred consumer work |
| --- | --- | --- |
| Loadouts and active marker | `list`, `create`, `duplicate`, `activeSelection`, `select` | F3 rail, rename controls and launch selection |
| Guided/Expert effective values and text | `read.document`, exact versions, `update` | F3 controls/drafts/editor; F4 catalog/defaults and rule evaluation |
| Locks and notes | Sidecar `locks` and `notes`, metadata-only updates and snapshots | F3 row actions and enforcement; import/check-up policy in their units |
| Origin, credit and comparison | Sidecar `origin.kind/source/credit`, `lastImportedValues`, other loadout reads | F3 comparison labels/ghost markers; directory player data in the import unit |
| History sources, summaries and differences | UUID/date/source/summary plus both config/sidecar revisions; current `read` for Now | F3 localized source labels, title/detail/diff formatting, filters and confirmation |
| Restore/undo | Expected-version `restore`, either revision, a new Restore snapshot | F3 native undo and confirmation; explicit external-conflict disposition |

No new UI-specific store capability was required. A History import label can use the snapshot's stored origin, rather than the profile's later origin. Detailed change copy comes from the two revisions plus catalog metadata. The same expected version covers a metadata-only gesture.

## Verification

Evidence: `~/Library/Application Support/CoD2-native-current-readiness/WS51.noindex/`. All compilation and tests ran under `taskpolicy -b nice -n 19`, with no more than two simultaneous compiler processes. No private corpus contents were printed or committed.

The resumed baseline command `sh tests/launcher/config_profiles.sh` passed the parked eight-case suite (`baseline.log`). The added regressions produced these behavioral reds (exit 133 from the test runner's fatal assertion):

| Log | Failure before fix |
| --- | --- |
| `red-metadata.log` | `FAIL: expected conflict` |
| `red-running.log` | `FAIL: no additional file write after the game starts` |
| `red-path.log` | `FAIL: expected unsafePath` |
| `red-history.log` | `FAIL: invalid history is rejected before committing config bytes` |
| `red-journal.log` | `FAIL: expected invalidHistory` |
| `red-normalized-root.log` | `FAIL: expected invalidHome` |

`green-history-path.log` and `path-diagnosis.log` record intermediate system-alias compatibility failures (`unsafePath`), not passes. The latter is a synthetic-path diagnostic run; no private config contents were printed. It was diagnosed and corrected before final acceptance. Four synthetic homes left by the earlier fatal assertions were identified within this unit's test log creation/completion windows and removed (`failed-test-cleanup.txt`). Later test homes are inside runner scratch and clean up on failure.

Final commands and results:

```sh
sh tests/launcher/config_profiles.sh
# PASS: 14 focused cases with the normal macOS temporary root.

# Final run also uses the normal macOS temporary root.
sh tests/launcher/config_profiles.sh --corpus \
  "$HOME/Projects/cod2-native-wt/_agents/research/corpus/windows-players"
# PASS: 14 cases: profiles, exact-byte transactions, conflict/running guards,
# recovery, fifty-entry history and idempotent migration
# PASS: private corpus store/create/copy/history round trip,
# 34 files, 581727 bytes, both dialects; content withheld

TMPDIR="$HOME/Library/Application Support/CoD2-native-current-readiness/WS51.noindex/scratch" \
  taskpolicy -b nice -n 19 sh tests/launcher/config_document.sh --corpus \
  "$HOME/Projects/cod2-native-wt/_agents/research/corpus/windows-players"
# PASS: config document byte preservation, splitter/tokenizer,
# effective edits, limits and insertion
# PASS: corpus round trip, 34 files, 581727 bytes, both dialects; content withheld

taskpolicy -b nice -n 19 bash scripts/build-launcher.sh \
  "$HOME/Library/Application Support/CoD2-native-current-readiness/WS51.noindex/scratch/core" --test
taskpolicy -b nice -n 19 \
  "$HOME/Library/Application Support/CoD2-native-current-readiness/WS51.noindex/scratch/core/LauncherTests" \
  tests/launcher/fixtures
# PASS: real master/status fixtures, malformed packets, colours,
# native key CRC/private storage, config and URL injection,
# frame cap 0–1000 (250 by default)
# PASS: cm/360, advanced dvar forms, settings migration, release notes,
# maps, server facts, media, key tag, player name and scrims

sh -n tests/launcher/config_profiles.sh
git diff --check
test -z "$(git diff --name-only 1a2c0da -- src build/lp64_gen CMakeLists.txt)"
# All exit 0; no engine/build-source changes.
```

The F2 runner compiles production Core/Presentation/ConfigDocument/ConfigProfiles/ConfigHistory with Swift 6, complete strict concurrency, warnings as errors, optimization, the current SDK and an arm64/macOS 13 target. Its native support objects compile sequentially. `final-profiles-default.log`, `final-profiles-corpus.log`, `corpus.log` and `core.log` retain the results. Scratch build/test directories are deleted before finish; only evidence logs/receipts remain (`verification.json`, `scratch-cleanup.txt`). No QA app bundle or LaunchServices registration was created.

These tests prove the bounded model/disk contracts, including simulated write failures and observed callback transitions. They do not prove actual power-loss durability, uncooperative writers racing the final rename, launch/attach serialization, mod-specific ownership or rendered UI acceptance. Normal reads may create the private store lock/directory bookkeeping; while the game runs, they cannot replay pending user-data writes. No API follows a config's `fs_basepath` value, and the synthetic linked-home test leaves its separate data tree untouched. The real licensed install and the real CoD2 Silicon data folder were never written.

## Integration limits and known red release gates

F3 must compose the lifecycle guard and explicit journal-conflict recovery before shipping UI integration. F4, chain editing, import classification and the engine profile-path work remain separate units. The full engine/release merge gate, game and launcher snapshot harness were not run for this store-only change.

The wave's known red gates remain visible and unchanged: `legacy-abi` has 10 differences (generated `build/lp64_gen/data_native.c` ×5, excluded by CMake OFF; test-only `tests/platform/server_control.c` ×5); `tests/cod2x/check_inactive_gates.py` has 436 differences across intentional stock/native repairs and synthetic x64-defined-zero configurations; x86 Linux artifact comparison needs unavailable x86_64 Linux multilib. These counts are the supplied wave baseline, not new runs or green claims. `COD2X_HOST_REVISION=0` was not changed. No engine file was touched, so there is no per-file `unifdef -UCOD2_X64`/`cmp` obligation in this unit; full native-OFF binary preservation remains the integration gate.

The implementation/tests are committed as `928fa3f`; the report/plan form the second focused DCO-signed local commit. The orchestrator can compose only these commits onto its release lineage; nothing was pushed, tagged, released or merged, and nothing is intentionally left uncommitted.
