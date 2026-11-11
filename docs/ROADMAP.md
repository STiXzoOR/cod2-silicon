# CoD2 Silicon roadmap

CoD2 Silicon is **Mac-first**. The goal is the best way to play Call of Duty 2
multiplayer anywhere, built natively for Apple silicon and current macOS
standards first, and for Windows and Linux after. These are planned directions, not release promises. The current
evidence is in the [port reports](macos-port/README.md).

## 0.2 — Launcher and servers (released 0.2.0)

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
- **Gameplay correctness.** A source-wide audit (WS38) and hands-on testing
  fixed dozens of 64-bit layout and reconstruction errors: menus, controls,
  sound settings, turrets, grenades and effects.

## 0.3 — The original's picture, and CoD2x in full (released 0.3.0)

- **The picture follows the original.** Replayed demos are compared frame by
  frame with the Windows 1.3 game, and the original shaders are built from
  the player's own game files.
- **Two games in one app.** CoD2x 1.4.6.8 in full, for players and for hosted
  servers, or the original 1.3, chosen in Settings.
- **Voice capture, in-app updates and opt-in crash reports.**

The [0.3.0 verification](macos-port/release-0.3.0-verification.md) says what
was tested and what was not.

## Next

These are not in a fixed order.

- **A whole match, played.** Every test so far was short and mostly on empty
  servers. Long sessions on busy servers, a real microphone, and CoD2x's
  services against the CoD2x project's own servers come first.
- **Windows and Linux.** The reconstructed engine has been built and run for
  both in separate work that is not in this repository yet. The app, its
  launcher and its packaging are Mac-only today.
- **WebAssembly.** The upstream browser build is not maintained here yet.
- **Launcher and Config Studio improvements**, including the player directory.

## Metal renderer

Metal was planned for 0.3 and moved behind the work above; smooth frame
pacing through Metal presentation moved with it. The
[readiness plan](plans/2026-10-13-03-metal-readiness-plan.md) defines
the integration order and certification gates. It builds on the preserved
Metal implementation and the [IW4L/Highball source study](research/2026-10-13-iw4l-highball.md).
This is preparation; Metal is not yet available in the released application.

- **Native Metal renderer.** Metal 4 on macOS 26 and later, implementing the
  existing D3D9-style renderer contracts so engine code is unchanged. OpenGL
  remains the classic fallback and visual parity reference on supported Macs
  (macOS 26 and later).
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
- **Voice capture:** test with a real microphone and follow input device
  changes while talking. Capture, permission and the encoded stream are in.
- **Third-person models:** they sit about half a pixel off the original in
  replayed demos.

## Later

- An iPad and Apple TV client, once the Metal renderer has replaced OpenGL
  (neither platform has OpenGL).
- Server-manager hosting presets for Mac mini and Mac cloud machines.
