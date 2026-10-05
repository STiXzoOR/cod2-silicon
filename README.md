# CoD2 Silicon

**Call of Duty 2 multiplayer, native on Apple silicon. CoD2x 1.4 compatible.**

CoD2 Silicon ports the reconstructed [opencod2](https://github.com/opencod2/opencod2)
engine to native macOS arm64. It runs directly on Apple silicon, with raw mouse
input, native audio and an OpenGL renderer. Bring a licensed copy of Call of
Duty 2: no game content is included.

## Status

The `0.2.0` release is playable on stock 1.3 and CoD2x 1.4 servers, with a
native launcher and server browser. Movement, firing, grenades, mounted
turrets, HTTP mod downloads and round restarts have been tested, and every
menu is opened by an automated sweep.
Smoother frame pacing and active Game Mode remain goals. See the
[verified results below](#verified-results) for compatibility coverage,
rendering comparisons, fullscreen limits and measured performance.

## Requirements and game data

- An **Apple silicon Mac**; Intel Macs are not supported by this port.
- **macOS 13 or later**.
- Your own licensed Call of Duty 2 1.3 game data and CD key.
- The original Steam Mac copy enables matching shaders; without it the app
  uses approximate shaders, with visibly different lighting.

Keep the game outside this repository. The data folder must contain
`main/iw_00.iwd` through `main/iw_15.iwd` and matching localized archives.
Existing Windows 1.3 data works too; the Mac executable is needed separately
for shader extraction. See [getting your game data](docs/macos-port/game-data.md).

## Install

### Prebuilt app

1. Open [GitHub Releases](https://github.com/STiXzoOR/cod2-silicon/releases).
2. Download `CoD2-Silicon-0.1.0-macos-arm64.zip` and `SHA256SUMS` into the same
   folder. Verify the archive, then unzip it:

   ```sh
   shasum -a 256 -c SHA256SUMS
   ```

3. Move **CoD2 Silicon.app** to `/Applications` and open it.

The app is ad-hoc signed and **not notarized**. On macOS 15 and later, open it
once, then go to **System Settings → Privacy & Security → Open Anyway**.
Alternatively, after verifying your download:

```sh
xattr -dr com.apple.quarantine "/Applications/CoD2 Silicon.app"
```

### From source

```sh
git clone https://github.com/STiXzoOR/cod2-silicon && cd cod2-silicon && ./scripts/install.sh
```

The installer needs Swift 6 (Xcode 16 or later), the Xcode Command Line Tools and CMake and explains how to
get them. It downloads and builds the pinned SDL release itself (no Homebrew),
compiles the client and installs `~/Applications/CoD2 Silicon.app`. No original
game binary is needed to build. A locally built app is not quarantined, so
Gatekeeper does not block it.

## First run and playing

Open **CoD2 Silicon.app** to use the native SwiftUI launcher: Play, a server
browser with favorites and recents, settings, demos/screenshots and update checks.
It finds the game folder automatically or asks you
to choose the folder containing `main/`. If no CD key is stored, it asks for
yours and saves it in `~/.cod2/preferences`. Never share that file or your key.
It extracts the original shaders from your own Steam Mac copy when available,
otherwise continues with approximate shaders. Configs, logs, demos, screenshots
and the shader cache live in `~/Library/Application Support/CoD2 Silicon`.

Use **Play** to enter the original multiplayer menus, or join from the launcher
server browser. Quitting the game returns to the launcher; a game crash leaves
the launcher open with a link to its crash report. You can also open a `cod2x://`
server link (including while the game is running):

```sh
open -a "/Applications/CoD2 Silicon.app" 'cod2x://connect/127.0.0.1:28960'
```

For a direct CLI launch, run the app executable with `--play`. Append
`--exit-after-game` to close the launcher when the game quits, or pass trusted
engine arguments after `--`. Settings include 1080p–6K, the frame cap, vsync,
raw mouse, audio and native fullscreen Spaces. Metal options are marked as
upcoming until the native renderer is integrated.

Replace the address with your server. For a source install, use the app under
`~/Applications` instead. The default settings target 1080p fullscreen,
a 250 fps cap, vsync off and raw mouse input. The cap can be set anywhere from
0 (no cap) to 1000. Competitive CoD2x servers enforce their own 125–250 fps
caps.

## Known limitations

- No microphone/voice capture; no intro cinematics.
- No standalone native dedicated-server binary yet. Use the client executable
  with `+set dedicated 1 +map mp_toujane` and your normal data arguments.
- Frame-pacing hitches remain; active Game Mode and quiet-desktop display
  restoration still need acceptance testing.
- Some rendering and mod edge cases remain. A server restart during an HTTP
  download can stall it; reconnecting recovered in the tested case.
- This engine inherits security-sensitive networking, file parsing and memory
  behavior from an old reconstruction. It is not hardened. Use trusted content
  and servers; see [SECURITY.md](SECURITY.md).

## Verified results

The `0.1.0` port is playable, with further stability and frame-pacing work ahead.
Verified development results include:

- Online stock 1.3 (protocol 118) and CoD2x 1.4 (protocol 120) play: movement,
  firing, reloading, round restart and respawn; HTTP mod downloads also worked.
  The sessions were short and mostly on empty servers. [Online report](docs/macos-port/reports/WS14-online.md)
- CoD2x's 68-row compatibility inventory has **41 implemented**, **12 native
  equivalents**, **2 partial** and **13 excluded or server-owned** behaviors.
  This is implementation coverage, not 68 live acceptance tests. Match-service
  UI/backend validation remains partial; the auto-updater is excluded.
  [Compatibility report](docs/macos-port/reports/WS10-cod2x-full.md)
- With the original Mac shaders, three matched Toujane views have RGB mean
  absolute errors of **2.0050, 1.8726 and 0.5856 out of 255** against Windows.
  Full post-processing, shadows and combat FX parity are still unverified.
  [Rendering report](docs/macos-port/reports/WS15-render.md)
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
