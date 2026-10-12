# Config Studio foundation implementation plan

> **For agentic workers:** Use the current ce-work orchestrator's bounded units and verification contracts. Engineering uses Sol 6.1 xhigh; design uses Astra high. The user has authorized implementation and autonomous checking.

**Goal:** Deliver the native Configs page, faithful config editing, loadouts, migration and History described in sub-project 1 of the Config Studio spec.

**Architecture:** Byte-preserving Foundation models own parsing and disk transactions. An observable main-actor store connects those models to native SwiftUI views. Catalog metadata comes from the current engine's registrations; research supplies evidence and authored explanations, never private game assets.

**Tech stack:** Swift 6 strict concurrency, SwiftUI/AppKit, macOS 13+, Liquid Glass on supported SDK/OS, Python source-analysis tooling and existing launcher test scripts.

**Spec:** `docs/superpowers/specs/2026-10-06-config-studio-design.md`, especially §§2–8, 13, 15–17 and Appendix A. Visual reference: saved canvas DesignRules, ConfigsGuided, ConfigsExpert, ConfigsLight, History and Settings boards.

## Global constraints and settled decisions

- Plain language by default; dvar names only in Expert and Edit as text. No FOV or invented crosshair-colour control.
- User confirmed on 2026-10-07: new advanced chains use launcher-owned `main/cs_<profile>.cfg`, loaded with `+exec`; preserve imported `seta` chains; provide Export for Windows.
- User confirmed Windows profile import. Preserve the current 40/125 network defaults while accurately distinguishing upstream competitive enforcement from unconditional startup defaults.
- User authorizes all available automated and local runtime checks; no manual playtest approval dependency. Public release remains a separate delivery action.
- Keep original i386 behavior unchanged. No game assets, extracted shaders, decompiler dumps, private profiles or credentials in git.
- Config root is `<home>/main/players/<profile>/config_mp.cfg`; selection is `<home>/main/players/active.txt`. Engine fidelity follow-up must fix the current CoD2x path collapse before end-to-end profile acceptance.
- Ordinary valid controls save as you go: one transaction per committed gesture, not per slider tick. Imports, Check-up and restore use explicit confirmation. Every transaction snapshots; keep 50 snapshots per loadout.
- Read-only while the engine runs, including attached processes. Refresh changes and record a Game session snapshot after exit.
- Preserve existing launch/onboarding/deep-link behavior. Stop forcing personal dvars only after migration succeeds.

## Review focus

- Latin-1 bytes, CRLF, duplicate assignments and unknown commands survive unrelated edits byte-for-byte (unit F1).
- Partial migration or repeated launch must not overwrite the original profile or repeat initial settings changes (F2).
- Game launch races and external profile edits must not be overwritten by stale UI state (F2/F3).
- Catalog reports dynamic/server/mod-only/unavailable entries honestly; no fabricated defaults or editable cheat-protected controls (F4).
- Long translated text, narrow window, light/dark, increased contrast, reduced transparency/motion and keyboard-only use retain usable controls (F3).

## F1 — Config grammar and edited spans

**Files:** new `launcher/ConfigDocument.swift`, `tests/launcher/ConfigDocumentTests.swift`, focused test runner `tests/launcher/config_document.sh`. Synthetic fixtures may be authored; machine-local research corpus stays outside git.

**Interfaces:** `ConfigDialect` stock/cod2x; `ConfigDocument.init(data: Data, dialect: ConfigDialect)`; `serialized() -> Data`; effective setting/bind lookup; mutating setting/bind edits that throw meaningful errors before lossy serialization. Preserve spans, command separators and untouched suffix bytes. Expose parsed commands and tokens with byte ranges for later safety review.

- [ ] Characterize source and independent grammar fixtures before implementing. Assert an untouched document serializes to exact input bytes, including high-bit bytes, BOM, NUL suffix and CRLF.
- [ ] Add failing tests for quote-parity splitting, escaped quote tokenization, semicolons after comments, duplicate last-wins assignments, bind limits and canonical-vs-script insertion.
- [ ] Implement the engine's actual splitter and tokenizer; canonical insertion only after full canonical-shape validation. No UTF-8 normalization of existing bytes.
- [ ] Verify focused synthetic tests and all local corpus files round-trip without printing their content. Verify actual mutation preserves unrelated byte spans. Record counts and unresolved differences.

## F2 — Profiles, atomic transactions and migration

**Files:** new `launcher/ConfigProfiles.swift`, `launcher/ConfigHistory.swift`, `tests/launcher/ConfigProfilesTests.swift`, focused test runner; later integrate `Core.swift`/`Model.swift` only after foundation units compose.

**Interfaces:** `ConfigProfileStore` initialized with explicit home URL (testable temporary roots); list/create/duplicate/select loadouts; read/update document; metadata locks/notes/origin; history list/restore. Updates accept expected original bytes or equivalent disk version and running-game state. `ConfigMigration` imports effective existing launcher personal values into Main exactly once with recoverable original snapshot. Methods return real errors; never silently mark migration successful.

- [x] Write tests that create two profiles, switch active.txt, apply one setting, inspect exact on-disk bytes, and restore a snapshot as a new history transaction.
- [x] Prove read-only running-game behavior, stale-file conflict behavior, 50-snapshot retention, idempotent migration and preservation of failed-migration originals before implementation.
- [x] Implement private atomic writes using existing PrivateFile conventions, safe profile identifiers within engine path limits, sidecar `studio.json`, history and initial migration.
- [x] Verify no operation modifies licensed fs_basepath; run tests in fresh private temporary homes. Document the active-profile launch contract for F3 and the engine integration.

**Verified 2026-10-09:** 14 focused disk/model cases, private store/create/copy/history round-trips of all 34 corpus files in both dialects, F1 corpus checks and existing launcher Core tests pass. Six resumed-pass defects have behavioral red/green evidence. See `docs/macos-port/reports/WS51-config-profiles.md` for exact commands, bounds and F3 obligations. Process lifecycle serialization, explicit journal-conflict disposition, UI and engine/mod launch ownership remain F3/integration work.

## F4 — Complete catalog and coverage gate (independent of F1/F2)

**Files:** new `tools/catalog/`, `launcher/ConfigCatalog.swift`, `launcher/Resources/ConfigCatalog.json`, `tests/launcher/ConfigCatalogTests.swift`; own catalog tests/build checks. Do not copy private extraction tools wholesale.

**Interfaces:** Codable/Sendable catalog entries with stable names, type/default/range/flags, availability, purpose group, typed control, plain title/summary/details, sources and guided flag; registered commands carry bindable/menu labels. `ConfigCatalog` loads resource data and exposes settings/commands by name and group. Clearly separate actual stock/CoD2x/port registrations from reference-only and script/mod entries.

- [x] Inspect current source registration mechanisms and test a missing registration, extra claimed registration and mismatched type/default all fail coverage.
- [x] Generate factual metadata from current source; author copy for menu-exposed and competitive/network settings first, with honest generic technical reference descriptions for the remaining Expert entries. Avoid claims not established by source.
- [x] Include dynamic values as dynamic, protected/read-only entries as such, and corrected names `r_swapInterval`, `r_anisotropy`, `mp_QuickMessage`.
- [x] Verify every actual registration is represented under both native build modes; conditional/indirect/table registrations need explicit source evidence.

WS98 verification targets readiness integration `048ff5e`: both native modes have zero catalog differences; the older foundation engine has expected differences and passes with its own scratch-generated resource. Swift 6/macOS 13 tests and deterministic JSON regeneration pass. See [WS98 report](../../macos-port/reports/WS98-config-catalog.md) for commands, counts and bounded claims. Run coverage against integration source again after later engine fixes/Metal merge; this check-off verifies the named snapshot, not future registration changes.

## F3 — Native Settings and History integration (depends F1/F2/F4 and Astra design primitives)

**Files:** new `launcher/ConfigStudioModel.swift`, `launcher/ConfigsView.swift`, `launcher/ConfigSettingsView.swift`, `launcher/ConfigHistoryView.swift`; modify `Model.swift`, `RootView.swift`, `Main.swift`, `SettingsView.swift`, `Core.swift`, build scripts and launcher tests. Use Astra-owned Theme.swift/ConfigStudio presentation primitives without independently redesigning them.

**Interfaces:** observable main-actor store owns selection, effective document, draft controls, search/group/filter/mode, transactions, errors and history. Root model exposes the store and synchronizes engine lifecycle. Existing GameLaunchPlan adds active-chain exec after profile initialization and retains machine/housekeeping settings only after migration.

- [ ] Update launch/migration tests first: selected profile's personal values survive launch; machine settings and advanced options remain valid; chain filename cannot split command line; failed migration retains previous working launch semantics.
- [ ] Add Configs navigation and actual loadout rail. Guided/Expert share eight purpose groups and effective values; provide typed controls, per-setting reset/locks/notes, changed/locked filters, search, comparison and text editor with validation/undo.
- [ ] Connect History list/restore and running-game banner to real stores. Use native undo/keyboard focus and Command-K. Never ship disconnected buttons or fictional player/loadout data outside the test preview mode.
- [ ] Move personal settings out of app Settings and wire resource/build lists. Preserve renderer/display/update/privacy/advanced machine settings.
- [ ] Run all launcher unit tests, strict Swift 6/macOS 13 fallback typecheck, current-SDK build and snapshot harness. Inspect actual rendered light/dark/Reduce Transparency screens against canvas, and exercise filesystem results through real UI actions where harness permits.

## Integration and completion

The orchestrator reviews actual diffs, commits completed units with DCO and composes them after the 0.2.1 gate baseline. A fresh Sol review checks profile ownership, data preservation, launch ordering and coverage completeness; Astra checks visual fidelity. This plan is foundation only; Binds, Import/Directory and Check-up plans cover the remaining spec sections and remain in the user's authorized 0.3 scope.
