# macOS port development log

CoD2 Silicon grew from an AI-orchestrated opencod2 port organized into 21
workstreams. [PLAN.md](PLAN.md) is the shared development brief; `reports/WS*.md`
record decisions, commands, observations and limits. These are historical
records, not current player instructions. Later follow-ups and workstreams
can supersede earlier failures or claims. Use the top-level
[README](../../README.md) to install and play.

Reference paths under `~/Projects/cod2-native-refs/`, private game paths and
ignored evidence directories describe the original development environment.
Those inputs are not distributed. Keep these document paths stable so
existing cross-references continue to work.

## Reports

- [WS1: macos-build](reports/WS1-macos-build.md) — arm64 compilation, SDK guards and initial LP64/link inventory.
- [WS2: datagen](reports/WS2-datagen.md) — STABS generation, i386 round trips and recovered scalar values.
- [WS3: platform](reports/WS3-platform.md) — native display, input, audio, networking, paths and fixtures.
- [WS4: cod2x](reports/WS4-cod2x.md) — protocol, identity, competitive policy and animation behavior.
- [WS5: verify](reports/WS5-verify.md) — legacy CI comparison, parity harness and benchmark methodology.
- [WS6: game](reports/WS6-game.md) — pointer-width game/server layouts, snapshots and wire fixtures.
- [WS6: renderer](reports/WS6-renderer.md) — GL widths, disk material/font marshalling and command storage.
- [WS6: script](reports/WS6-script.md) — native script arenas, parser/compiler storage and VM semantics.
- [WS8: wine-baseline](reports/WS8-wine-baseline.md) — real Windows CoD2x under Wine and measured baseline limits.
- [WS9: fixes13](reports/WS9-fixes13.md) — reconstruction corrections, buffer sizes and timing fixtures.
- [WS10: cod2x-full](reports/WS10-cod2x-full.md) — 68-row feature inventory, native equivalents, demos and URL support.
- [WS11: bringup](reports/WS11-bringup.md) — first menu, Toujane movement, firing and runtime diagnosis.
- [WS12: abi-audit](reports/WS12-abi-audit.md) — translation-unit contracts, callbacks and import audits.
- [WS12: abi-callbacks](reports/WS12-abi-callbacks.md) — renderer-table and floating-argument callback repairs.
- [WS12: abi-game](reports/WS12-abi-game.md) — game/server/UI signatures, pointers and storage.
- [WS12: abi-imports](reports/WS12-abi-imports.md) — import load depth, storage provenance and placeholders.
- [WS12: abi-locations](reports/WS12-abi-locations.md) — source-location index for ABI repairs.
- [WS13: perf](reports/WS13-perf.md) — measured hot paths, renderer costs and source-based fixtures.
- [WS14: online](reports/WS14-online.md) — public sessions, authentication, restart and HTTP downloads.
- [WS15: render](reports/WS15-render.md) — original shaders, lighting/sky/HUD/FX and Windows comparisons.
- [WS16: vm-safety](reports/WS16-vm-safety.md) — shutdown, return/unwind, timeout and unaligned-value fixes.
- [WS17: fullscreen](reports/WS17-fullscreen.md) — exact render backing through 6K, pacing and Game Mode limits.
- [WS18: ship](reports/WS18-ship.md) — firing fix, shader setup, combat soak and six performance captures.
- [WS20: publish](reports/WS20-publish.md) — release docs, credits, contribution policies, CI and privacy hygiene.

WS7 has no separate report in this checkout; the numbering is retained.
WS19 (frame pacing) and WS21 (packaging) are concurrent workstreams whose
reports are not yet present here.

## Deeper references

- [Game data](game-data.md) — licensed content and local shader inputs.
- [CoD2x compatibility](cod2x-compat.md) — reference behaviors and feature gates.
- [Parity methodology](parity.md) — simulation, build and timing comparisons.
- [LP64 inventory](lp64-inventory.md) — initial architecture migration inventory.
- [Test server](test-server.md) — private stock/CoD2x control-server setup.
- [Contributing](../../CONTRIBUTING.md) — current rules and merge gate.
