# CoD2 Silicon roadmap

These are planned directions, not release promises. The current evidence is
in the [port reports](macos-port/README.md).

## Near term

- **Constant 333 fps:** WS18 averages approach the cap, but 1% lows remain
  205–233 fps in capped captures. WS19 traced most slow frames to the OpenGL
  swap waiting on the compositor; a GPU fence did not prevent it, so that path
  stays opt-in (`r_presentMode 1`). Next: present through Metal (an
  IOSurface/`CAMetalLayer` bridge that keeps the existing renderer), then
  accept only unlocked, quiet-machine results from `tools/macos/validate-333.sh`.
- **Game Mode:** establish actual ON state in macOS built-in fullscreen,
  verify input focus and display restoration, then measure a matched ON/OFF
  comparison. The plist eligibility flag alone does not establish activation.

## Engine gaps

- **Standalone dedicated server:** finish linking `cod2_macos_ded` without
  renderer/data roots. The current workaround is `cod2_macos +set dedicated 1`.
- **185 placeholder globals:** classify and replace remaining zero-filled
  placeholders using verified type, size, initialization and ownership facts;
  retain a zero-mismatch ABI audit and unchanged i386 output.
- **Voice capture:** implement the native microphone path, permissions,
  device changes and compatible capture/encoding without blocking gameplay.
- **Intro cinematics:** restore playback from player-owned content with safe
  timing, skip and cleanup behavior.

## Later

- **Quality of life:** clearer setup and diagnostics, input/display options,
  server-browser usability and accessible settings while preserving protocol
  and competitive-server policy.
- **Modern graphics:** explore optional “2026 era” post-processing, with a
  faithful classic mode and measured latency/cost. A possible Metal renderer
  is a separate research track; first establish renderer contracts and
  reproducible comparisons, then prototype rather than promise a rewrite.
