# WS25 launcher design and implementation plan

The brief is the authority: build a current native Mac front end around the
existing game, retain CoD2x identity and the untouched i386 path, and finish
autonomously in `port/launcher`. The instruction to make decisions without
questions overrides interactive design/plan approval in the skills.

## Architecture decision

Use `Contents/MacOS/CoD2Launcher` (SwiftUI) and a separately running
`Contents/Helpers/CoD2 Game.app`. In-process hosting cannot isolate engine
crashes. A command-line helper without an app identity cannot reliably own
fullscreen/Game Mode eligibility. The nested app therefore has a regular
activation policy, the same original icon, its own `.game` bundle identifier,
and `LSSupportsGameMode`. Only the outer bundle registers `cod2x`.

The launcher owns setup and validated arguments, starts the nested executable,
becomes an accessory app while the engine owns the Dock, and restores its
window/Dock presence after termination. A URL received while playing is sent
directly to the engine's existing validated Apple-event handler. Cold URLs,
server joins and `--play` bypass Home after setup. CLI arguments following
`--` are forwarded to the engine for developer/test use. Neither process
changes protocol, identity or updater behavior. Game Mode eligibility is
preserved; actual system activation needs visual acceptance on a fullscreen
display and must not be inferred from the plist.

## Components and behavior

- `launcher/NativeSetup`: narrow Objective-C bridge over WS21's discovery,
  migration, validation and native extractor. No engine-source edits. Swift
  writes validated keys with exclusive mode-600 temporary files and rename.
- `launcher/Core`: bounded wire parsers, colour runs, settings/config writer,
  safe URL adapter using the existing C parser, persisted favorites/history.
- `launcher/Network`: Network.framework UDP, both masters/protocols, bounded
  responses, query throttling, timeout/cancellation and disk cache.
- `launcher/Model`: MainActor state, onboarding progress, isolated engine
  process, crash-report discovery, media listing and notification-only updates.
- `launcher/Views`: sidebar, Home, Servers/player inspector, Settings,
  Demos/screenshots, About, and three onboarding steps. System fonts/accent,
  SF Symbols, accessible controls and keyboard shortcuts. Liquid Glass is
  availability guarded on 26; materials work on 13–25. Original procedural
  terrain artwork and an original compass/terrain icon contain no game art.
- `launcher/Snapshots`: deterministic 1440×900 light/dark PNGs, fake servers,
  empty secure key fields, no network/setup side effects.

Settings write a separate `launcher.cfg` and pass its dvars at startup, never
rewrite the game's config. Resolution spans 1080p–6K; presets retain WS21's
333 fps/raw mouse/vsync defaults. Native Spaces uses SDL's existing environment
hint. Render scale, Metal/MetalFX/HDR are explicitly upcoming, disabled controls
until the renderer workstreams expose their interfaces.

## Execution plan (inline)

1. Write parser/config/key tests, capture public UDP fixtures, verify failure,
   implement Core/bridge and run the tests. Commit the independently tested core.
2. Implement UDP querying and persistence, SwiftUI screens, setup/process/update
   state and deterministic rendering. Build in Swift 6 strict concurrency for
   arm64 macOS 13; render and inspect every screen. Commit the launcher.
3. Integrate swiftc with native-only CMake and release scripts; package nested
   game/SDL, validate every Mach-O minos/dependency/signature. Add bundle tests
   and extend the scratch-home smoke through launcher→menu→Toujane→launcher.
   Commit packaging/docs.
4. Run both client builds and CONTRIBUTING suites, both full ABI checks,
   packaging/first-run smoke, legacy guards, and a separate final review.
   Record every failed/unavailable gate in the report, commit it.

Review focuses: malicious UDP records; URL/config injection and paths with
spaces; no keys/passwords in logs/cache; interrupted setup and child crashes;
rapid Play/URL delivery without duplicate engines. Network unavailability is
a visible recoverable error. No packages, pushes, PRs, issues, remote changes,
renderer/server edits or sibling-worktree access are authorized.
