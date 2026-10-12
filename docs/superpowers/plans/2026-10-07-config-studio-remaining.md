# Config Studio remaining sub-project plans

> **For agentic workers:** Execute bounded units through the ce-work orchestrator, Sol 6.1 xhigh engineering and Astra high design. The user authorized all four spec sub-projects and autonomous verification. Do not treat this as authorization to publish unpermissioned player configs or private research.

**Goal:** Complete Binds, Directory/import, Check-up, local match summaries and the server ready check after the foundation.

**Architecture:** Build on ConfigDocument, ConfigProfileStore, ConfigCatalog and the main-actor Config Studio store from the foundation plan. Views consume the shared Astra primitives and `docs/macos-port/config-studio-ui-contract.md`; they never own independent persistence or classify imported text themselves.

**Tech stack:** Swift 6 Foundation/SwiftUI/AppKit, local event monitors and existing IOKit input enumeration, CryptoKit for directory signatures, Python tooling, native-gated C engine instrumentation.

**Spec:** `docs/superpowers/specs/2026-10-06-config-studio-design.md` §§5, 7, 9–17 and Appendices A/B. Apply every global constraint in `2026-10-07-config-studio-foundation.md`.

## Review focus

- Imported commands after apparent comments, recursively resolved exec/vstr and plus-action releases must match engine semantics (B2/I1).
- A changing keyboard layout or device must not silently alter existing bindings; focus loss cancels capture and releases held state (B1).
- Import review and all advice writes obey locks, game ownership and disk revision checks from the same store (I2/A1).
- Unknown rules and absent measurements must never appear as a positive compatibility or performance judgment (A1/A3).
- Retention and privacy toggles must work through disconnect/shutdown without allocations in the gameplay loop or altered i386 behavior (A2).

## B1 — Native binds and device mapping

**Own:** new `launcher/ConfigBindings.swift`, `launcher/InputDevices.swift`, `launcher/ConfigBindsView.swift`, focused launcher tests; coordinate build list with root integration owner.

**Consumes:** catalog command metadata, document bind lookup/mutation, store transaction/locks/process state; Astra StudioKeyRole/StudioKeyCap.

**Produces:** testable physical input → engine key mapping, detected device/layout model with explicit user overrides, capture lifecycle, primary/secondary action mapping and real List/Visual views.

- [ ] Pin the current engine's SDL mapping and key-number names in synthetic fixtures. Compare ABC, Greek, AZERTY, QWERTZ, Dvorak and JIS mappings; explicitly retain unsupported-key outcomes. Reference evidence does not support mapping literal § to key0xa7, which is F1.
- [ ] Implement ANSI/ISO/JIS shape and current input-source labels; update labels on input-source change without rewriting binding identity. Device size guesses are labeled and overridable.
- [ ] Implement local-event capture, cancel/focus/game-start cleanup, conflict review and Mouse1–5/wheel. Consume captured input so Return/Space cannot activate another control.
- [ ] Implement action list and keyboard/pointer visual inspector using real catalog/state. Keep unavailable keys inspectable for explanation and spatial keyboard navigation usable.
- [ ] Verify production mapping against engine keys, state mutation via temporary profiles, local monitor cleanup, and actual light/dark/accessibility renders.

## B2 — Advanced binds and portable export

**Own:** new `launcher/ConfigChains.swift`, `launcher/ConfigAdvancedBindsView.swift`, tests; depends B1 and foundation grammar.

**Produces:** multi-command, toggle and per-press cycle models; compilation to `cs_<profile>.cfg` with persistent triggering bind, explicit Windows export and accurate press/release preview.

- [ ] Add independent expected press/release traces for leading-plus binds, semicolon tails and toggle/vstr chains. Prove non-yielding recursion detected and imported archived chains serialize unchanged.
- [ ] Prefer engine `toggle <dvar> v1 v2 ...` for single-setting cycles. Use launcher-owned `set` helpers for command cycles; report runtime variable usage accurately (unarchived still occupies pool entries).
- [ ] Enforce actual line/token/bind/command-buffer restrictions before writing; preserve opaque imported scripts rather than misidentifying them as an editable structured chain.
- [ ] Implement quote-safe portable `seta` export only when semantics can be preserved; show actionable errors for unrepresentable quotes rather than silently losing behavior. Ensure independent on-disk export does not mutate the active loadout.
- [ ] Verify actual emitted script execution through engine fixture, profile startup ordering and stored chain/bind transaction history.

## I1 — Text imports, dependency resolution and review classifier

**Own:** new `launcher/ConfigImport.swift`, `launcher/ConfigImportSafety.swift`, `launcher/WindowsProfiles.swift`, focused tests.

**Produces:** source/provenance descriptor; parsed candidates with safe/needs-OK/blocked classifications and transitive dependencies; mounted-volume Windows profile discovery.

- [ ] Add tests for semicolons in apparent comments, escaped-quote parity, unknown forwarded commands, secrets/filesystem paths, `unbindall`, nested bind/vstr/exec effects, same-root exec resolution, cycles and size limits.
- [ ] Use ConfigDocument commands and bytes rather than a second approximate parser. Block credential/CD-key/path assignment, expose all reachable risky commands, preserve unknown lines as review material without executing them.
- [ ] Support user-selected file/folder, paste/drop, HTTPS/GitHub/gist and localhost HTTP text sources; isolate network fetching from classification and never execute downloaded text during preview.
- [ ] Discover Windows installs/backups using bounded known paths on mounted volumes and user-selected folders. Offer active profile first and backups explicitly; do not traverse unrelated personal directories.
- [ ] Verify tests with authored synthetic fixtures and private corpus locally, with no raw profile text printed or copied into git.

## I2 — Review UI, directory and signed index

**Own:** new `launcher/ConfigImportView.swift`, `launcher/ConfigDirectory.swift`, `launcher/ConfigDirectoryView.swift`, directory schema/generator/checker under `tools/config-directory/`, tests; depends I1/F2/F4.

**Produces:** functioning import session/review/apply, Players/Saved/player detail, signature-verified cached directory loading, permission-gated metadata tooling.

- [ ] Test default new-loadout target, Review default with risky entries unchecked, Import everything still blocking forbidden content, lock preservation, failed apply retaining prior bytes and one transaction per confirmed import.
- [ ] Implement actual change review grouped in plain language, exact details on demand, platform translation and explicit confirmation. Wire real import sources and errors.
- [ ] Implement versioned index schema, Ed25519 verification, tampered/wrong-key rejection and offline cache; ensure fetching profile bytes validates the expected identity/digest and still invokes normal safety review.
- [ ] Directory entries require redistribution permission and source credit. Ship honest empty/unavailable state until permissioned entries and a real signing/publication setup exist. Do not fabricate players, permissions, URLs or signatures. A local fixture key is test-only, never a production trust root.
- [ ] Prepare standalone repository contents for the requested cod2-configs project without publishing until delivery authorization. Review source artifacts and verify local generator/CI checks.

## A1 — Rulesets and evidence-based Check-up

**Own:** new `launcher/ConfigRules.swift`, `launcher/ConfigCheckup.swift`, `launcher/ConfigCheckupView.swift`, ruleset/optimizer JSON resources, tests.

**Produces:** policy evaluation for stock/public CoD2x/verified zPAM versions; suggestion objects with exact proposed changes, explanation, observable effect, evidence and source.

- [ ] Test zPAM competitive caps125–250, rate25000, snaps40, packets125, wait-disabled and server-owned settings using verified source, not illustrative canvas defaults. Unknown/PAM/league variants require a real source before adding fixed policy.
- [ ] Implement ruleset-aware validation and typed suggestions for movement/frame cap, aim/input, connection and visibility. Never modify locks or server-owned settings or describe uncertain advice as measured.
- [ ] Implement tune-for modes and real Check-up cards/dials, explicit Apply, Show me, Not now and Undo via the shared store. Absent data displays Not measured.
- [ ] Unit-test every shipped suggestion rule and verify its on-disk apply/undo using temporary profiles.

## A2 — Local match summaries

**Own:** native-only local statistics module plus narrow lifecycle hooks, launcher summary reader/privacy setting and focused engine tests. Coordinate common/client files with fidelity owner and keep shader/render work separate.

**Produces:** `main/stats/matches.jsonl`, max200 records; map/mode/duration, server/rule hints, frame-timep50/p99/max, cap compliance, ping/loss/snapshot counts and cap, exactly as spec§13.

- [ ] Characterize lifecycle and timing sources. Write tests for bounded histogram/quantile accounting, disconnect/map-end once-only summary, retention200 and privacy disabled before adding hooks.
- [ ] Implement no per-frame allocation, no gameplay effect, no network transmission, finite-value JSON output and bounded local storage. Gate every engine addition behind COD2_X64 and prove original source unchanged.
- [ ] Connect launcher privacy toggle to launch behavior and summary read/Check-up evidence. Report exactly which metrics were available; don't invent hits or packet-loss values from unrelated counters.
- [ ] Validate deterministic fixture metrics, real local session output/pruning/opt-out, legacy gates and no unexpected release diagnostic files.

## A3 — Server ready check and final UI integration

**Own:** `ServersView.swift`, narrow `Core.swift`/`Model.swift` launch additions, ready-check view and tests; depends A1 and profile launch contract.

- [ ] Test verified vs unknown advertised server rules, password/direct/deep-link joins, choosing another loadout and per-connection overrides that never persist into profile bytes.
- [ ] Place real readiness result above Join with Details, alternative loadout and Fix for this server only. Bind overrides to that server; clear them if selection changes.
- [ ] Run full launcher tests and screenshots across all Configs/Binds/Players/Import/Check-up/History/Servers states, both appearances and accessibility modes. Exercise keyboard and local monitor operations, inspect actual resulting files, and ask Astra for independent visual review.
- [ ] Re-run full composed native merge gates and fresh engineering review before delivery. Document remaining external prerequisites honestly; do not replace unfinished features with disconnected buttons.
