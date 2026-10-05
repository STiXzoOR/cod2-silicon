# macOS port development log

CoD2 Silicon grew from an AI-orchestrated opencod2 port organized into 22
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
- [WS19: hitch](reports/WS19-hitch.md) — client-frame observer, presentation experiments, Spaces fullscreen and 333/Game Mode validators.
- [WS20: publish](reports/WS20-publish.md) — release docs, credits, contribution policies, CI and privacy hygiene.
- [WS21: package](reports/WS21-package.md) — self-contained macOS 13 app, bundled SDL, native shader setup, first run and release scripts.
- [WS22: datagen-snapshot](reports/WS22-datagen-snapshot.md) — committed typed-data snapshot so clean checkouts build without private inputs.
- [WS25: launcher](reports/WS25-launcher.md) — native SwiftUI launcher with game hand-off, first-run setup and `cod2x://` links ([design](WS25-design.md), [design v2](WS25-design-v2.md)).
- [WS26: dedicated](reports/WS26-dedicated.md) — native arm64 dedicated server for hosting on Mac hardware.
- [WS27: fuzz](reports/WS27-fuzz.md) — network-parser fuzzing with five robustness fixes.
- [WS28: launcher-v2](reports/WS28-launcher-v2.md) — Liquid Glass launcher from the approved prototype, the Icon Composer icon and a 250 fps default cap.
- [WS36: qa-fixes](reports/WS36-qa-fixes.md) — first hands-on fixes: server browser, menu text and colours, hover cadence, portal bounds and the desktop-fullscreen default.
- [WS38: lp64-audit](reports/WS38-lp64-audit.md) — source-wide audit of 32-bit offsets compiled into the 64-bit build, the raw-offset gate and the all-menus sweep ([core audit](reports/WS38-core.md)).

WS7, WS23 and WS24 have no separate report in this checkout, and the Metal renderer reports (WS29–WS35, WS37 and WS39) arrive with the renderer in 0.3; the numbering is retained.

Commit hashes quoted in these reports refer to the development history before
publication. The author email was normalized to a GitHub noreply address
before the first public push, which changed those hashes; commit subjects and
order are unchanged, so `git log --grep` finds them.

## Deeper references

- [Game data](game-data.md) — licensed content and local shader inputs.
- [CoD2x compatibility](cod2x-compat.md) — reference behaviors and feature gates.
- [Parity methodology](parity.md) — simulation, build and timing comparisons.
- [LP64 inventory](lp64-inventory.md) — initial architecture migration inventory.
- [Test server](test-server.md) — private stock/CoD2x control-server setup.
- [Contributing](../../CONTRIBUTING.md) — current rules and merge gate.
