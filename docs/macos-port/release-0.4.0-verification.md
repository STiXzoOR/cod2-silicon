# CoD2 Unbound 0.4.0 verification

Recorded on 20 November 2026, on a Mac mini (Apple M6, macOS 27, the macOS 27
SDK) with a 5K display. GitHub Actions were still switched off, so the checks
below were run on that Mac.

0.4.0 moves the game from OpenGL to Metal. The engine issues the Direct3D 9
calls the Windows game issues, and a library inside the app, mtld3d, turns
them into Metal. The app carries each game twice: a Metal engine and the
OpenGL engine 0.3.7 shipped, which Settings › Display can choose instead.
The dedicated servers draw nothing and are the code 0.3.7 shipped, apart
from the open-file limit below.

**Nobody has played a whole match on Metal.** Everything below is a replay, a
listen server with one player, a measurement run or an automated start. That
is the largest thing this record does not cover.

## What was run

- **The picture, against the original and against OpenGL.** The same
  recorded demos are replayed and a screenshot is taken at the same frames.
  Over the 20 views used since 0.3.0, Metal differs from the original Windows
  game by 0.111 of 255 on average and OpenGL by 0.113; Metal against OpenGL
  is 0.009. With 4x anti-aliasing Metal against OpenGL is 0.048. With the
  DirectX 7 renderer at its highest settings (4x anti-aliasing, 16x
  anisotropic filtering) it is 0.053 to 0.063 on four maps. The HUD's pixels
  are identical.
- **Render scale**, in a 1920 × 1080 window. At Full, screenshots are
  identical, pixel for pixel, to the build before render scale existed, with
  anti-aliasing off and at 4x, on both renderers. At 50%, 67% and 75% the
  scene is softer and lines up with the full-size picture, and the HUD, text
  and menus are drawn at the window's full size. The intro film and the
  first menu fill the window at 50%. Changing the scale and restarting the
  renderer gives the same picture as starting with it. One screenshot was
  looked at on the 5K display at 67%.
- **Smoke.** Thick smoke is drawn in a layer of its own at full, half or
  quarter size. With a frozen cloud, the half-size layer differs from the
  full-size one by 0.28 of 255 on average and the quarter-size layer by
  0.58, on a 1920 × 1080 picture. What the smoke hides is the same: the layer
  only changes how the smoke itself is drawn.
- **Frame timing in exclusive full screen**, four smoke grenades at the
  player's feet on Carentan, 4x anti-aliasing, every effect on, one player on
  a listen server. With the cap at 250: 3840 × 2160 at Full held 249.6 frames
  a second with no late frame (99 in 100 frames within 4.52 ms); 5120 × 2880
  at 67% held 250.0 (4.54 ms); 5120 × 2880 at 75% did not (210). At 1920 ×
  1080 and 2560 × 1440 at Full, a cap of 333 was held. A cap of 333 was not
  held above 2560 × 1440 at any scale tried. These are one machine's numbers.
- **The first visit to a map.** With nothing saved, Toujane at 2560 × 1440
  compiled 80 shaders in 202 ms; frames longer than usual came only in the
  first two seconds. On the next start the saved result was read in 0.06
  seconds.
- **CoD2x's film colour controls** work on Metal, including the sky's own
  saturation with anti-aliasing on and soft particles and shadows off, which
  was wrong until `206c1ef5`.
- **The engine checks.** `python3 tests/fixes13/legacy.py --base main`
  reports 0 mismatches. The byte-offset check reports nothing unreviewed on
  the Metal build, the OpenGL build and the OpenGL reference build.
- **The launcher's tests and the capture suite.** At `4b0c94d9`, the commit
  the package was built from: the unit tests, the config, hosting and Steam
  download tests, the font check and the typecheck of the fallback rendering
  path all passed. The capture suite was run in full and passed all 17 of
  its phases the first time: 410 screens in both appearances, at full and at
  minimum size and with the accessibility settings, their accessibility
  trees, the real clicks, and 2,348 shapes with no finding. The new
  Display screens (Metal with a render scale set, and OpenGL with Render
  scale dimmed) were looked at by eye in both appearances, and About in
  Dark.
  The package was built one commit later, at `c447364f`, which changes one
  engine file (the profile list, below) and nothing in the launcher; the
  suite was not run again on it.
- **The package.** `scripts/package-release.sh` built both games twice (for
  Metal and for OpenGL), both dedicated servers and the launcher from
  `c447364f`, signed the app with a Developer ID and had Apple notarize it,
  then built the disk image, signed it and had Apple notarize that too; both
  tickets are stapled. The app taken out of the ZIP and the app inside the
  disk image each pass the checksum, the signature check, Gatekeeper's
  assessment and the bundle audit (twelve programs and libraries, the Metal
  library among them, all signed by the same Developer ID), and each carries
  the crash-report address.
  `tests/packaging/release_smoke.py` started all six engines from the app in
  the disk image: both games, with Metal and with OpenGL, reached the menu,
  loaded Toujane, played it and quit cleanly, and both servers answered a
  status query and stopped.
- **A start through the launcher.** The launcher from the disk image, in an
  empty home of its own with Render scale saved as 67%, was started with
  `--play`. It set itself up, started the Metal CoD2x engine and passed the
  scale; the game reported a 1286 × 724 scene in a 1920 × 1080 window and
  reached its menu, and the launcher came back when the game ended. Started
  again with the scale at Full, it passed no scale.
- **The game's profile list.** That start showed something older than
  Metal: Select Profile listed `.studio-migration`, a folder the launcher
  keeps beside the loadouts, and because the list then had more than one
  entry the game asked for a profile at every start. Hidden folders are
  left out of the list from `c447364f`. With one loadout and no profiles in
  the game folder, the game now opens on its main menu. Profiles that are
  in the game folder itself (a Windows install has some) are still listed,
  and with more than one entry the game still asks, as the original does.
- **The match record.** After a listen-server game the record carries the
  new fields (whether the window was on screen and had the keyboard; memory
  at the start, the end and the highest). The launcher does not show them
  yet; it skips fields it does not know.

## Known differences from OpenGL

- **Rooflines behind quarter-size smoke.** When the smoke layer drops to
  quarter size, the edge of a building seen through the smoke can lose its
  smoothing. Measured on a frozen cloud at 1920 × 1080: about 1,200 of 2
  million pixels differ by more than 12 of 255.
- **The weapon in your hands, inside smoke.** The original draws a little of
  the nearest smoke over the weapon. The half-size and quarter-size layers
  leave the weapon a shade clearer (about 4 of 255 over 8% of the picture, in
  dense smoke only).
- **Render scale is a plain stretch.** Below Full, the scene is softer; there
  is no sharpening and no MetalFX upscaling.

## What was not

- A whole match, on a busy server or any server, with Metal. Not by a
  person, and not by a script.
- The shellshock blur (a grenade close by) with a render scale set. Its
  code was changed and read through; it could not be triggered from a
  script.
- Spectating, the killcam and any view that does not fill the screen, with a
  render scale set.
- Render scale together with the DirectX 7 renderer in exclusive full
  screen. In a window both were compared and agree.
- Switching away from the game and back, Mission Control and a second
  display, on Metal, beyond what the measurement runs did by themselves.
- A long session. Nothing ran longer than a few minutes, so heat and memory
  over an hour are unknown; the match record now notes memory so the first
  long session can show it.
- The very first start on a Mac that has never run the app. The cold start
  above cleared this app's saved shaders; whether macOS kept its own copy
  from earlier runs could not be ruled out.
- Install Update from 0.3.7 to 0.4.0. It needs the published release.
- A Mac other than this one. No Intel Mac is supported; no other Apple
  silicon Mac was tried.
- The Windows and Linux build files that came in with this release build
  the engine's source; no Windows or Linux app was started.
- VoiceOver itself, as before.

Everything in [0.3.7's record](release-0.3.7-verification.md) about the
launcher, and in the records before it about the game, still applies.
