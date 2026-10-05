# CoD2 Silicon roadmap

CoD2 Silicon is **Mac-first**. The goal is the best way to play Call of Duty 2
multiplayer anywhere, built natively for Apple silicon and current macOS
standards. These are planned directions, not release promises. The current
evidence is in the [port reports](macos-port/README.md).

## 0.2 — Launcher, servers and smooth frames

- **Native launcher.** A SwiftUI front end that replaces the first-run
  dialogs. It covers:
  - onboarding: game data, CD key and shader setup;
  - a server browser for stock 1.3 and CoD2x 1.4 servers, with direct
    connect, favorites and ping;
  - display, graphics, input, audio and fps-cap settings;
  - demos, screenshots and update notices.

  It follows the current macOS design language and uses only original
  artwork.
- **Dedicated servers on Mac hardware.** A native, headless arm64
  `cod2_macos_ded` that runs as a launchd service on any Apple silicon Mac,
  followed by a server manager. Network and file parsing will be fuzzed and
  hardened before internet hosting is recommended.
- **Smooth frame pacing.** Present frames through Metal (`CAMetalLayer` and
  `CAMetalDisplayLink`), so the game's frame loop never waits on the
  compositor. WS19 traced most current hitches to the OpenGL swap. Results
  count only when `tools/macos/validate-333.sh` runs on an unlocked, quiet
  machine.

## 0.3 — Metal renderer

- **Native Metal renderer.** Metal 4 on macOS 26 and later, implementing the
  existing D3D9-style renderer contracts so engine code is unchanged. OpenGL
  remains the classic fallback on macOS 13–25 and the visual parity reference.
  The original Mac shaders are translated to Metal locally from the player's
  own copy; they are never distributed.
- **Modern options, with a faithful classic mode:**
  - render scale with MetalFX upscaling;
  - supersampling;
  - anisotropic filtering;
  - EDR/HDR output;
  - ProMotion and variable refresh;
  - correct widescreen FOV;
  - HUD scaling for 4K–6K.

  Server policy is respected, and there is no frame interpolation in
  competitive play.
- **Game Mode.** Native fullscreen Spaces, verified on and off with matched
  measurements.

## Engine gaps

- **185 placeholder globals:** classify and replace the remaining zero-filled
  placeholders using verified type, size, initialization and ownership facts.
  Keep the ABI audit at zero mismatches and the i386 output unchanged.
- **Voice capture:** a native microphone path that handles permissions and
  device changes and encodes compatibly, without blocking gameplay.
- **Intro cinematics:** restore playback from player-owned content, with safe
  timing, skip and cleanup behavior.
- **CoD2x server features:** server-side CoD2x 1.4 behavior for the native
  dedicated server.

## Later

- An iPad and Apple TV client, once the Metal renderer has replaced OpenGL
  (neither platform has OpenGL).
- Server-manager hosting presets for Mac mini and Mac cloud machines.
