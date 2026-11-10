# Changelog

All notable changes to CoD2 Silicon are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.3.0] - 2026-11-10

The picture now follows the original Windows 1.3 game, checked frame by frame
in replayed demos, and the original shaders are built from your own game
files. [What was tested and what was not](docs/macos-port/release-0.3.0-verification.md).

### Added

- Voice chat sends your microphone. Hold the voice chat key on a server with
  voice on and macOS asks once for microphone access; the microphone is open
  only while you are sending and for two seconds after, so the system's
  microphone indicator shows exactly when the game listens. The original
  games kept the microphone open the whole time. Tested with a recording in
  place of a microphone between two clients, not yet with a real one.

### Changed

- The game's videos play. The `cinematic` console command and the intro
  (publisher and studio logos, then the game's intro film) were switched off
  because they showed a black screen. Started without the launcher, the game
  plays the intro once on its first run, as on Windows; any key skips each
  part. The launcher still goes straight to the menu.
- A dedicated server build keeps its settings in `config_mp_server.cfg` and
  its log in `console_mp_server.log`, like the original Linux server, and no
  longer needs a player profile to load a map.
- Bushes, grass and trees have the right colour. The colours baked into
  static models had red and blue swapped on the default render path, so
  foliage came out slightly blue-green where the original is yellow-green.
- Tinted map surfaces have the right colour. The colours a map paints onto
  its walls and ground had red and blue swapped, so warm tints came out
  cool: the sandy foot of the walls on Toujane was blue-grey. Maps that only
  use grey shading there, such as Carentan and Burgundy, look the same.
- The HUD and menus line up with Windows 1.3. Everything 2D sat half a pixel
  right of and below where Windows draws it, and text was drawn twice a
  quarter pixel apart, which made it bolder and blurred its shadow. In a
  replayed demo the ammo counter, grenade icons, compass and match timer now
  match the Windows game pixel for pixel. The 3D view is unchanged.
- Text and script HUD elements (timers, scores, objective text) that fall
  exactly between two pixels go to the same pixel as on Windows 1.3. Mac 1.3
  put them one pixel lower or further right. At 1920x1080 this moved the
  match timer and some lines of menu and hint text up one pixel.
- The Direct3D 7 renderer (`r_rendererPreference dx7`) draws correctly. With
  the game's shaders installed the world came out pink and teal, because its
  lightmaps were built for the shader path; trees and bushes also had red and
  blue swapped. Replayed demos now match the Windows game on that path about
  as closely as the default renderer does.
- Automatic texture detail follows Windows 1.3's memory tiers. On the Direct3D
  7 path this means full-resolution textures, where Mac 1.3 halved them. The
  default renderer already used full resolution on Apple silicon and looks
  the same.
- The original shaders no longer need a Mac copy of the game. Setup builds
  them in a few seconds from the shader sources in your own game files, the
  way the Windows game does when it starts. They follow Windows: flags and
  other animated surfaces move as they do there, where the Mac 1.3 shaders
  used an approximation. An existing shader cache is rebuilt once on the next
  launch. A Mac copy still works as a fallback.
- The CoD2x machine ID (`cl_hwid2`) is now computed the way CoD2x computes it:
  the same list of hardware details, hashed the same way. Only the hash is
  sent. This changes the ID once for existing Mac players, so a CoD2x server
  sees them as a new machine one time.
- Anti-aliasing works. Edges are smoothed at the `r_aaSamples` setting, 2x
  by default as on Windows; before, the setting did nothing and wires and
  rooflines were jagged. `r_aaSamples 1` turns it off and looks exactly as
  before. It cost no measurable frame rate in testing.
- Release packaging can sign with a real identity (`COD2_SIGN_IDENTITY`), which
  adds the hardened runtime and a secure timestamp, and can notarize and staple
  the app (`COD2_NOTARY_PROFILE`, a `notarytool` keychain profile). Without
  them, packages stay ad hoc signed as before.
- Anisotropic filtering works, as on Windows 1.3: samplers that ask for it get
  `r_anisotropy` (default 8). Mac 1.3 passed the setting's raw bits as a float,
  so it never took effect. Default settings look the same; with
  `r_textureMode anisotropic`, distant ground textures are sharper.
- Distant and angled textures are filtered as on Windows 1.3. Mac 1.3 mixed up
  two OpenGL filter modes, so the default bilinear setting picked the nearest
  texel on minified textures. Walls and grass in the distance looked grainy,
  shimmered as you moved, and grass looked slightly blue-green. Replayed demos
  now match the Windows game much more closely.
- Require macOS 26 or later on Apple silicon for new builds, with Xcode 26+
  and the macOS 26+ SDK for source builds. Align the launcher, engine, bundled
  SDL libraries, icons, bundle metadata and verification with that minimum.
- Build and audit the complete release package on macOS 26 for pull requests,
  including launcher core and isolated first-run checks before release tagging.

### Fixed

- Whole-number arithmetic wraps around as it did in the original game.
  The game was written for processors where a number past its largest value
  wraps to the smallest; the Mac build left that case undefined, so the
  compiler was free to assume it never happens. A network message parser test
  found one such sum (a server time at its maximum plus a small step).
- Icons stored without smaller copies (mipmaps) draw again. They showed as
  black squares, or white with approximate shaders: the melee, artillery and
  crush kill-feed icons, the scoreboard's dead icon, and many mod icons. The
  game asked OpenGL for mipmap levels these images don't have. It now uses
  the levels each image has, as on Windows.
- Setup now takes the original shaders from the Universal (PowerPC and Intel)
  Mac 1.3 game as well as from an Intel-only copy. Before, setup reported a
  Universal copy as not found and the game fell back to approximate shaders,
  which lose clouds and most lighting detail. The setup text now says so
  plainly.
- CoD2x builds start with the stock packet and snapshot rates
  (`cl_maxpackets 30`, `snaps 20`), as CoD2x does. They started at 125 and 40,
  and some 1.3 servers then showed "Connection Interrupted" and a ping of 999.
  You can still raise both to CoD2x's maximums of 125 and 40, and competitive
  CoD2x servers still set them. Your saved config keeps its value: if you
  played an earlier build, enter `/cl_maxpackets 30` once in the console.
- Script HUD text for the map name and game type showed each other's value.
- After damage, the health bar's red section now waits its full delay before
  shrinking, as in Mac 1.3.
- `com_statmon 1` no longer crashes the client on the first frame after a file
  read, and it shows the file-access and slow-frame warning icons instead of
  trying to load their text as images.
- A sound restart no longer drops streamed sounds that come after a free
  stream channel.
- Release and launcher builds take the macOS SDK from the selected Xcode, so
  packaging works on Xcode 27 when the Command Line Tools SDK is also present.
  The layered app icon is now compiled the same way; on such a Mac the package
  got the flat fallback icon.
- A script call on something that is not an object no longer crashes the
  server. This happened with the stock test-client script once a map rotation
  left the server full. The call is skipped and the script carries on, as on
  the Linux 1.3 server.
- Builds without CoD2x now search `fs_homepath/main` when it is not the game
  folder, as Mac and Windows 1.3 do. Before, the game wrote profiles to
  `fs_homepath/main/players` but did not find them there on the next launch,
  so maps would not load. Release builds already searched it.
- Indexed draws give OpenGL the vertex range their indices actually use. The
  range was off by the base vertex, as in Mac 1.3, which Apple's GL tolerated
  but Mesa read past the vertex buffer on. Apple client storage is set only on
  GLs that have it. The picture on macOS is unchanged.

### Known limitations

- Voice chat has not been tested with a real microphone, only with a
  recording in its place.
- The launcher skips the intro videos.
- Third-person player models sit about half a pixel off the original in
  replayed demos.
- OpenGL remains the renderer, and frame-pacing hitches remain.
- The player directory is still unavailable.

## [0.2.1] - 2026-10-13

[Verification scope and remaining release gates](docs/macos-port/release-0.2.1-verification.md).

### Added

- Config Studio: named loadouts, guided and expert settings, keyboard and mouse
  binds, multi-command and cycling binds, reviewed imports and change history.
  Ordinary edits save to the selected loadout; imports, suggested changes and
  restores require an explicit action. Settings for this Mac remain separate
  from each loadout.
- Ruleset-aware Check-up and a server ready check before joining. Unknown rules
  remain unknown, and suggested changes require an explicit action.
- Local match summaries for frame timing and connection measurements, with
  bounded storage and a privacy switch. These measurements stay on this Mac.

### Fixed

- Server metadata stays readable at the minimum window size, with connection
  actions kept visible. Config Studio uses native group selection and keyboard
  focus, responsive Expert controls and system popover materials.
- Native voice playback initialization, the original 8192 Hz playback rate,
  server relay packet handling and spectator routing. Microphone capture is
  still unavailable.
- CoD2x profile configuration paths and launcher profile startup ordering.
- Command-chain validation now uses the native engine's actual value limits
  and quoted-command behavior.
- Native rendering, animation, audio and network layout errors found by the
  composed fidelity checks. Evidence and remaining qualifications are recorded
  in the workstream reports.

### Known limitations

- The player directory remains unavailable until redistribution permissions
  and a signed publication are in place.
- OpenGL remains the renderer. Metal and further frame-pacing work remain on
  the roadmap.
- Original 32-bit Linux binary comparison still requires a working multilib
  host; native-off source preservation is a separate check.

## [0.2.0] - 2026-10-05

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
- Thrown grenades fell through the world: movement checked the wrong
  bounce flag, so frags exploded far below the map and smoke never formed
  a cloud.
- Effect colour curves and scales were not parsed, so smoke and explosion
  particles drew tiny and black.
- The world turned black for several seconds after a nearby explosion: the
  OpenGL backbuffer copy behind the screen-feedback effect never copied.
- Ordinary projectile impacts were skipped, and splash damage swapped its
  inner damage with its radius.
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

### Known limitations

- OpenGL remains the renderer. The native Metal renderer is in progress for
  0.3, and so is presentation through Metal for smoother frame pacing.
- Exclusive fullscreen can fail to restore after the display sleeps. The
  default desktop fullscreen is not affected.
- The byte-for-byte comparison of the original 32-bit Linux build needs an
  x86_64 Linux host and runs in CI only.

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

[Unreleased]: https://github.com/STiXzoOR/cod2-silicon/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.3.0
[0.2.1]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.2.1
[0.2.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.2.0
[0.1.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.1.0
