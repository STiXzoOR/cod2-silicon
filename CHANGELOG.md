# Changelog

All notable changes to CoD2 Silicon are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Native SwiftUI launcher with Liquid Glass styling on macOS 26+ (material
  fallback on macOS 13–25): server browser, library, settings, first-run
  setup and `cod2x://` links. The game helper gets keyboard and mouse focus
  on launch.
- Layered Icon Composer app icon with light, dark, clear and tinted variants.
- Native arm64 dedicated server (`cod2_macos_ded`) for hosting on Mac
  hardware, with a launchd service, example config and operator guide in
  [docs/server.md](docs/server.md).

### Changed

- Fullscreen now defaults to desktop (borderless) fullscreen at the chosen
  render resolution. Exclusive fullscreen remains selectable.
- `com_maxfps` defaults to 250 and accepts 0 (no cap) through 1000.

### Fixed

- Opening Start New Server crashed the game. The UI read the game-type,
  map, mod and movie lists through 32-bit offsets on the 64-bit build.
- Smoke, explosions, muzzle flashes and other effects never advanced: the
  effects clock misread `fx_freeze` on the 64-bit build.
- Join Server: localized placeholders such as "Source: &&1Internet", the
  empty Game Type, the empty server list and the cyan column tint.
- Menu items flickered on hover; the pulse now follows the original timing.
- Portal visibility overran its pool and clipped projections incorrectly,
  a likely cause of the world briefly disappearing while looking around
  (that symptom has not been reproduced in automated sweeps).
- Black screens after Cmd-Tab, Mission Control or a hot corner: desktop
  fullscreen avoids display mode switches, and exclusive fullscreen now
  steps aside while the game is hidden. Exclusive fullscreen can still fail
  to restore after display sleep.
- A second (system) cursor in fullscreen menus.
- Players could not use stock mounted turrets. The use check read the
  wrong field, and the server marked every living player active each frame,
  which skipped the turret query.
- Temporary collision models dropped their content mask and broadphase
  links used the wrong entity numbering, so they were unlinked from the
  world.
- The Controls pages showed no key names next to their actions.
- After a game started from a server link or `--play` with engine options,
  the launcher came back without a window.
- A source-wide audit fixed more fields that the 64-bit build read at
  their 32-bit positions:
  - key binding records (the Controls menu);
  - 13 sound-setting reads (enable, pause and volume);
  - swapped network-channel sequence and dropped-packet fields;
  - player, corpse, obituary, scoreboard, pickup-sound and turret-event
    records;
  - renderer world, sun, sky and occluder objects;
  - collision-map allocation sizes.

  A gate now rejects new raw offsets, and a test opens every menu and runs
  every safe menu script.
- Five network-parser memory-safety bugs found by fuzzing, including an
  out-of-bounds decompression read and an `Info_SetValueForKey` overflow
  reachable through `getinfo`.

## [0.1.0] - 2026-10-04

### Added

- Native Apple silicon multiplayer client, SDK type guards and typed LP64
  data generation, retaining the original i386 build path.
- Native SDL/OpenGL display, raw mouse input, AudioUnit output, UDP networking,
  writable user paths, crash reports and a freeze watchdog.
- CoD2x 1.4 client compatibility: protocol negotiation, native identity,
  competitive policy, demo recording/upload and `cod2x://` links.
- Original Mac shader extraction from player-owned data, validated local
  cache reuse and approximate rendering fallback.
- Fullscreen render sizes through 6K with exact backing dimensions and
  desktop-fullscreen fallback.
- Self-contained `CoD2 Silicon.app` for Apple silicon on macOS 13 or later,
  with pinned SDL3/sdl2-compat built from checksum-verified source and
  bundled; ad-hoc signed release zip and a from-source `scripts/install.sh`.
- First-run setup: game-folder discovery or picker, validated CD-key prompt
  with owner-only storage, native (Python-free) shader extraction and one-time
  migration from earlier `CoD2x Native` installs.
- Committed typed-data snapshot (`build/lp64_gen/`) so clean checkouts and CI
  build without any original game binary; local regeneration verifies it.
- Opt-in client-frame observer, `r_presentMode 1` fence presentation
  experiment, Spaces fullscreen selection and the `validate-333.sh` /
  `check-gamemode.sh` acceptance tools.
- ABI, ILP32 preservation, script, game, renderer, platform, network,
  packaging and performance fixtures; public documentation and contribution
  policies.

### Fixed

- Pointer-width layouts, calling contracts, data/import indirection and
  misaligned script values across the native engine.
- Stock and CoD2x internet authentication, reliable commands, pure checksums,
  server-browser parsing and HTTP mod downloads.
- Script shutdown/timeout errors, lighting and sky reconstruction, HUD
  matrices, texture completeness, FX dispatch and impact-mark storage.
- First-shot stencil-clear argument order, verified by a ten-minute combat
  soak with firing, reloading and grenades.

### Known limitations

- Smooth frame pacing at high caps and active macOS Game Mode remain unverified. WS18's capped
  1080p capture averaged 331.2 fps with a 205.2 fps 1% low; most slow frames
  wait inside the OpenGL swap (WS19).
- No microphone capture, intro cinematics or standalone native server link.
- Rendering comparisons cover three matched views, not complete graphics
  parity. Memory-safety and mod edge cases remain.

Evidence and historical qualifications are preserved in the
[workstream reports](docs/macos-port/README.md).

[Unreleased]: https://github.com/STiXzoOR/cod2-silicon/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.1.0
