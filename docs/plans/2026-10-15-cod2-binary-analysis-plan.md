---
title: CoD2 owned-binary analysis with Rizin and REA
date: 2026-10-15
execution: knowledge-work
status: A0–A4 catalogs delivered with explicit gaps; A5–A6 verification/audit in progress
---

# CoD2 owned-binary analysis with Rizin and REA

## Objective and completion meaning

Build a comprehensive, searchable private reference for the owned Call of Duty 2
executables, using Rizin and REA to make reconstruction and Metal integration
easier. Preserve and improve the existing reconstructed source instead of
replacing it with bulk pseudocode. This is the user's requested analysis plan,
added alongside U1; it is not a claim that CoD2 has been fully decompiled.

“Full” means every executable region and discovered function in a declared
binary/version/platform manifest has a recorded disposition. Coverage is
measured separately for discovery, disassembly, pseudocode, type recovery,
source mapping and behavioral validation. Compiler optimization, inlining,
stripped names and indirect dispatch can prevent exact source recovery. Never
claim original source, comments, types or complete semantics merely because an
export command succeeded. Unresolved regions remain visible.

The repository is currently an MP reconstruction. Start with its owned Mac MP
references; inventory SP, Windows MP/SP, dedicated binaries and variant versions
as separate targets if present. Do not label an MP-only result “all CoD2.”
Unavailable variants remain explicit inventory gaps, not inferred duplicates.

## Verified starting point

Read-only checks on 2026-10-15 found and hashed:

| Target | SHA-256 | Size |
| --- | --- | ---: |
| Mac MP 1.3 i386 STABS reference | `15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12` | 20,211,172 |
| Owned Steam Mac MP value reference | `a6aa70d4e2b0bf5f388b0653752847bddac95398ee0d860e757ec79e466f5d1b` | 4,958,488 |

Their machine-local paths are the defaults documented in
[tools/datagen/README.md](../../tools/datagen/README.md):
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386` and
`~/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer`.
Do not derive the Steam file's version from its filename. Record headers,
architecture slices and build/version evidence during inventory.

`rizin`, `rz-bin`, `rea`, `ghidraRun` and `analyzeHeadless` were absent from PATH.
No matching Hopper/Ghidra/Cutter/IDA application name was found directly under
`/Applications`. These are bounded availability checks, not an exhaustive scan.
Both checked references identify as Mach-O i386 executables. Node v24.21.0
and npm 11.19.0 are available. No tools were installed or agent configuration
changed in U1.

Existing useful evidence includes STABS-backed data generation, ABI audits,
`tools/fidelity/`, WS75 original-address contracts and WS94 lighting field
contracts. Import their identities and findings into the analysis ledger;
do not redo validated work merely to obtain a different tool's output.

## Tool roles and pinned research

The user selected [Rizin](https://rizin.re/install/) and
[REA](https://github.com/morluto/rea). Documentation was inspected on 2026-10-15.
Resolved upstream source pins: REA main `e9a71efa31d6c3675b67938b8a79fa4ddf01604c`,
Rizin dev `73c42c65a2692714dbe05d305f832647b2e0c66b`. These are research identities,
not installed or validated release versions. Select immutable released packages
and record their hashes before execution; do not silently execute moving latest.

- Rizin: independent binary metadata, sections, strings, symbols, disassembly,
  cross-references and function inventory. Its official macOS installation page
  offers Homebrew and package/DMG downloads, plus source builds. Choose an
  isolated tools prefix for this project rather than altering system packages.
  [Installation source](https://rizin.re/install/).
- REA: evidence-backed orchestration and searchable analysis through its CLI/MCP.
  Deep native pseudocode needs a supported provider such as Ghidra, Hopper or IDA;
  Rizin alone is not a promise of decompilation. Proposed first provider: Ghidra,
  subject to a successful i386 Mach-O/STABS pilot and available JDK. Retain the
  original binary/assembly as authority when tools disagree.
  [REA overview](https://github.com/morluto/rea).
- REA can invoke a caller-supplied Rizin via `REA_RIZIN_COMMAND` and
  `execute-rizin-command`. Its documented one-shot profile suppresses user
  settings/scripts, but plugins remain available. Rizin debugger sessions in REA
  require Linux; use offline analysis on this Mac. Bound exports because provider
  captures can truncate at 16 MiB and MCP has a separate response budget.
  [Provider contract](https://github.com/morluto/rea/blob/e9a71efa31d6c3675b67938b8a79fa4ddf01604c/docs/reverse-engineering-tools.md).
- Prefer a private CLI workspace first. MCP registration is optional and a
  separate configuration change; REA setup can preview exact changes with
  `--dry-run`. Validate Node/provider prerequisites against the pinned release.
  [Setup contract](https://github.com/morluto/rea/blob/e9a71efa31d6c3675b67938b8a79fa4ddf01604c/docs/installation.md).

## Execution units

| Unit | Work and concrete deliverable | Exit criterion |
| --- | --- | --- |
| A0 — binary manifest | Inventory only owned executable candidates under the known reference/game roots. Record hash, size, provenance, platform/version evidence, architecture slices, executable regions, symbols/debug format. List missing SP/Windows/server variants. Store read-only working copies privately and verify before/after hashes. | Every chosen input has immutable identity and a declared role; missing targets are named. |
| A1 — provider pilot | Provision pinned local tools only when installation is authorized under CONTRIBUTING.md. Record tool/JDK/Node versions and hashes. Use Rizin for metadata/disassembly and REA with Ghidra for pseudocode on a bounded set of already-understood functions. Check format, endianness, image base and calling conventions explicitly. | Cross-check WS75 water and WS94 lighting addresses against assembly and known fixtures; reject a provider that yields wrong boundaries/types. Save reproducible commands and diagnostics. |
| A2 — whole-program discovery | Analyze each executable slice; inventory symbols, functions, call graph, xrefs, imports, strings and executable ranges. Merge symbol-backed and heuristic discoveries with provenance. Deduplicate aliases/thunks without hiding them. Flag overlaps, jump tables, code/data ambiguity and unresolved indirect targets. | Every executable range has a classification; unknown regions are enumerated. Report both function and executable-byte denominators. |
| A3 — resumable decompilation | Export per-function pseudocode plus assembly/CFG/xrefs in bounded batches. Key outputs by binary hash, architecture and relative address. Checkpoint after each batch. Preserve failed/timed-out/truncated outcomes and retry under explicit budgets; never convert them to successes. | Each discovered function has a durable record and outcome; output completeness is checked against inventory, not log line counts. |
| A4 — types and source correspondence | Use i386 STABS and existing engine headers to recover types/structures, vtables and call signatures. Keep 32-bit reference layouts separate from native LP64. Map binary functions to existing repo functions by symbols and corroborated behavior. Annotate dataflow and field contracts; resolve ambiguous overloads manually. | Each mapping states evidence/confidence, source commit and unresolved discrepancies; no raw dump is imported into runtime code. |
| A5 — integration-first verification | Prioritize D3D9 objects/resource lifetime, renderer state, water/lighting, screenshots, FX timing, map loading, then remaining engine subsystems. Convert uncertain behavior into minimal synthetic fixtures and private differential probes. Maintain engine, network, GSC and demo invariants. | A behavior is marked validated only when independent fixtures/reference comparison support it. A decompiler's agreement with itself is insufficient. |
| A6 — comprehensive audit | Reconcile all remaining functions/regions, runtime-discovered targets and variant differences. Publish a sanitized coverage summary with denominators, unresolved counts and exact scope. Keep searchable private databases and original evidence. | No silent omissions; every listed target and region has a disposition. Outstanding unknowns prevent a “complete validated reconstruction” claim even if pseudocode export is complete. |

A0→A1 precedes bulk A2/A3. A4/A5 can progress in bounded batches while A3
continues. A6 audits all batches. Do not make exhaustive whole-game analysis a
prerequisite for every Metal slice: attach relevant A5 findings to U2–U6, and
continue recovering other subsystems separately. No runtime attachment or game
launch is needed for A0–A4; any later dynamic probes use the project's exclusive
bench wrapper, owned processes and private writable homes.

## Private workspace and coverage schema

Proposed external root:
`~/Library/Application Support/CoD2-native-binary-analysis/20261015.noindex/`.
Keep inputs, tools, provider projects, batch exports, receipts and coverage there.
This location was not provisioned by U1; the subsequent authorized execution
created it with owner-only root permissions.

Each function record should contain binary SHA-256, slice, address/size,
symbol/aliases, discovery source, code hash, tool/version/options, analysis
status, pseudocode status, output hashes, truncation flag, callers/callees,
unresolved indirect references, type provenance, source mapping, confidence,
verification evidence and last completed batch. A manifest binds all outputs.
Maintain independent counts for discovered, analyzed, decompiled, mapped,
reviewed and behaviorally validated functions. Report excluded libraries,
thunks, data and unresolved executable bytes explicitly.

Private outputs may contain proprietary code and stay outside the repository,
release bundles and public CI artifacts, as required by CONTRIBUTING.md. Commit
only concise behavioral findings, independently authored fixes and synthetic
fixtures with reviewed provenance. Rizin/provider source or generated code must
not be copied into the MIT port without satisfying the project's licensing
rules. This plan does not claim a formal clean-room separation.

## Readiness and next action

U1 verified the two reference identities and produced this plan. A0's complete
variant inventory and A1's tool/provider provisioning are still pending. Full
binary decompilation has not run. Tool installation is the first execution
prerequisite: CONTRIBUTING.md says “Do not install system packages or change
global configuration during autonomous port work” and to record missing tools.
A private, pinned installation proposal can be prepared without changing global
settings. Confirm tool behavior on the two known contracts before scheduling
large exports or estimating completion time.

## Authorized execution update — 2026-10-15

The user subsequently authorized local Rizin, REA and Ghidra installation,
starting analysis of owned binaries, keeping all decompiled output private, and
committing/pushing per phase. The requested model preference is Sol 6.1 at
xhigh; verify that setting in the execution host. This authorization supersedes
the earlier installation-pending state above.

- [A0 inventory](../macos-port/reports/2026-10-15-binary-analysis-A0.md): complete
  for three selected owned Mac i386 targets, including Steam single-player.
- [A1 toolchain and pilot](../macos-port/reports/2026-10-15-binary-analysis-A1.md):
  complete: pinned local tools, passing Ghidra readiness, and a verified
  lighting/water pilot within a six-function private export batch.
- [A2 discovery](../macos-port/reports/2026-10-15-binary-analysis-A2.md): complete
  for the selected corpus, with reconciled function and byte denominators,
  recovered candidate boundaries and explicit unknown regions.
- [A3 private export](../macos-port/reports/2026-10-15-binary-analysis-A3.md): complete
  against all 24,735 final inventory records: 22,777 decompiled bodies, two
  no-decoded-body records and 1,956 external imports. All Rizin candidates have
  explicit dispositions; unknown bytes and unproven behavior remain visible.
- [A4 types and source correspondence](../macos-port/reports/2026-10-15-binary-analysis-A4.md):
  catalog delivered for every record, with 13,523 unique source candidates and
  explicit type/parser and mapping gaps. Symbol correspondence is not behavior proof.
- A5 verification and A6 comprehensive disposition audit are in progress.

The earlier “verified starting point” and “readiness and next action” sections
are the U1 snapshot. The linked execution reports carry newer evidence.
