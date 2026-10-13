# WS107 — Config Studio B2 model layer

2026-10-09. Worktree `config-chains-b2`, branch `codex/config-chains-b2`,
base `e0f49b25d1c108cfb95b421a2425cc8eeb502271`. This unit supplies the
advanced-bind model, companion-file transactions, Windows export and focused
tests. It changes no view, presentation file, engine source or game data.

## Delivered behavior

`ConfigChains.swift` models direct multi-command binds, single-setting cycles
and command cycles. Setting cycles emit the engine's `toggle` value list and
use no helper dvars. Command cycles use one unarchived `set` helper per step
plus one unarchived pointer. They update that pointer with `set` after a step,
without immediate recursion. Both archived and unarchived helpers occupy the
runtime pool; the edit exposes every owned helper name and its exact count.
Pool capacity is 1280 for stock and 4096 for this native CoD2x build. Available
runtime slots are unknown, because registration and mod-created dvars are not
measured by the launcher. The count is not an available-slot estimate.

The triggering bind stays in `config_mp.cfg`. Only helper definitions go in
`main/<ConfigStudioLaunch.chainBasename(profileID:)>`, using WS101's 47-byte
hashed `cs_<profile>.cfg` naming contract. Empty helpers remove the companion;
F3 then omits `+exec`. No raw identifier concatenation or unconditional exec
was introduced. A session fixture proves the chain's `+exec` is the launch
argument suffix after profile setup. This does not prove engine filesystem
search under every mod; WS102/profile-path integration remains necessary.

Imported archived scripts serialize unchanged. `inspect` leaves vstr/exec,
comments and scripted mutation opaque, while showing their bounded effects.
Imported command cycles are never silently converted into owned definitions.
A companion without matching owned metadata, a changed companion, helper
name collision or malformed definitions refuses replacement. External or
in-game rebinding is preserved when clearing an old managed definition, and
Windows export retains the actual live bind bytes.

`ConfigChainMachine` consumes F1's byte splitter/tokenizer and maintains
symbolic string state across presses. It expands exec/vstr at the front of
the buffer, tracks changes before expansion, and distinguishes a repeated
immediate expansion from a wait-yielding loop. It catches mixed exec/vstr
recursion and permits terminating self redefinition. It bounds execution at
8192 commands and 64 expansion frames. Unresolved files/variables make traces
incomplete; they do not become a positive compatibility result. Unknown leaf
commands remain visible, without being executed or pretending to know their
gameplay effect. Registration coercion, enum-vstr behavior, server changes,
gameplay, key catchers and real frame timing remain outside this preview.

Independent traces cover leading `+`, leading whitespace, semicolon tails,
multiple `+` actions, and commands after apparent comments. Key/time append
to the whole leading-plus binding; release changes only its first byte. Tail
commands run twice and extra held actions receive no automatic release.
Time formatting also models the engine's signed `%i` interpretation of the
32-bit event time. Positive wait yields; competitive CoD2x wait does not.
Edits expose `usesWait` and the competitive `waitDisabled` warning, without
claiming that a particular connected server's policy is known.

## Limits and reference discrepancies

The compiler validates the emitted bytes after quoting, using F1 rather than
a second text grammar. Safe line maximum is 4094 bytes (engine clamps at
4095 and consumes another byte), tokens are bounded by 512 and their 8704-byte
storage, binds by 1023 bytes, and add-text configs by 65535 bytes. Leading-plus
binds reserve space for the worst key/time suffix in the 1024-byte event
array. Inserted exec/vstr text counts its added newline bytes against the
65536-byte buffer. Filenames use F3's basename within the 64-byte exec buffer.
New cyclic steps containing held `+` actions are rejected: a vstr trigger
cannot deliver their required release. New command rows must be complete and
contain no hidden comment effects; imported comments retain engine behavior.

The private Mac 1.3 reference confirms the 1024-byte event formatter and its
whole-binding press/release formatting. Its set join permits 4093 value bytes.
The supplied native engine still passes 1023 to `I_strncat` in
`Dvar_GetCombinedString_impl`, allowing only 1022 stored bytes. B2 therefore
enforces 1022 for generated helpers and simulates that truncation in imported
dependencies. A native oracle demonstrates why this matters: truncating an
imported `vstr safe` suffix to `vstr sa` can execute a different command.
`nativeSetTruncation` exposes the discrepancy; no imported bytes are rewritten.

**Integration dependency:** when the faithful engine wave repairs this join,
update B2's native limit/preview and the production oracle together. The
fixture currently asserts the checkout's 1022-byte behavior and will fail on
a changed join. Do not retain this workaround as reference behavior.

The stock native tokenizer also still drops escaped quotes, and native
CoD2x still differs from F1 on unquoted high bytes. New stock scripts requiring
embedded quotes and new unquoted high-byte commands are refused. Quoted
CoD2x helper strings are verified through the production tokenizer. Imported
bytes remain preserved. These refusals are explicit model limitations until
the corresponding engine repairs are composed; B2 does not change the engine.

## Persistence and export

F2 required a narrow additive extension: `ConfigProfileVersion.chain`,
`ConfigHistoryEntry.beforeChain/chain`, `ConfigStudioMetadata.chainDefinitions`
and `ConfigProfileStore.updateChain`. Optional fields decode old history and
versions. Ordinary edits preserve the current companion, and exact expected
versions include it. The existing flock, path checks, game guard, journal,
history retention and recovery disposition apply to all three files. Helper
text publishes before its trigger. A mixed transaction is journaled and
replayed, with one history row, before normal launcher reads/launches proceed.
Undo/restore recovers config, sidecar and companion, including absence.

Copied companions get a separate full creation receipt before publication.
Retrying the same duplicate request resumes an interrupted copy, including
a writer that wrote the companion and then threw, without an orphan conflict
or duplicate history row. A different request or externally changed bytes
remains a conflict. This is tested exception/retry behavior, not a physical
power-loss test or exclusion of an uncooperative writer between filesystem
checks. Uncompleted creation receipts are retained for the same-request retry.

Windows export compiles owned helper definitions and pointer mutations to
`seta`, preserving imported archived bytes and current triggers. It refuses
embedded quotes/backslash endings that the engine's unescaped archived writer
cannot preserve, including effective values changed by `set`, mutations on
press/release, and values observed while wait yields. Reachable exec files,
unresolved variables and unarchived vstr/setfromdvar sources are actionable
export errors; each effective engine key is visited once, and repeated full
symbolic variable/archive state must be reached within 128 presses. A longer
cycle receives `portablePreviewLimit` rather than an unsupported compatibility
claim; this unit does not silently package unknown external scripts.
The exported file is independent. Resolved/symlinked destinations inside the
active home are refused. No profile, selection or history is mutated by export.

## API for the UI unit

Include `ConfigChains.swift`, `ConfigImport.swift` and `ConfigImportSafety.swift`
in the composed launcher build; these types are compiled together by the new
focused runner. Existing view/build lists were not changed by this unit.

```swift
let definitions = try ConfigChains.ownedChains(read: session.current!)
let chain = ConfigChain(id: "fps", key: "j",
    behavior: .settingCycle(name: "com_maxfps", values: ["125", "250", "333"]))
let edit = try ConfigChains.prepare(chains: definitions + [chain], read: session.current!)
// Inspect edit.document/script, conflictingKeys, issues and runtimeVariableCount.
// Obtain I1 labels with ConfigChains.safetyReview(edit:catalog:).
try ConfigChains.apply(edit, to: session, replaceConflicts: false)
```

`ConfigChainBehavior` also has `.commands([String])` and
`.commandCycle(steps: [[String]])`. Supply the complete desired owned list to
`prepare`; an empty list clears owned definitions without rewriting imports.
`ConfigChainEdit` exposes `profileID`, `before`, `document`, `metadata`,
`script`, `chains`, `conflictingKeys`, `issues`, `runtimeVariables`,
`runtimeVariableCount`, `runtimeVariablePoolCapacity` and `hasChanges`.
Review conflicts explicitly before passing `replaceConflicts: true`.
Apply recompiles against the receipt; it does not trust altered edit fields.
The session overload is main-actor synchronous, refuses drafts/running games
or stale selection/version, and refreshes shared current/history on success.
The store overload returns the F2 history entry for non-UI clients/tests.

`safetyReview` accepts I1's `ConfigImportCatalog` protocol or its snapshot
implementation. The base F4 `ConfigCatalog` is not itself that protocol;
retain WS99's catalog-adapter seam when composing the review UI. Do not pass
an empty catalog and describe its unknown-command labels as researched safety.

`inspect(key:document:files:waitDisabled:)` returns `kind`, original `binding`,
`press` and `release`. Its initial variables come from the supplied document;
`files` resolves explicit exec calls, not implicit launcher startup. Use a
`ConfigChainMachine` to load the owned companion when previewing its helpers.
For sequential cycle
previews, initialize a machine with the document dialect and caller-reviewed
files, execute the profile then its companion, and call
`keyEvent(binding:key:down:time:)` repeatedly. `commands` contains leaf argv
as Data; `issues` and `complete` must stay visible. `variables` is symbolic.
`windowsExport(read:)` returns Data. `exportWindows(read:to:home:)` writes the
independent output. Bind failures to their localized actionable errors.

A caller-reviewed reload bind can use `.commands(["exec " + basename])`,
where `basename` comes from the F3 helper. The model does not overwrite F12
automatically. An exec-dependent reload bind requires dependency review for
portable export, since archiving helpers alone cannot reproduce file reload.

Locks check keys, directly named settings/actions and resolved old/new bind
effects, including imported vstr dependencies, later cycle presses and release
tails. With locks present, old/new cycles must repeat their complete symbolic
state within 128 presses; otherwise the edit refuses an unverified lock check.
I1 risk classification is exposed separately by `safetyReview`; the UI must display
the same review labels before applying risky user commands. This is not a
claim to classify unknown external files that were not supplied for review.

## Verification ledger

Evidence: `~/Library/Application Support/CoD2-native-current-readiness/WS107.noindex/`.
All compilation/heavy checks run under `taskpolicy -b nice -n 19`; the B2
runner uses Swift 6 strict concurrency, warnings as errors, macOS 13 and `-j 1`.
No game ran and no licensed data was accessed. The native/Swift model
fixtures open no window; the broader gate ran its existing launcher capture
harness with synthetic data.

Initial red (`red-api.log`) is absent-API compilation evidence. Behavioral
reds include imported recursion (`red-transitive.log`), all five review
findings (`red-review-behavior.log`, six assertions), stock quote drift
(`red-stock-quotes.log`) and a transient unsafe archive before wait
(`red-transient-archive.log`). Intermediate compiler/harness errors are kept;
they are not passes. One fixture expectation incorrectly omitted argument-join
spaces on unescaped archive reload; its exact expected bytes were corrected,
without weakening the assertion or changing production code.

Fresh Astra engineering review found one P1 and four P2 issues. Every finding
received a behavioral reproduction and a fix: native imported truncation,
resumable copy publication, setfromdvar durability, effective/transient
archived quote validation and old imported action locks. The full focused
suite is green after this single fix pass. No deferred review findings remain.

Focused commands:

```sh
TMPDIR="$HOME/Library/Application Support/CoD2-native-current-readiness/WS107.noindex/scratch" sh tests/launcher/config_chains.sh
TMPDIR="$HOME/Library/Application Support/CoD2-native-current-readiness/WS107.noindex/scratch" sh tests/launcher/config_profiles.sh
```

`chains-final.log` passed nine Swift groups and the production command
fixture with ASan/UBSan. `profiles-final.log` passed all 15 F2 cases. The
focused compilation is strict Swift 6, macOS 13, warnings as errors, `-j 1`.
The final self-audit added two behavioral reds:
`red-export-bound-mouse.log` reproduced an unarchived dependency after press
128, and `red-cycle-lock.log` reproduced an old locked action on the second
press. The original letter-key fixture was already refused because aliases
caused duplicate visits; `export-bound-diagnostic.log` keeps that investigation.
The independent mouse-key fixture demonstrated the actual gap. The final
state-bound correction is verified by `chains-complete.log`, including a
positive full 128-state export, the longer-cycle refusal and later-press lock
protection: ten Swift groups plus the production ASan/UBSan fixture passed.
This latest optimized focused run supersedes the earlier B2 pass.

The frozen broader command was:

```sh
COD2_LAUNCHER_TEST_OUTPUT="$HOME/Library/Application Support/CoD2-native-current-readiness/WS107.noindex/scratch/final.noindex" \
TMPDIR="$HOME/Library/Application Support/CoD2-native-current-readiness/WS107.noindex/scratch" \
taskpolicy -b nice -n 19 sh tests/launcher/run.sh
```

These commands ran with the private scratch directory present. Recreate it
before repeating them; cleanup removed it after all owned processes ended.

**RED:** `final-launcher.log` exited 1 at `snapshots.py:47`, the Home
assertion that dark and light captures differ. All preceding focused Config
Studio suites passed, including F1/F2/F3/F4/I1/A1/B1/B2. Font verification,
button-style audit and the macOS 13 fallback-view typecheck passed. The
capture harness produced 26 images and reported 200 audited shapes with zero
findings, but this is no visual pass: Home dark/light are identical, 2880×1800,
93458 bytes, SHA-256
`ea66def3d9d6105477ce392b36e46ed4a643bd8a5ffc1e04281541239c2fe81a`.
All 26 images share that hash. Original-resolution inspection shows an empty image; stdlib PNG decoding
confirms every RGBA and filter byte is zero. The assertion was not weakened.
Fallback captures did not run because the assertion stopped the suite.
The two private captures and `snapshot-receipt.json` retain this failure.
The other focused suites used the same final F2/session code; the later
self-audit changes affect B2 only and were rerun through its focused gate.
No OS/TCC/lock-state change was attempted. The earlier `full-launcher.log`
is also red and predates the final review fixes; it is not a green claim.

`sh -n tests/launcher/config_chains.sh tests/launcher/run.sh` and
`git diff --check` passed. `verification-receipt.json` records check results,
composition commits and cleanup. All created external scratch builds and the
Python bytecode cache were deleted. No `.app` bundle was created; a read-only
LaunchServices dump found no WS107 evidence paths to unregister. No bench
lease was taken, no unrelated process was stopped and no real game home was
written. Logs, private reference inspection, the two failed captures and JSON
receipts remain only in the evidence directory.

## Rulings and composition

- Extend F2/session narrowly: its existing two-file API could not journal a
  companion and trigger together. Keeping them separate would permit a bind
  to reference absent or stale helpers. Cost if wrong: the integration owner
  must adjust the additive receipt fields; no old fields or engine layout change.
- Honor the current native 1022-byte join and refuse stock quote/high-byte
  drift for new scripts, while recording reference behavior. Cost if wrong:
  conservative editing restrictions; lifting them requires matching engine
  and model changes, not accepting silently changed scripts.
- Hash stable chain IDs for helper namespaces independently of profile IDs.
  Only the selected companion loads; copies retain exact helper/bind identity.
  Cost if wrong: simultaneously executing multiple profiles' companions could
  reuse IDs. Imported name collisions refuse writes, and normal launch selects
  one profile.
- Keep imported scripted binds opaque; simulate only bounded symbolic command
  state and refuse incomplete portable exports. Cost if wrong: some valid
  custom exports need explicit archive/dependency review. Unknown execution
  does not become an invented green result.

Foundation commit: `c340319`; model/tests: `38017aa`; runner wiring: `0c3c778`;
cycle validation correction: `c5d63df`. The final report follows in the branch
log. Compose them in order, retaining the F3 optional-chain contract
and adding the model/import files to the UI unit's build list. No push, tag,
release, PR, remote change, integration merge or pre-existing commit rewrite.

Engine/OFF preservation is structural for this unit: no `src`, `build/lp64_gen`
or CMake file changed, so there are no engine files requiring per-file
`unifdef -UCOD2_X64` + `cmp`. The full native engine merge gate remains an
integration check, not a launcher fixture claim. Supplied baseline red gates
remain visible: legacy-abi 10 differences (generated data_native ×5, excluded
by CMake OFF; test-only server_control ×5), inactive-gates 436 differences,
and x86 artifact comparison unavailable on this Mac without Linux multilib.
Those counts are not reruns. `COD2X_HOST_REVISION=0` is unchanged.
