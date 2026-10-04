# Changelog

All notable changes to CoD2 Silicon are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

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
- App bundle preparation, launch defaults and signed-resource verification.
- ABI, ILP32 preservation, script, game, renderer, platform, network and
  performance fixtures; public documentation and contribution policies.

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

- Constant 333 fps and active macOS Game Mode remain unverified. WS18's capped
  1080p capture averaged 331.2 fps with a 205.2 fps 1% low.
- No microphone capture, intro cinematics or standalone native server link.
- Rendering comparisons cover three matched views, not complete graphics
  parity. Memory-safety and mod edge cases remain.

Evidence and historical qualifications are preserved in the
[workstream reports](docs/macos-port/README.md).

[Unreleased]: https://github.com/STiXzoOR/cod2-silicon/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.1.0
