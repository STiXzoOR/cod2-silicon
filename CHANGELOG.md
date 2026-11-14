# Changelog

All notable changes to CoD2 Silicon are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.3.0] - 2026-11-14

The port was audited against the original Windows 1.3 game, and its picture
was compared with it on every stock map in replayed demos. The differences
that remain are listed in the verification record. The original shaders are
built from your own game files.
[What was tested and what was not](docs/macos-port/release-0.3.0-verification.md).

### Fixed

- Recoil, spread, tracers, item drops and randomized sound/effects now use the
  original Windows random stream. CoD2x keeps its separate shotgun and server
  challenge stream, and renderer clouds and water retain their own stream.
- Dropped weapons now tumble along the same axes as Windows for each random draw.
- Bullet and rocket spread now preserve the original degree conversion and
  trigonometric rounding.
- Dense effects now share the original fixed block arena and its allocation
  limits, including delayed effects and reuse across effect types.
- CoD2x recognizes stock pack names with Windows filename case and separator rules.
- CoD2x finds required and latest zPAM packs even after large lists of ignored packs.
- Demo playback uses upstream's base movie directory without an extra home override.
- CoD2x preserves the upstream 8192-character info-string boundary without
  overflowing callers' buffers.
- Fresh CoD2x profiles now remember shadows, soft smoke, effect sorting,
  dynamic-light limits and model depth-prepass choices after restarting.
- Team text colors now use Windows nearest-even byte conversion.
- Sound pitch, playback positions and durations now preserve Windows arithmetic
  and rounding, including the distinct order used when starting a sound.
- Shellshock duration, logo timing and fog colors retain Windows precision
  when converting parsed decimal values.
- CoD2x spectator labels no longer leave later HUD drawing clipped when a
  crowded frame runs out of render commands.

- CoD2x now honors competitive settings requested by stock 1.3 servers, including
  frame-rate and network limits, shadows, and the restriction on `wait`. Turning
  the rules off restores the same setting limits as upstream.
- CoD2x's nine film-color controls now adjust the 3D scene, including separate
  sky saturation, while leaving the HUD and the default picture unchanged.
- Older servers retain their literal text and player names; CoD2x's extended
  formatting is enabled only where upstream enables it.
- Radar firing flashes keep their captured position when the radar moves or
  zooms. Radar calibration starts at the correct origin and handles each mouse
  update. Long vMix overlay names no longer lose characters prematurely.
- CoD2x update notices explain how to update this native app and open its release
  page. They no longer prevent joining servers that still accept this version.
- Explicit `writeconfig` works with automatic saving disabled. Passwordless
  connect links preserve the existing password, and engine archive setup no
  longer deletes CoD2x files from the game-data folder.
- Mod WebSockets deliver messages received with the connection reply and accept
  long upgrade requests. Server shutdown cancels pending mod HTTP work without
  waiting for a stalled DNS resolver.
- CoD2x diagnostic reports preserve complete log lines and legacy text, distinguish
  long messages, retry failed delivery and send the oldest queued reports first.
- Original 1.3 no longer starts CoD2x's freeze watcher or handles CoD2x links.
  Native CoD2x keeps PunkBuster off and explains that its provider is unavailable.

- CoD2x now announces revision 7 and implements its extra player information,
  spectator and demo x-ray, occluded outlines and glow, health fills, head names
  and weapon icons, with the upstream controls. Revision checks accept this
  client on both current revision-7 and released revision-6 servers. The native
  client has joined our servers at both revisions; revision-7 spectator health
  and recorded-demo playback have been captured.
- Automatic demo recording follows name changes, playback and disconnects at
  the right time. Upload markers, retry and quit behavior, filenames and the
  displayed transfer speed now follow upstream. vMix keeps long camera/API
  names and cancels pending requests when disconnecting.
- CoD2x stance transitions retain their state and use the correct changing
  blend duration. Movement on older servers and third-person camera arithmetic
  follow upstream; competitive requests apply at the correct lifecycle points.
- Asian console text wraps at the same boundary as upstream. Chat uses its
  intended fading background, and hidden-IP text uses the upstream length limit.
- CoD2x file selection handles quoted required files and active-profile configs.
  Original 1.3 ignores shared CoD2x menu archives without deleting them.
- Mod HTTP and WebSocket receive limits, response parsing and shutdown callbacks
  follow upstream more closely. zPAM manifests and download history retain long
  lists, and downloads enforce the expected status, length and transfer format.
- CoD2x reports retain console text, apply the upstream retry and retention rules,
  and limit freeze reports to three per run. Loading refreshes the freeze
  heartbeat, and fatal reporting uses a real three-second wait deadline.
- Decals no longer show through surfaces a few units in front of them. Grime,
  marks and posters were pushed forward in depth twice as far as on Windows;
  on Railyard the dirt on the brick wall drew over the factory windows.
- A script that hits a runtime error no longer leaves its failed values on the
  script stack. A loop of such errors used to grow the stack until the server
  stopped; it now recovers as Windows does.
- The CoD2x console types 8 and 2 from the keypad instead of walking the
  command history, the mouse-rate meter counts a late update as upstream does,
  and a CoD2x window saved on a display that is no longer connected opens on
  the main display.

### Known CoD2x limitations

- Revision-7 spectator/demo, automatic recording/upload, film controls and
  Original isolation have running-game evidence. Main-menu/update-button
  readbacks remain black and need physical inspection; this is not a claim
  that the on-screen menu is black. Original baseline and film-control replays
  are unchanged across all five views. The
  [compatibility matrix](docs/macos-port/cod2x-compat.md) records exact proof
  and remaining platform/service limits.
- The experimental native Metal renderer lacks the CoD2x film and x-ray passes;
  the shipped OpenGL renderer implements them. The CoD2x filesystem still caps
  directory lists at 1024 archives before filtering. Input, focus and audio
  hardware checks remain with their platform owners. PunkBuster protection is
  unavailable in both engines.

### Added

- Private logs for each game session, available from Settings → Advanced → Logs
  and failed-start or crash details. The last ten sessions stay on this Mac within
  a 20 MiB total limit, with passwords and CD keys filtered before writing.
- Check-up reports freezes longer than 0.1 s, their longest duration and recorded
  match times. Older match summaries still show the longest frozen frame.

- Play with CoD2x or with the original 1.3. **Settings → General → Game
  version** chooses which game starts; CoD2x is the default. Launching on a
  server that runs the other version starts that version for the match, after
  asking (“Don’t ask again” always uses the server’s version), and goes back
  to your choice afterwards. Config Studio, Check-up and the ready check follow
  the version you play, and the few settings one version cannot hold are kept
  for the other.
- Additional CoD2x compatibility features. As a player: demo
  upload, CoD2x's text colours and killfeed icons, hidden server addresses on
  streamed matches, the 32-port LAN search, zPAM downloads from CoD2x's site,
  the update notice, and vMix control for broadcasts. As a host: your server
  is a real CoD2x server, with its bans and hardware IDs, wallbang settings,
  zPAM selection and updates, the match service, and web requests and
  WebSocket connections for mods. Certificates are checked on every request,
  which CoD2x itself does not do. Upstream executable updates are announced;
  their Windows/Linux binaries are not installed by this native engine.
- CoD2x's update check, done the way CoD2x does it. With CoD2x on, the game
  asks the CoD2x project's update server for the current CoD2x version each
  time the main menu opens, and shows the game's Auto-Update notice. As in
  CoD2x on Windows, the request carries your CoD2x version, an MD5 hash of
  your CD key, an install ID, your CoD2x hardware IDs and a short description
  of your Mac. It goes over HTTPS, and over plain UDP when that fails. If
  CoD2x marks an update as required, the notice directs you to this app’s
  releases; servers still enforce their own version requirements. The original
  1.3 sends none of this.
  [What the app sends](README.md#what-the-app-sends) lists everything.
- CoD2x's own menus and icons. **Settings → Game Data → CoD2x menus and
  icons** downloads
  the official CoD2x 1.4.6.8 release from the CoD2x project's GitHub page,
  checks it, and keeps only those files on this Mac; or copies them from a
  Windows game folder that has CoD2x installed. The launcher manages them for
  CoD2x games; Original 1.3 ignores matching archives in a shared game folder.
  Your game folder is never changed by engine archive setup.
- Install updates from the launcher. When a newer release is found, **Install
  and Relaunch** downloads it, checks it against the release's checksum and
  signature, replaces the app and restarts it. Nothing installs without that
  click, and Cancel stops it until the new app takes the old one's place.
- A Settings window with seven panes: General, Display, Game Data, Privacy,
  Updates, Advanced and About. It can hide the launcher while you play, follow
  or override the system's appearance, check for updates once a day while the
  launcher is open (on unless you turn it off), name the display and the
  resolutions the game can use, rebuild your shaders, replace your CD key,
  delete match summaries, reset launcher settings, copy the last game log,
  list everything the app contacts, and show every bundled component's
  licence in full. **Report a Problem** opens a prefilled GitHub issue in your
  browser; the app sends nothing itself.
- Crash reports you can choose to send. Two switches in **Settings →
  Privacy**, both off unless you turn them on: one sends CoD2x's reports for
  faults in CoD2x's own code to the CoD2x project, as CoD2x itself does; the
  other, in builds that have a report address, sends everything else to this
  project, with your home folder's name left out and never your CD key.
- Voice chat sends your microphone when you play the original 1.3. Hold the
  voice chat key on a server with voice on and macOS asks once for microphone
  access; the microphone is open
  only while you are sending and for two seconds after, so the system's
  microphone indicator shows exactly when the game listens. The original
  games kept the microphone open the whole time. Tested with a recording in
  place of a microphone between two clients, not yet with a real one.

### Changed

- The Original 1.3 engine reports its platform as `macOS-arm64` in the
  `version` text that servers and status queries see, instead of the Intel
  Mac release's `MacOSXS-i386`.
- Shadow-cookie clears preserve scene depth near the bottom-left corner.
  Character shadows also remain visible when soft smoke edges are enabled.
- Glow samples the correct sky direction on widescreen displays; glow and menu
  blur align to pixel centers and cover the right edge.
- Heat distortion samples the correct scene pixels.
- Smoke fades where it meets walls, ground and characters when Soften Smoke
  Edges is enabled.
- Crowded custom menus retain the original drawing limit instead of dropping
  elements early.
- The renderer logging option writes its requested graphics-call trace.
- Changing specular power preserves the original lighting response instead of
  making world surfaces excessively shiny.
- Colored impact marks preserve red and blue; cylinder effects retain their tint
  and transparency.
- Impact marks use the original clipping arithmetic at surface edges.
- Sun glare fades with the visible area of the sun, using GPU occlusion queries.
- Sun glare stays visible after loading or changing maps.
- DirectX 7 water keeps its blue-gray color instead of turning purple or yellow
  with the viewing angle.
- Water wave patterns follow the original renderer's random sequence.
- Snow cloud positions follow the original renderer and remain consistent
  across repeated demos and both game versions.
- Unsupported 3× antialiasing falls back to 2×, as in the original game.
- Custom models accept Windows path separators.
- Menus with a blurred game background avoid an unnecessary full-screen
  copy when anti-aliasing is enabled.
- Local match summaries retain slow-frame timings and renderer activity to
  help diagnose freezes during play.
- Graphics resolutions are listed once, in ascending order, up to the selected
  display’s native pixel size. An old oversized setting falls back to a size
  the display supports; a 5K panel is no longer offered a scaled 6K backing.
- Turning Shadows on draws soft character shadows in both game versions,
  as in Windows 1.3. Sunlit walls and ground keep their lighting. In the original
  multiplayer game, your own body casts a shadow in third person, not in
  the normal first-person view.
- Resolution choices follow the selected display’s native pixels, including notebook
  sizes, in ascending order without duplicates. Unsupported saved sizes move down
  to a fitting size and Settings explains the change. New installs start at native
  size through 4K or half the panel’s dimensions above it. Display explains the cost
  of larger sizes, anti-aliasing and fullscreen modes.
- Home’s recent-server rows keep the map, game mode and version readable, including
  at the minimum window size. About indicates when its credits and footer continue
  below the window.

- Setup has six steps: welcome, game data, CD key, shaders, your name and
  display, and a summary. A wrong folder says which archives are missing, the
  CD key is checked as you type and can be skipped, a stored key is never asked
  for again, and running setup again from Settings can be cancelled at any step
  without losing your place in the launcher; only confirmed choices are kept.
- The sidebar's player card edits your in-game name in place, showing its
  colours, and saves it as you type. Jump to… (⌘K) finds servers, loadouts,
  settings and menu actions. The menu bar has Server and Loadout menus, and the
  Dock menu lists recent servers.
- A `cod2x://` or `cod2://` link opens Connect to Address with the address filled
  in, preserving a supplied password in memory, instead of starting the game. Opening a demo plays it; opening a config
  shows the import review.
- While you play, the launcher keeps its Dock icon and hides its window when
  **Settings → General → Hide CoD2 Silicon while the game is running** is on.
  Quitting it with the game open asks whether to quit the game too.
- Launcher settings save automatically. Invalid advanced options stay editable
  with an explanation, while your last valid options remain in use. Extra
  launch options are kept as you typed them and accept lines copied from a
  shortcut or the console, such as `+set com_hunkMegs 512`.
- The server browser shows how old its results are, explains empty lists, and
  offers search (⌘F), reversible sorting and server context menus. All,
  Favorites and Recent switch between every server, your starred ones and the
  ones you played; Filters (game version, mode, ping, empty, full, locked) are
  kept between launches. Refresh shows how many servers have answered and can
  be stopped, keeping what arrived; when the master servers don’t answer, the
  last list stays with its time. **Connect to Address…** (⌘L) replaces the
  direct-connect bar: it takes an address, a host name or a `cod2x://` link,
  asks that server and shows what answered. A locked server asks for its
  password, which can be remembered in your keychain on this Mac. The ready
  check offers another loadout that already suits the server. Copy Join Link
  copies a `cod2x://` link CoD2x on Windows also opens.
- Library has three tabs. Matches lists how each recent match ran (typical
  frame rate, ping and the connection in a word) from the summaries kept on
  this Mac, with Check-up one click away. Demos and Screenshots list the newest
  first and refresh when you come back to the launcher. Recordings play, can be
  renamed, copied or moved to the Trash (with Undo); screenshots open with a
  double-click and offer Quick Look, Copy Image and Share.
- Home shows the server you last played with Launch to rejoin it, the three
  servers you joined most recently, how your last match ran, this release's
  notes, and the loadout and frame cap your next game uses, with a menu to
  switch. Game data not found, a missing CD key or approximate shaders each get
  a chip that goes straight to the fix. While the game runs, Home offers
  Return to Game and Quit Game; when an update is ready it offers What's New
  and Install Update.
- Config Studio can create, duplicate, rename and export loadouts, or move an
  unused loadout to the Trash with Undo. The Loadout menu and row menus use
  the same actions. Check-up opens a selected match from Home or Library and
  explains frame rate and connection quality in words, with figures behind
  Show Details. Configs stays read-only while the game runs.
- With CoD2x on, voice chat is off, as in CoD2x. It works with the original
  1.3.
- With CoD2x on, the game no longer writes its recommended graphics settings
  into a new profile, as in CoD2x. The launcher supplies the first-run
  picture, the 250 fps cap and raw mouse input unless your loadout sets them.
- A new profile starts with glow, heat distortion, soft smoke and shadows on
  in both engines, as the game recommends for its best graphics cards. The
  game's own table does not know Apple graphics and used to turn them off.
  Existing profiles keep their settings.
- Crash reports from a CoD2x game are named `cod2x_crash.<date>_<time>.<pid>.log`
  and follow CoD2x's form; the original 1.3 keeps `cod2_crash_<pid>.txt`.
  Nothing is sent unless you turn a report switch on in Settings.
- The game's videos play. The `cinematic` console command and the intro
  (publisher and studio logos, then the game's intro film) were switched off
  because they showed a black screen. Started without the launcher, the game
  plays the intro once on its first run, as on Windows; any key skips each
  part. The launcher does the same for a new player: the first launch to the
  game menu plays the films, and later launches, server links and demos go
  straight in. Players who have launched before are not shown them again.
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
- Commit IDs from 0.2.1 onward changed on 2026-11-11, when a personal email
  address was removed from the commit history. The files in every commit are
  the same, and the `v0.2.1` tag moved to the matching commit. An existing
  clone needs a fresh clone or a hard reset to `origin/main`.

### Fixed

- Remove the spurious fall-damage camera kick and restore Windows 1.3 landing
  slowdown and view dips, small-step footsteps, acceleration rounding, aiming
  sway and stance-transition timing.
  Moving objects, collision contacts and explosion damage also follow the
  original arithmetic at threshold boundaries.
- Attach weapon effects to the gun and restore the original cylinder-smoke
  random draw order, tracer lifetime, equal-distance effect ordering and HUD
  layer ordering. Turret prompts show the matching weapon icon, telefrags use
  the generic death icon, and locally hosted item pickups retain their sounds.
- Match Windows screen scaling, HUD rounding, script waits, slow-motion timing
  and color conversion. Control rebinding keeps the pulsing key name; visible
  menus receive hover updates, loading images retry correctly, and deleting a
  favorite no longer uses a selection made stale by a server-list refresh.
- Original 1.3’s startup banner correctly identifies macOS on Apple silicon.
- Brief movement-key presses queued during a slow frame retain their held
  duration, as in Windows 1.3.
- The physical console key works across keyboard layouts, and the original
  extended Latin key bindings are available.
- Console and chat support Ctrl+V paste, Ctrl+C clear and Ctrl+H backspace.
  Typed and pasted text is converted to the game's language encoding so
  accented letters and punctuation no longer turn into unrelated characters.
- Scrolling outside a focused menu list moves focus to the next or previous
  control, as in Windows 1.3.
- Keypad navigation now works in menu lists, edit fields, focus movement and
  console history, matching the original keyboard alternatives.
- Mono output and the 11 kHz and 22 kHz sound settings now use the original
  channel routing, mixing rate and downsampling behavior. Positional sound
  uses the original gain steps.
- Ambient tracks and music decode in the background with bounded buffers,
  avoiding the full-track decode pause and the decoded-size limit on long music.
  A bounded read cache also prevents repeated archive decompression while
  opening tracks stored in IWD files.
- Game files load consistently on case-sensitive disks, including paths whose
  directory or filename casing differs. Listings, writes, renames and deletes
  follow the same rule; the default case-insensitive disk keeps its OS behavior.
- Room and shellshock reverberation now reproduce the original software mixer’s
  room presets, reflections, decay, damping and stereo spread.
- Consecutive screenshot commands retain every image, and `cl_avidemo` saves
  frames continuously instead of continually postponing the capture.
- `viewlog` opens a native scrolling console with a command field; it can be
  hidden, restored and minimized as in the original game.
- Fatal engine errors show their explanation before the game exits and return
  a failure status.
- Minimized games pause for five milliseconds between frames, as in Windows 1.3.
- Original 1.3 restores the saved window position after restarting video.
- HTTP downloads fail after 30 seconds without receiving data, instead of
  waiting indefinitely for a stalled server.
- After an abnormal exit, Original 1.3 offers its safe-mode recovery choice
  on the next launch. A normal exit clears the recovery marker.
- File loading follows Windows archive/search-path priority. Pure servers
  reject loose string tables and RoQ files outside localized search paths.
- Archive discovery accepts dot-prefixed `.iwd` files and rejects filenames
  that only end in `iwd` without the extension separator.
- Wheel messages change weapons once each, including high-resolution scrolling,
  as in Windows 1.3.
- Opening chat in a window keeps mouse-look active. Original 1.3 windowed
  menus use the original cursor speed and can reach every edge.
- Sounds in the world are placed as in Windows 1.3. A sound straight ahead
  was 3 dB louder than in the original and one behind you 5.5 dB louder,
  because sounds were panned on a different curve and front and back were
  treated alike. Now a sound behind you is quieter than the same sound in
  front, as in the original, which makes direction easier to judge. Measured
  against the original game's output, every direction is within about 0.1 dB.
- Sounds keep their treble. The game mixed everything at 48 kHz, which put
  every sound through a simple resampler and cost about 1 dB at 8 to 12 kHz
  and 4 dB above 16 kHz against the original. It now mixes at the game's own
  44.1 kHz, as the original does.
- Two small differences from Mac 1.3, found by compiling every source file
  with the original compiler and comparing each function with the original
  game: a failed language load no longer reports a warning when
  `fs_ignoreLocalized` is set (it checked `developer`), and the errors for a
  damaged map file show their text as written. A second pass matched four
  more error texts, the rate message now printed for `sv_debugRate`, a host
  no longer applying its own server's settings twice, and the script calls
  `deactivateReverb` and `deactivateChannelVolumes`, which do nothing in 1.3
  multiplayer because of a mistake in the original and now do nothing here.
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
- CoD2x's services (the update check, zPAM updates, the match service, script
  web requests, vMix, and crash and error reports) were tested against
  stand-ins on a Mac, not against the CoD2x project's own servers.
- The new Settings rows (CoD2x files, the report switches, Install and Relaunch)
  were tested through the launcher's tests, and nobody has installed an update
  from a real release yet: 0.3.0 is the first version that can.
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
[0.3.0]: https://github.com/STiXzoOR/cod2-silicon/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.2.1
[0.2.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.2.0
[0.1.0]: https://github.com/STiXzoOR/cod2-silicon/releases/tag/v0.1.0
