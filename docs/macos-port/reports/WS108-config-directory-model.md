# WS108 — Config Studio I2 models and directory preparation

2026-10-09. Branch `codex/config-directory-i2`, worktree
`/Users/stix/Projects/cod2-native-wt/config-directory-i2`, base `e0f49b2`.
Model layer only: no SwiftUI views, presentation, gameplay, renderer, engine
source or build-list changes. Nothing published, pushed, tagged, released,
merged or rewritten. No player corpus or licensed data was read.

## Import review and apply

`ConfigImportSession` is a Sendable value owned by the review UI. It consumes an
I1 bundle and a `ConfigImportCatalog`; `ConfigImportCatalogAdapter` supplies the
F4 registration/protection flags conservatively across all registration sites.
Unknown protection flags cannot become safe editable settings. Review defaults
to a new UUID-named loadout, with safe entries selected and risky entries
unchecked. Import everything selects only eligible entries. Credentials,
protected settings, unknown forwarded commands and raw exec references remain
non-importable. Comments are review material, not selectable writes.

Rows expose grouped plain-language titles, previous/proposed values, redacted
I1 details/effects, classifications, lock names and platform explanations.
Old bindings containing credentials are redacted too. Changing mode or target
resets selection to the corresponding safe defaults. Choosing an existing target
captures its config and sidecar bytes without changing `active.txt`.

Preview assembles selected original command bytes with explicit command
boundaries, preserves the prior file as its prefix, then reparses and reclassifies
the combined result. It rejects altered token boundaries, ignored NUL suffixes,
omitted vstr definitions, worsened safety classifications and locks reached
through the combined dependency graph. Reviewed dependency commands from ordinary
file/folder imports can be selected; their original exec references are never
emitted. This is a reviewed merge, not a simulation of original exec ordering.
The final config must fit the 65,535-byte command buffer.

Locks include case-insensitive setting names, `bind:<engine-key>` aliases,
`action:<command>` and bare action names. Direct commands, nested scripts,
unbindall, replacement of old bindings and changes to helpers reached by existing
locked actions are checked. Locks and existing metadata survive an existing-target
import. New imports carry imported origin metadata; only selected direct values
enter `lastImportedValues`, never blocked source text.

Windows translation preserves portable setting values and engine key identities.
Display/resolution/refresh choices, launcher machine settings, renderer choices,
Windows driver selections and unsupported or Windows-origin Command key mappings
are explained and omitted. No guessed resolution, Ctrl/Command swap, sensitivity
conversion or unverified driver replacement is applied. The UI should direct the
user to Settings or Binds to make those choices explicitly. Original directory
config files remain unchanged.

Confirmed apply uses one guarded F2 creation/update and one `.import` history row.
It neither selects a new profile nor changes Main by default. A successful session
receipt prevents another apply. Expected config and metadata bytes are checked
again by F2 inside its lock, alongside its game-running callback. Rejected parser,
lock, revision, dialect, ownership and first-write failures retain prior bytes and
the import review. Interrupted durable F2 writes retain F2's existing journal and
recovery/roll-forward semantics; this unit does not promise rollback after a
partial filesystem failure or physical power loss.

Two small additive foundation seams were necessary to honor the same-store
contract: F2 `create` accepts optional history `source` and `summary` parameters
(old defaults unchanged); F3 exposes its dialect, reads explicit import targets
and commits reviewed imports through its existing private store. No raw store is
exposed to views and no F2/F3 implementation was replaced. A pending draft blocks
import commits. Refresh failure after a successful session commit publishes its
error while still returning the successful receipt.

## Exact UI binding API

| Purpose | API |
| --- | --- |
| Catalog seam | `ConfigImportCatalogAdapter(catalog: ConfigCatalog)` conforms to I1 `ConfigImportCatalog`. Use it for `ConfigImportLoader` and `ConfigImportSession`. |
| Create review | `try ConfigImportSession(bundle:dialect:catalog:displayName:platform:origin:)`. `platform` defaults to `.native`; use `.windows` for WindowsProfiles candidates and Windows directory entries. Optional `origin` is a `ConfigOrigin`; directory callers can supply the verified source URL and credit. |
| Read review | `review`, `rows`, `mode`, `target`, `selectedIDs`, `receipt`. A row exposes `candidate`, `id`, `title`, `group`, `oldValue`, `newValue`, `lockedNames`, `platformNote`, `canSelect`. Display I1's redacted `candidate.details` and `effect.details`, not raw document/token bytes. |
| Edit review | `setMode(.review/.everything)`, `setTarget(.newLoadout(id:displayName:)/.existing(read))`, `try select(id, selected: Bool)`. Nonselectable rows cannot be enabled. |
| Capture target | `try studio.readImportTarget(id:) -> ConfigProfileRead`; use that exact read in `.existing`. `studio.configDialect` supplies the correct dialect. Neither call changes active selection. |
| Show final change | `try session.preview() -> (document: ConfigDocument, metadata: ConfigStudioMetadata)`. It performs no disk writes. |
| Confirm once | On MainActor, `try session.apply(to: studio) -> ConfigImportReceipt`. Receipt has `profileID` and `historyEntry`. The direct `apply(to: ConfigProfileStore)` overload is for model/test clients with the same guarded store. Views should use the studio overload. |
| Internal F3 seam | `commitReviewedImport(id:displayName:document:expected:metadata:)`. I2 alone constructs/rechecks its inputs; UI confirmation should call the import session instead. |

The UI agent/integration owner must add `ConfigImportSession.swift`,
`ConfigImportCatalogAdapter.swift` and `ConfigDirectory.swift` to its composed
launcher build/typecheck lists, alongside the existing I1/F4 models. This unit
deliberately did not edit the shared view/build lists. The new focused runners
compile the actual production files, including the F3 seam.

## Verified directory and honest unavailable state

`ConfigDirectory` has no production configuration by default. `load()` returns a
real unavailable explanation and does not load an unsigned bundled scaffold or
bootstrap trust from a cache. No production public key, endpoint, player,
permission grant or signature has been fabricated.

Configured loading uses CryptoKit's Ed25519 API (`Curve25519.Signing`) to verify a
raw 64-byte detached signature over the exact index bytes with an explicitly
supplied raw 32-byte public key. It validates schema version 1, positive publication
revision, unique IDs, closed metadata objects, credit, source, public-redistribution
permission records, dates, profile length and SHA-256. The index is bounded to
1 MiB and 2,048 entries; configs are bounded to 65,535 bytes.

The cache retains exact signed bytes and is reverified against the caller's trust
configuration on every load. A cached key never establishes authority. Network
outages and HTTP 5xx can return a verified cache with an explicit refresh error;
signature, identity, policy and malformed-cache errors remain errors. A corrupt
cache is not silently discarded or overwritten. Revision rollback and changed
bytes at an existing revision are rejected. A filesystem lock and a fresh cache
read immediately before replacement prevent a slower request from overwriting a
newer revision. This protects a retained cache, not a fresh install from an older
correctly signed publication.

Profile retrieval requires an entry ID from a verified snapshot, the exact expected
final URL, byte count and SHA-256. It returns a normal I1 bundle, so the ordinary
safety review and import apply still run. Raw exec dependencies are not fetched or
trusted merely because their parent config is signed. Offline cache supports
directory browsing; downloaded profile bytes are not cached here. Already imported
loadouts remain ordinary local F2 profiles.

Saved bookmarks persist IDs under the directory cache lock. They expose metadata
only from the supplied verified index. If an entry disappears, its bookmark
remains explicitly unavailable and can be removed; stale unsigned metadata cannot
appear as a verified player.

| Purpose | API |
| --- | --- |
| Unavailable production state | `ConfigDirectory(cacheURL:)`; `try await load() -> ConfigDirectoryLoad`. Load exposes `snapshot`, `unavailableReason`, `refreshError`. |
| Future trust setup | `try ConfigDirectoryConfiguration(indexURL:signatureURL:publicKey:)`, then `ConfigDirectory(configuration:cacheURL:fetcher:)`. Index/signature must be HTTPS siblings without URL credentials/query/fragment. Fetcher is the existing bounded I1 protocol. |
| Browse/details | `ConfigDirectorySnapshot.index: ConfigDirectoryIndex` has `schemaVersion`, `revision`, `entries`; `origin` is `.network` or `.cache`. Entry fields match the schema, including narrative/style, credit/source, permission, platform and download identity. Snapshot construction is private to verified loading. |
| Fetch for import | `try await directory.profile(entryID:snapshot:dialect:catalog:) -> ConfigImportBundle`; feed that into `ConfigImportSession`. A valid signature is never a safety approval. |
| Saved | `try directory.savedEntries(snapshot:) -> [ConfigDirectorySavedEntry]`; each has `id` and optional verified `entry`. `try directory.setSaved(entryID:saved:snapshot:)` updates bookmarks. |

## Unpublished standalone repository preparation

`tools/config-directory/` is ready to copy as the `cod2-configs` repository root:
entry/index JSON schemas, an intentionally empty unsigned index, deterministic
generator/check mode, readable INDEX.md, count-only review receipts, production
Swift grammar/safety auditing, Ed25519 signer/verifier and a CI check workflow.
Permission/credit/schema failures, blocked content, symlink inputs and stale
generated artifacts reject the generator. Config bytes are never rewritten.
Permission metadata requirements do not prove that a grant is genuine; a maintainer
must review the evidence before publication.

Standalone CI needs separately configured `MODEL_REPOSITORY`, an immutable full
`MODEL_REVISION` containing the composed models, `PUBLICATION_BASE_URL` and
`INDEX_REVISION`. These have no made-up defaults. It checks without deploying or
signing. A present signature additionally requires a separately reviewed
`trust/public-key.ed25519`. Signing key ownership/custody/rotation, actual hosting,
launcher trust configuration and real permissions remain external delivery work.
The hosted workflow has not been run and nothing has been published. Test keys
are generated ephemerally, kept only in trapped scratch space and never shipped
as a trust root. Tool compilation supports the host's arm64/x86_64 architecture;
launcher model runners explicitly target macOS 13 arm64.

## Verification and evidence bounds

Evidence directory:
`~/Library/Application Support/CoD2-native-current-readiness/WS108.noindex/`.
Every compiler/heavy test runs under `taskpolicy -b nice -n 19`; new Swift runners
use Swift 6, complete strict concurrency, warnings as errors, macOS 13 and `-j 1`.
At most two of this unit's compile jobs run concurrently. All inputs are authored
synthetic fixtures or committed source/catalog metadata. No live game, QA app,
GUI lease, network publication, private player data or real app home was used.

```sh
evidence="$HOME/Library/Application Support/CoD2-native-current-readiness/WS108.noindex"
mkdir -p "$evidence/scratch"
TMPDIR="$evidence/scratch" sh tests/launcher/config_import_session.sh
TMPDIR="$evidence/scratch" sh tests/launcher/config_directory.sh
TMPDIR="$evidence/scratch" sh tools/config-directory/test.sh

# Existing focused runners, run sequentially under background priority:
for unit in document profiles launch catalog import bindings rules checkup; do
  TMPDIR="$evidence/scratch" taskpolicy -b nice -n 19 \
    sh "tests/launcher/config_${unit}.sh"
done

git diff --check e0f49b2 HEAD
test -z "$(git diff --name-only e0f49b2 HEAD -- src build/lp64_gen CMakeLists.txt \
  launcher/RootView.swift launcher/SettingsView.swift launcher/ServersView.swift launcher/Theme.swift)"
```

The final I2 import and directory runners pass. The tool runner passes all six
generator tests, actual signing-tool round trip, tamper/wrong-key rejection and
empty-artifact check. Logs are `import-session-final.log`, `directory-final.log`
and `directory-tools-final.log`. All eight existing focused runners pass: F1
document, F2 profiles (14 cases), F3a launch/lifecycle, F4 catalog (733 settings,
235 commands), I1 import, B1 bindings, A1 rules and A1 Check-up. Their exit codes
and commands are retained in `focused-regression-receipt.json` and per-runner logs.

Red-first API absence logs precede implementation for both models; generator
absence produces three failures among six tests (negative-input tests can already
reject missing tooling). Behavioral reds additionally caught nested platform
toggles, an existing locked action's helper replacement, machine/Command choices
and a slow directory request overwriting a newer verified revision. Saved API
absence was observed before its implementation. One initial test incorrectly
expected an unterminated block comment to swallow later logical commands; it was
corrected to F1's actual command-boundary semantics and replaced with a NUL-suffix
case. Strict-concurrency and standalone Swift-entry-point compiler errors were
fixed; intermediate failures are not counted as passes. These tests prove bounded
model/filesystem/cryptographic contracts, not real player consent, hosted CI,
physical crash durability, UI rendering or gameplay fidelity.

The full launcher GUI/snapshot suite and full engine merge gate were not rerun for
this model-only unit. Existing WS99/WS101 snapshot failures remain visible:
home-dark was 2824×1764 instead of an accepted size, with the later WS101 capture
black in a locked-session environment. No assertion or capture gate was changed.
`tests/launcher/run.sh` now calls both new model runners and the tooling runner;
the UI/integration owner still needs composed build lists and visual acceptance.

No engine file changed, so no touched-engine-file `unifdef -UCOD2_X64`/`cmp` proof
is required. Native-OFF object/binary identity and the full composed engine gate
remain integration checks. Supplied known red baselines are unchanged and were
not rerun: legacy-ABI 10 differences (generated native data ×5, excluded by CMake
OFF; test-only server control ×5), inactive-gates 436 intentional stock/native
repairs and synthetic x64-defined-zero differences, and x86 comparison requiring
unavailable Linux multilib. `COD2X_HOST_REVISION=0` remains locked.

## Composition

Compose `6433317` (directory/cache/Saved), `012714e` (import apply and small F2/F3
seams), `1c68ae7` (standalone tooling and runner wiring), then this report commit.
All commits use the requested author email and DCO sign-off. There is no unit
blocker. The honest unavailable production directory and outstanding hosted
publication/trust/permission work are deliberate user requirements, not a fake
completed online directory. Combine with the UI unit and its build-list changes;
this branch alone creates no import or directory views.

All owned external scratch compiler outputs and synthetic homes are removed after
the checks finish. Logs and receipts remain in the evidence directory. No QA app
was created or registered, no owned game process or bench lease remains, and
nothing is intentionally left uncommitted.
