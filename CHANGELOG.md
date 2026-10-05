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
