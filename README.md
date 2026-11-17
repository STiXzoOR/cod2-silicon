# CoD2 Silicon

**Call of Duty 2 multiplayer, native on Apple silicon. CoD2x 1.4 compatible.**

CoD2 Silicon ports the reconstructed [opencod2](https://github.com/opencod2/opencod2)
engine to native macOS arm64. It runs directly on Apple silicon, with raw mouse
input, native audio and an OpenGL renderer. Bring a licensed copy of Call of
Duty 2: no game content is included.

**To play:** download the app from
[Releases](https://github.com/cod2unbound/cod2-unbound/releases), move it to
Applications, open it, and point it at your Call of Duty 2 folder. Setup does
the rest. A Windows 1.3 install's folder works as it is.

## Status

Version `0.3.3` is playable on original 1.3 and CoD2x 1.4 servers, with a
native launcher, server browser and Config Studio. Movement, firing, grenades, mounted
turrets, HTTP mod downloads and round restarts have been tested, and every
menu is opened by an automated sweep. The picture is compared with the
original Windows game frame by frame in replayed demos.
Smoother frame pacing and active Game Mode remain goals. See the
[verified results below](#verified-results) for compatibility coverage,
rendering comparisons, fullscreen limits and measured performance.

## Requirements and game data

- An **Apple silicon Mac**; Intel Macs are not supported by this port.
- **macOS 26 or later**.
- Your own licensed Call of Duty 2 1.3 game data and CD key.
- No Mac copy of the game is needed: the app builds the original shaders from
  the game's own files.

Keep the game outside this repository. The data folder must contain
`main/iw_00.iwd` through `main/iw_15.iwd` and matching localized archives.
Existing Windows 1.3 data works as it is. See
[getting your game data](docs/macos-port/game-data.md).

## Install

### Prebuilt app

1. Open [GitHub Releases](https://github.com/cod2unbound/cod2-unbound/releases).
2. Download `CoD2-Silicon-0.3.3-macos-arm64.zip` and `SHA256SUMS` into the same
   folder. Verify the archive, then unzip it:

   ```sh
   shasum -a 256 -c SHA256SUMS
   ```

3. Move **CoD2 Silicon.app** to `/Applications` and open it.

The app is signed with a Developer ID and notarized by Apple, so it opens
like any other downloaded app.

### From source

```sh
git clone https://github.com/cod2unbound/cod2-unbound && cd cod2-silicon && ./scripts/install.sh
```

The installer needs Swift 6 (Xcode 26 or later), the Xcode Command Line Tools and CMake and explains how to
get them. It downloads and builds the pinned SDL release itself (no Homebrew),
compiles the client and installs `~/Applications/CoD2 Silicon.app`. No original
game binary is needed to build. A locally built app is not quarantined, so
Gatekeeper does not block it.

## First run and playing

Open **CoD2 Silicon.app** to use Home, Servers, Library and Configs (⌘1–⌘4).
Setup walks through your game data, CD key, shaders, name and display mode,
then shows a Ready summary. It finds a game folder or lets you choose or drop
one. A CD key can be skipped; servers need one. A verified key is stored in
`~/.cod2/preferences`. Never share that file or your key.

The app builds original shaders from your own game files. If that fails, you
can choose a Steam Mac copy or continue with approximate shaders. Configs,
logs, demos, screenshots and the shader cache live in
`~/Library/Application Support/CoD2 Silicon`.

**Launch** (⌘↩) rejoins the last server, or the selected server when Servers
is showing one. **Open Game Menu** (⇧⌘↩) opens the multiplayer menu. Servers
has All, Favorites and Recent views, saved filters, and **Connect to Address…**
(⌘L). A server using the other game version asks to switch for that match;
your default version stays in Settings. While playing, the launcher keeps its
Dock icon and can hide its windows. Home offers Return to Game and Quit Game.

**Servers → Hosting** runs your own server on this Mac, with CoD2x or the
original 1.3, for people on your network or listed publicly. Choose the maps
and rules, start it, and the page shows who is playing, the address to share
and the server's log. Competitive rules use zPAM, which the launcher downloads
from the zPAM project's releases when you ask for it; it is not bundled.
**Export for Another Machine** writes `server.cfg` and start commands for a
Linux or Windows server instead. See [the server guide](docs/server.md).

Three things worth knowing before friends try to join:

- **Ports.** The game's own local-server list only asks ports 28960 to 28963
  (original 1.3) or 28960 to 28991 (CoD2x). Hosting keeps to them; on any
  other port friends connect by address, and the page says so.
- **Firewall.** The first start may bring up macOS's question about incoming
  connections for the server. Deny means nobody else can see or join it.
- **Wi-Fi.** A Mac hosting over Wi-Fi can make players freeze and jump while
  AirDrop or Handoff is in use. A network cable is steadier; the guide's
  [Wi-Fi section](docs/server.md#wi-fi-players-who-freeze-and-jump) has the
  other remedies.

**Settings** is a separate window, reached from the sidebar, app menu or ⌘,.
Its seven panes cover the launcher, display, game data, privacy, updates,
advanced launch options and About. Changes save automatically. The renderer
is OpenGL; Metal controls are not shown in this release. New installs start borderless at the display’s native size up to 3840 × 2160,
or half its width and height above that (2560 × 1440 on a 5K panel).
The resolution and vertical sync belong to each loadout: change them in
Configs or in the game’s own Graphics menu and they stay as you set them.
Configs lists the sizes your display offers; a size it can’t show is replaced
by the largest that fits, and Home tells you beforehand. Settings › Display
keeps the display and the mode: Borderless stretches the picture to the
desktop, Full Screen shows it pixel for pixel. The other defaults are a
250 fps cap, vertical sync off and raw mouse input.

**Configs** holds loadouts: frame cap, mouse, audio, graphics and binds, plus
Check-up and History. Create, duplicate, rename, export or delete an unused
loadout with Undo; **Use for Next Game** selects the one the game will read.
Home also has a loadout picker. Configs is read-only during a game.

Configs opens on its own home page: your loadouts, and **player setups** from a
[public catalogue](https://github.com/cod2unbound/cod2-configs). The catalogue
collects configs that were published on the open web, credits each to the page
it came from, and leaves out names, chat lines, passwords, server settings and
timed key macros. It does not claim a player approved a listing, and removes
one on request. You see what a setup would change and review every line before
anything is imported. **Add a Config** brings in one of your own from a file, a
link, the clipboard or a Windows install, with the same review.

**Check a Config** reads any config on this Mac, for CoD2x or the original 1.3
and optionally a set of server rules such as zPAM's. It marks, line by line,
settings that don't exist, values the game refuses, settings that only change
with cheats on, repeats and stored passwords, and offers a fix for each. Fixes
change a copy; the file you checked is never written.

**Library** keeps Matches, Demos and Screenshots. Match summaries stay on this
Mac and open Check-up for that match. Check-up shows freezes over 0.1 s,
the longest and the recorded times; older summaries still show the worst frame.
Each game session also keeps a private, filtered output log under `logs` beside
`main`: the last 10 sessions, at most 4 MiB each and 20 MiB total. Passwords and
CD keys are filtered before writing. **Settings → Advanced → Logs → Show in
Finder** selects the latest session. These logs are never sent automatically. Demos can be played, renamed, copied
or moved to the Trash with Undo. Screenshots support Quick Look, Copy Image
and Share. **Jump to…** (⌘K) finds servers, loadouts, Settings and actions;
in Configs it finds game settings.

A `cod2x://` join link opens Connect to Address for review before Launch;
pasted `cod2://` links work too. For example:

```sh
open -a "/Applications/CoD2 Silicon.app" 'cod2x://connect/127.0.0.1:28960'
```

Replace the address with your server. For a source install, use the app under
`~/Applications`. For direct CLI launch, run the executable with `--play`;
append `--exit-after-game` to close the launcher when the game quits, or pass
trusted engine arguments after `--`.

## Reporting a problem

**Settings → About → Report a Problem** opens a new issue on GitHub in your
browser, filled in with the app version, the game versions installed, your
macOS version and Mac model, and the shader and display modes. You can edit
all of it before you post, and the app sends nothing itself.

- **Include the last game log** copies the end of the last game session's log
  to the clipboard for you to paste into the issue. **Include the last server
  log** does the same for the last server you hosted, when there is one.
- Logs are filtered as they are written: lines with a password or a CD key
  are left out, and your user name is replaced in paths. A log can still hold
  server names and player names, so read what you paste.
- The game's logs are under `logs` in
  `~/Library/Application Support/CoD2 Silicon` (**Settings → Advanced → Logs →
  Show in Finder**); a hosted server's are in its own folder (**Show in
  Finder** on its page).

## What the app sends

- **As the original game does:** the game's master and key servers, and the
  servers you browse and join.
- **With CoD2x on:** the CoD2x project's master server for its server list, and
  CoD2x's update check each time the main menu opens. Like CoD2x on Windows,
  that check carries your CoD2x version, an MD5 hash of your CD key, an install
  ID, your CoD2x hardware IDs and a short description of your Mac, over HTTPS
  or, when that fails, plain UDP. CoD2x servers you join receive your hardware
  ID hash. zPAM files a server needs are downloaded from the CoD2x project's
  site.
- **With the original 1.3 running**, the game sends nothing to the CoD2x
  project. **Settings → General → Game version** sets your default; joining
  a server can use the other version for that match.
- **The launcher** contacts GitHub when you check for updates (once a day while
  it is open, unless you turn off **Settings → Updates → Check for updates
  automatically**), install one, or download the CoD2x files. It asks both
  master servers when you refresh the server list or enable refresh on opening
  Servers, and asks a single server when you type its address in **Connect to
  Address**. Passwords you choose to remember stay in your keychain on this Mac
  and are sent only to that server. **Settings → Privacy** lists these contacts.
- **Configs** fetches the player setups catalogue from GitHub when its home page
  opens, and a setup's config when you look at it.
- **A server you host** contacts nothing while it is set to My Network. Set to
  The Internet, it lists itself on the master servers. A CoD2x server asks the
  CoD2x project's update service about new versions while it runs, and players'
  games fetch zPAM and its maps from the CoD2x project's site. Getting zPAM
  downloads it from GitHub.
- **Report a Problem** opens a prefilled issue in your browser; the app sends
  nothing itself.
- **Crash reports** are sent only if you turn a switch on in **Settings →
  Privacy**. 0.3.0 has one, for CoD2x's own reports to the CoD2x project. It
  is built without a report service of this project's own, so nothing is
  sent here. Your CD key is never part of a report.

## Known limitations

- The CoD2x game and server run as CoD2x's latest stable release, 1.4.6.8,
  so players on that release for Windows and Linux can join a server you
  host. This was tested against the server's own version rules, not yet with
  a person on the released Windows CoD2x. A CoD2x server still hosted from
  CoD2 Silicon 0.3.0 turns 0.3.1 players away until it updates. A server on
  the original 1.3 takes any 1.3 player.
- Voice chat works with the original 1.3 only; CoD2x turns it off. It records
  from your microphone only while you hold the voice chat
  key on a server that has voice on. The sound path was tested with a
  recording in place of a microphone, not yet with a real one, and the level
  meter in the options moves only while you are sending. Device changes while
  talking are not handled.
- The intro videos play once, the first time a new player launches to the
  game menu, as in the original; any key skips each part. `cinematic
  cod_intro` in the console plays the intro film again.
- The native dedicated server is available from source; see the
  [server guide](docs/server.md).
- Frame-pacing hitches remain; active Game Mode and quiet-desktop display
  restoration still need acceptance testing.
- Some rendering and mod edge cases remain. A server restart during an HTTP
  download can stall it; reconnecting recovered in the tested case.
- This engine inherits security-sensitive networking, file parsing and memory
  behavior from an old reconstruction. It is not hardened. Use trusted content
  and servers; see [SECURITY.md](SECURITY.md).

## Verified results

The following historical measurements retain their original scope; they are not
new performance claims for every release. Verified development results include:

- Online stock 1.3 (protocol 118) and CoD2x 1.4 (protocol 120) play: movement,
  firing, reloading, round restart and respawn; HTTP mod downloads also worked.
  The sessions were short and mostly on empty servers. [Online report](docs/macos-port/reports/WS14-online.md)
- The CoD2x layer follows CoD2x 1.4.6.8 for players and for hosted servers,
  including the match service, zPAM handling, script web requests and vMix.
  This is implementation coverage checked against local stand-ins, not live
  acceptance of every feature. CoD2x updates are announced, never installed,
  and certificates are always checked. An earlier inventory counted 41 of 68
  behaviors. [Earlier compatibility report](docs/macos-port/reports/WS10-cod2x-full.md)
- In 0.3.0, a demo replayed in the original Windows 1.3 game and in this port
  differs by a mean of **0.02 to 0.30 out of 255** per colour value, view by
  view on Toujane, Trainstation, Rhine and Harbor at 1920x1080 with the
  shaders built from the game's files. Thin anti-aliased edges alone account
  for 0.1 to 0.3. Every stock map was compared during the cycle, and the
  port was audited against the original: 119 of 173 findings are fixed and
  12 are still open. The original ran under Wine with DXVK, not on a Windows
  machine. [What was tested, and what still differs](docs/macos-port/release-0.3.0-verification.md)
- 0.3.1 fixed two of the differences 0.3.0 listed (menu panel edges, and fog
  in the Direct3D 7 renderer) and measured a third, the near clip plane, as
  not a difference. [What was tested for 0.3.1](docs/macos-port/release-0.3.1-verification.md)
- 0.3.3 moves the project to github.com/cod2unbound and changes nothing
  in the game. [What was tested for 0.3.3](docs/macos-port/release-0.3.3-verification.md)
- 0.3.2 enlarges the in-game console above 1080 lines and changes what the
  launcher shows first. [What was tested for 0.3.2](docs/macos-port/release-0.3.2-verification.md)
- Before 0.3.0, with the original Mac shaders, three matched Toujane views had
  RGB mean absolute errors of **2.0050, 1.8726 and 0.5856 out of 255** against
  Windows. [Rendering report](docs/macos-port/reports/WS15-render.md)
- Fullscreen rendering up to **6016×3384 (6K)**, with desktop-fullscreen fallback
  when an exclusive mode is unavailable. [Fullscreen report](docs/macos-port/reports/WS17-fullscreen.md)
- A ten-minute combat soak completed without a crash after fixing the first-shot
  stencil-clear fault. [Playable-app report](docs/macos-port/reports/WS18-ship.md)

WS18 measured these two-minute live combat captures on a Mac mini with Apple
M6, macOS 27.0.1 and original Mac shaders:

| Render size | FPS cap | Average FPS | 1% low FPS |
| --- | ---: | ---: | ---: |
| 1920×1080 | 333 | 331.2 | 205.2 |
| 1920×1080 | Uncapped | 798.6 | 354.1 |
| 2560×1440 | 333 | 332.4 | 233.0 |
| 2560×1440 | Uncapped | 663.5 | 326.8 |
| 3840×2160 | 333 | 332.3 | 230.8 |
| 3840×2160 | Uncapped | 417.1 | 256.9 |

The 1440p and 4K runs rendered at those sizes on a 1080p physical display using
desktop fullscreen. Other user applications were running. A 1% low is the
reciprocal of the mean of the slowest 1% of frame intervals. These are
measurements, not guarantees of steady frame pacing. Game Mode activation is
unverified.
See the [roadmap](docs/ROADMAP.md).

## Building for development

Read [CONTRIBUTING.md](CONTRIBUTING.md) for the complete merge gate and data
requirements. Development builds need SDL2-compatible development libraries
(the port was tested with SDL3 + sdl2-compat; `scripts/build-sdl.sh` builds the
pinned versions) and Python 3.9+. No proprietary input is needed: arm64 builds
compile the committed typed-data snapshot in `build/lp64_gen/`, described in
[typed-data generation](tools/datagen/README.md).

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos --parallel 3
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx --target cod2_macos --parallel 3
```

`COD2_CODX=ON` is not a CMake option: use the feature flag shown above.
The original i386 Linux and Windows paths retain their build instructions in
the [upstream README](docs/upstream-opencod2-README.md); WebAssembly has its
[own build guide](src/web/README.md).

| Path | Contents |
| --- | --- |
| `src/PC`, `src/Mac` | Reconstructed engine and renderer |
| `src/platform` | Native macOS display, input, audio and system adapters |
| `cmake`, `tools`, `tests` | Build definitions, verification and test fixtures |
| `docs/macos-port` | [Development log and report index](docs/macos-port/README.md), compatibility and parity research |

## Credits and license

Built on opencod2, with behavior research from CoD2x, cod2engine,
CoD2rev_Server and KisakCOD, and native support from SDL, Apple frameworks,
libcurl and zlib. Most port code and analysis was written by OpenAI Codex and
Anthropic Claude Code agents, then verified with test suites and live server
play. [Full credits and provenance](CREDITS.md)

[MIT](LICENSE) covers this project's own changes after opencod2 `410342a`.
Upstream opencod2 declares no license and retains its authors' rights;
third-party licenses remain applicable. Read [NOTICE.md](NOTICE.md) for scope.

“Call of Duty” is a trademark of Activision. CoD2 Silicon is not affiliated
with Activision, Infinity Ward, Aspyr or the CoD2x project.
