# WS8 — real CoD2x under Wine

Date: 2026-10-03. Branch: `port/wine-baseline`. Worktree: `wine-baseline`.

## Result

The released **CoD2x 1.4.6.8** runs with the existing diagnostic CD 1.3 reference executable and the user's copied Steam assets. It renders the menu and an empty `mp_toujane` listen server. This is not yet a verified “play now at 333 fps” setup: OpenGL is slow, the tested Vulkan paths have failures, and the supplied Steam executable requires account authentication that this agent cannot provide.

The original Steam executable is preserved. With Valve's Windows Steam installed in the prefix and its own `Steam.dll` beside the game, it reports: **“You do not have a valid Steam subscription for this application [AppId=2630].”** The user must sign into Windows Steam in this prefix with the account owning CoD2. A CD key is a separate online requirement, not a substitute for that Steam entitlement.

## Scope and machine

- Read all of `docs/macos-port/PLAN.md`; stayed on the assigned branch and did not access sibling worktrees.
- Changed only the launcher and this report. No native sources, headers, CMake files, remotes, PRs or issues changed. No system packages installed. `COD2_X64=OFF` inputs are untouched; no native build was needed to test these two additions.
- Apple M6 Mac mini, 12 CPU / 12 GPU cores, 24 GB, macOS 27.0.1 (26A434), Rosetta 2 already installed.
- PA27JCV display: physical 6016×3384, macOS logical 3008×1692, 60 Hz. Other user apps and other agents remained active. These measurements describe this session, not an isolated hardware ceiling.
- All installed apps, Wine prefixes, copied game files, DLLs, demos, screenshots and raw logs are outside git, under `$HOME/Library/Application Support/CoD2x-Wine` (about 12 GB at the time of testing). The source `$HOME/Games/CoD2` was read only.

## Installed and tested setups

| Setup | Versions / components | Result |
| --- | --- | --- |
| Highball, WineD3D OpenGL | Highball 0.10.3; `x64-sikarugir10.0_6-r19`, Wine 10.0; DX9 and DX7 game paths | Menu, map, screenshot and completed timedemos; slow. |
| Highball, WineD3D Vulkan | Same engine, bundled MoltenVK 1.4.1 | Fatal initialization: “Video card or driver doesn't support multitexture.” Other required fixed-function operations also absent. |
| Highball modern D3D9 DXVK | `d9vk-dxvk-macos-3.1-20260916` | With DXVK text HUD: MoltenVK “DrawIndex is not supported in MSL”, built-in pipeline failure. Disabling that HUD removes this error, but playback still times out at 150 s during map initialization. No valid fps. |
| Highball legacy D9VK | DXVK-Kegworks 1.10.4-async | Menu and eventually map; no completed short timedemo within 150 s. Retest with msync off and `r_smp_backend_allowed 0` did not finish either. A further no-DXVK-HUD retest also timed out. An attempted `DYLD_LIBRARY_PATH` switch to bundled CodeWeavers MoltenVK still logged version 1.4.1, so it is not evidence for a different MoltenVK backend. |
| Sikarugir D9VK component on the same Sikarugir engine | Official `d9vk-macOS-async-v1.10.3-20250511`, x32 DLL | Rendered Toujane after ~70 s map initialization; short timedemo did not finish within 150 s. Used the engine directly, not a separately configured Sikarugir GUI wrapper. |
| Plain Gcenx Wine 11 | `wine-devel-11.18-osx64`; separate new prefix | Wine boots. OpenGL is unavailable in this build. Both D9VK versions tested through the launcher (legacy 1.10.4 and modern 3.1) fail on unsupported external-memory buffer/image handle types. WineD3D Vulkan crashes after buffer creation fails (`vkCreateBufferView`, 0xc0000005). |

Highball's DXMT and D3DMetal selections route D3D9 through D9VK; they are not separate D3D9 backends. D3DMetal's license was not accepted; its files bundled in the runtime were not enabled. The Sikarugir GUI and a full wrapper were not installed because its Wine engine and official D9VK component were already exercised directly. See [Highball's renderer implementation](https://github.com/gauthierpiarrette/highball/blob/v0.10.3/Sources/HighballKit/Bottle.swift), [Sikarugir's renderer list](https://github.com/Sikarugir-App/Sikarugir/blob/main/README.md), and [Gcenx's build configuration](https://github.com/Gcenx/macOS_Wine_builds).

Modern and legacy DXVK initially carry Wine's builtin DLL marker. Copying them into the game folder with a native override alone fails with `c0000135`. The launcher clears **only the marker in its copied DXVK DLL**, matching Highball's `asNative` recipe. It leaves the DXVK HUD off: [DXVK 3.1's HUD text vertex shader](https://github.com/doitsujin/dxvk/blob/v3.1/src/dxvk/hud/shaders/hud_text_vert.vert) uses `gl_DrawID`. This explains the first shader failure; disabling it removes that error but does not cure the later timeout. It never patches a game executable or the pinned engine DLL.

## Performance method

The game writes `main/console_mp.log` with `+set logfile 2`; it does not create `qconsole.log`. CoD2's command is `timedemo <demo-name>`, rather than a `timedemo 1` dvar followed by `demo`.

Recorded an empty, stationary spectator view of `mp_toujane` using `record ws8_baseline` and `stoprecord`, after map initialization. The complete file has 1,001 packets and was properly closed. For the matrix, retained its first 201 complete packets, unchanged, and appended the standard `-1, -1` EOF record. This shorter local clip, `ws8_201packets.dm_1`, produces 427 timedemo frames. The original and clip remain outside the repo. No bots, player combat or moving viewpoint were measured.

Each run uses the same clip, a fresh game process, explicit renderer, resolution, fullscreen flag and `com_maxfps` (333 or 0), `cg_drawFPS 1`, vsync off, `m_rinput 2`, and `MTL_HUD_ENABLED=1`. msync is explicitly enabled for the matrix. Before playback, a delayed CFG disables CoD2x's watchdog after its initialization, then runs `timedemo ws8_201packets`. A builtin game JPEG is requested during the early part of playback. Readback/first-use compilation can therefore worsen the early frame times. The engine CSV is buffered and only accepted after its completed fps banner appears.

Average = engine's completed timedemo banner. Minimum = lowest instantaneous fps from the CSV (1000 / largest frame time), **not** a one-second minimum. Frame-time percentiles below use CSV frames after the first 100; the all-frame maximum is also reported. CSV timing has millisecond resolution. The initial CSV sample is excluded by the engine; all-frame and banner averages differ slightly. A 333-fps target needs about 3 ms per frame.

Metal HUD initializes on the Vulkan paths; `metalperftrace listen --pid <game-pid> --json` recorded the modern no-DXVK-HUD trial. That trial did not complete, so its HUD trace is not a completed timedemo comparison. Its two sampled intervals had zero presented frames; skipped-frame counts must not be labeled as game FPS. OpenGL's completed-run timing comes from the game CSV, not an independently verified Metal HUD trace. Do not present these as two independent measurements. CoD2x stays loaded during playback, but the console switches its server compatibility mode to legacy CoD2 1.3 for this demo; timedemo alone does not prove full 1.4 online parity.

| WineD3D game path | Resolution | Mode | Maxfps | Average fps | Minimum fps (all) | p50 / p95 / p99 ms (after frame 100) | Max ms (all) |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: |
| GL/DX7 | 1920x1080 | window | 333 | 16.5 | 1.64 | 48 / 84 / 98 | 610 |
| GL/DX7 | 1920x1080 | window | 0 | 18.5 | 2.51 | 47 / 56 / 63 | 398 |
| GL/DX7 | 1920x1080 | fullscreen† | 333 | 16.4 | 3.27 | 66 / 71 / 72 | 306 |
| GL/DX7 | 1920x1080 | fullscreen† | 0 | 17.1 | 3.16 | 64 / 71 / 73 | 316 |
| GL/DX7 | 2560x1440 | window | 333 | 21.9 | 3.08 | 45 / 50 / 55 | 325 |
| GL/DX7 | 2560x1440 | window | 0 | 20.7 | 3.32 | 45.5 / 64 / 72 | 301 |
| GL/DX7 | 2560x1440 | fullscreen† | 333 | 24.3 | 3.91 | 40 / 46 / 49 | 256 |
| GL/DX7 | 2560x1440 | fullscreen† | 0 | 24.6 | 3.91 | 40 / 43 / 46 | 256 |
| GL/DX9 | 1920x1080 | window | 333 | 6.8 | 0.55 | 133 / 183 / 220 | 1804 |
| GL/DX9 | 1920x1080 | window | 0 | 10.2 | 1.80 | 92 / 117 / 186 | 556 |
| GL/DX9 | 1920x1080 | fullscreen† | 333 | 11.7 | 1.86 | 84 / 97 / 104 | 539 |
| GL/DX9 | 1920x1080 | fullscreen† | 0 | 6.7 | 1.88 | 187.5 / 230 / 254 | 531 |
| GL/DX9 | 2560x1440 | window | 333 | 5.8 | 1.05 | 185.5 / 234 / 272 | 956 |
| GL/DX9 | 2560x1440 | window | 0 | 5.6 | 1.58 | 204 / 293 / 416 | 634 |
| GL/DX9 | 2560x1440 | fullscreen† | 333 | 6.0 | 1.02 | 185 / 230 / 271 | 981 |
| GL/DX9 | 2560x1440 | fullscreen† | 0 | 9.7 | 1.77 | 93 / 151 / 156 | 564 |

All 16 completed rows have matching renderer initialization dimensions and builtin JPEG dimensions, checked with `sips -g pixelWidth -g pixelHeight`. **† Visual caveat: all eight fullscreen images are black; all eight windowed images contain rendered content.** Fullscreen numbers are completed engine timing, not validated visible gameplay. Pixel checks are saved in `evidence/screenshot-pixel-check.txt`. No run held 333 fps. Single trials and background-load variation prevent a causal claim that 1440p is intrinsically faster than 1080p.

| Other renderer | Tested resolution / mode / cap | Average / minimum / frame times | Remaining combinations |
| --- | --- | --- | --- |
| Highball WineD3D Vulkan | 1080p / window / 333 | N/A — required multitexture capability absent | 1080p uncapped/fullscreen and all 1440p combinations not run after initialization failure. |
| Highball modern DXVK 3.1 | 1080p / window / 333, text HUD on and off | N/A — shader error with text HUD; 150 s timeout without it | Other requested combinations not run because no valid initial timedemo. |
| Highball legacy D9VK 1.10.4 | 1080p / window / 333, msync on/off | N/A — 150 s timeout | Other combinations not run because no valid initial timedemo. |
| Sikarugir D9VK 1.10.3 | 1080p / window / 333 | N/A — 150 s timeout | Other combinations not run because no valid initial timedemo. |
| Gcenx Wine 11 + D9VK 1.10.4 / DXVK 3.1 | 1080p / window / 333 | N/A — buffer/image external-memory feature failure | Other combinations not run after initialization failure. |
| Gcenx Wine 11 + WineD3D Vulkan | 1080p / window / 333 | N/A — Vulkan buffer failure and 0xc0000005 | Other combinations not run after initialization failure. |
| Gcenx Wine 11 + WineD3D OpenGL | Not run | N/A — build has no OpenGL | No GL modes available. |

Failed backends have no measured average/minimum or stability claim. The renderer/mode/cap combinations above marked not run are explicitly unmeasured, not presumed to share a numeric result.

The fastest **visually verified** matrix setting was GL/DX7 at 2560×1440 in an ordinary window: 21.9 fps with maxfps 333 and 20.7 uncapped. The launcher adopts this setting. Fullscreen produced faster timing in some runs, but its captured game frames were completely black, so it is not the recommended configuration. This choice is provisional: runs were sequential with uncontrolled background load, and the smaller 1080p mode was unexpectedly slower. Neither result approaches 333. A separate default-configuration test (`--sync none`, otherwise GL/DX7/1440p/window/333, HUD on) completed 427 frames in 27.2 s: **15.7 fps**; minimum 2.43 fps, p50/p95/p99 62/75/83 ms after frame 100, all-frame maximum 411 ms. An earlier fullscreen synchronization check returned 15.8 fps but its screenshot was black, so it was rejected as a visible-play recommendation. The launcher defaults to none to match Windows Steam's tested sign-in setup. For the fastest diagnostic configuration measured, explicitly select `--sync msync`. This session does not establish whether the difference is entirely due to synchronization, because background load was uncontrolled.

A separate, earlier full-demo GL/DX9/1080p/window/333 run completed **2,176 frames in 204.8 s: 10.6 fps**. It is not directly comparable to the shorter matrix or a moving multiplayer round. An early DX7 full-demo run quit before completion and was rejected. D9VK timeouts have no valid average/minimum/percentile; their buffered CSVs were empty when terminated.

## Features, input, audio and network

| Item | Verified result and limits |
| --- | --- |
| Real released patch | `CoD2x 1.4.6.8 loaded` in the console, release proxy DLL hash checked, `iw_CoD2x_01.iwd` extracted in the prefix. No reimplementation used. |
| Menu and local map | Menu initialization and `CL_InitCGame` complete; builtin game JPEGs show the empty Toujane spectator view. Local CoD2x compatibility switches on for server version 1.4.6.x. |
| HWID / WMI | The game produced a registry `HWID` of 32 characters, with `HWID_SOURCE` present. No WMI package or winetricks verb was needed on this engine. Actual identifier is not reproduced here (only presence and length checked). Server acceptance remains unverified. |
| Integrated input | `m_rinput 2` and “Registered raw input device (main window method)” confirmed; no physical mouse movement performed, measured polling-rate dvar remained 0. |
| macOS acceleration | The task's earlier audit says `m_rinput` does not bypass macOS acceleration under Wine. CoD2x uses Windows `GetRawInputData`; Wine 10's Mac driver derives relative mouse movement from `NSEvent.deltaX/deltaY`. This proves Windows raw-input registration, not raw hardware counts on macOS. No global acceleration setting was changed and no physical speed/distance test was possible. See [Wine 10 Mac input source](https://github.com/wine-mirror/wine/blob/wine-10.0/dlls/winemac.drv/cocoa_app.m). |
| FPS settings | Console confirms `cg_drawFPS 1`, `com_maxfps 333` (or 0); game screenshots and completed CSVs provide rendering/timing evidence. Merely setting 333 did not achieve it. |
| Window / fullscreen | Renderer logs and `sips` dimensions confirm 1920×1080 and 2560×1440 for all 16 completed GL runs. Fullscreen requests initialize, but all eight fullscreen JPEGs are black. The eight ordinary-window JPEGs show rendered game content. This could be presentation or readback failure; unavailable external capture prevents distinguishing them. Fullscreen is not recommended. |
| Borderless | CoD2x's `window_doBorderless` requires the game dimensions to exactly match the monitor dimensions. Neither requested resolution matches this Mac's logical 3008×1692 desktop; those `r_fullscreen 0` runs are ordinary windows. A final local-map run at 3008×1692 initializes: “Attempting 3008 x 1692 borderless window at (0, 0)” and `CL_InitCGame: 2.45 seconds`, but its game-only 3008×1692 JPEG is entirely black. Borderless content/presentation is unresolved and not recommended; fps was not benchmarked. See [CoD2x window code](https://github.com/callofduty2x/CoD2x/blob/v1.4.6.8/src/mss32/window.cpp). |
| Freeze watchdog | Early `+set com_freezeWatch 0` can be reset when CoD2x registers this dvar. A delayed startup CFG applies it again. Watchdog disabled for completed measurements; this does not fix renderer stalls. |
| Audio | DirectSound initializes 44 kHz, 16-bit stereo successfully. Console also reports missing `sound/misc/beep.wav`. Audible music/effects/output continuity were not verified; do not label sound fully working. |
| Public network | UDP status replies obtained from a CoD2x 1.4.6.3 / protocol 120 server and a stock 1.3 / protocol 118 control. This proves reachability, not successful game authentication or a played round. In-game result follows below. |
| Complete feature parity | Not verified. Input motion, audible sound, borderless presentation, server HWID validation, competitive settings, anti-cheat and a full public round still need interactive/authenticated testing. |

Attempted in-game `connect 5.130.157.156:28961` to `COD2x_HD_serv` (UDP status: protocol 120, version 1.4.6.3, no password, reachable in 129 ms; listed by the [CoD2x master](https://master.cod2x.me)). The client refused before joining:

> Key Code is not valid.

Its next line directs the user to **Multiplayer Options → Enter Key Code** before connecting again (`SV_Shutdown(EXE_ERR_INVALID_CD_KEY)`). The user must provide the legitimate CoD2 multiplayer CD key there. No key was guessed/generated and no key check bypassed. The Steam CD key was not available to this agent. A successful connection, server-side HWID acceptance and public round therefore remain blocked.

The stock control `37.44.215.192:28960` replied to UDP `getstatus` (protocol 118, version 1.3, 60 ms); it was not joined because the same local key prerequisite applies. Official testing endpoint `88.198.58.188:27397` timed out. Saved responses are `evidence/public-server-status.json`; the actual game rejection is `evidence/network-cod2x14-console.log`.

Final local validation used:

```sh
./tools/wine/play-cod2x.sh --reference --local --sync msync --resolution 3008x1692 --windowed -- +exec ws8-final.cfg
```

The resulting JPEG is black, so these console checks verify initialization/settings only, not successful borderless display. The prefix-only CFG waits 300 command-buffer ticks, queries `com_freezeWatch`, `com_maxfps`, `cg_drawFPS`, `m_rinput`, `r_fullscreen`, `r_mode`, and writes `screenshotJpeg ws8_final_borderless`. Saved `evidence/final-borderless-console.log` confirms respectively **0, 333, Simple, 2, 0, 3008×1692**, plus the CoD2x banner and completed map load. External window capture remains unavailable.

The final default-settings local run (`./tools/wine/play-cod2x.sh --reference --local -- +exec ws8-final-window.cfg`) confirms 2560×1440 ordinary-window initialization, `CoD2x 1.4.6.8 loaded`, `CL_InitCGame: 1.73 seconds`, and the same settings with `r_mode 2560x1440`. Its JPEG shows the Toujane world behind the initial Deathmatch / Click to Continue menu, unlike the borderless/fullscreen captures. The FPS counter is visible in the windowed timedemo shots; this initial local menu needs a physical click to continue. Evidence: `final-window-console.log` and `main/screenshots/ws8_final_window.jpg`. This uses the launcher's default synchronization (none).

## Launcher and reproduction

`tools/wine/play-cod2x.sh` uses the private installed prefix. Defaults are Highball's WineD3D OpenGL, DX7, 2560×1440 windowed, maxfps 333, vsync off, integrated `m_rinput 2`, and delayed `com_freezeWatch 0`. Synchronization defaults to none so the Windows Steam sign-in client and game use the same server settings. DX9 remains selectable with `--dx9`; every renderer trial can be reproduced with explicit flags.

```sh
# One-time user sign-in is still needed in the owning Steam account:
./tools/wine/play-cod2x.sh --steam
# Supplied, unchanged Steam executable:
./tools/wine/play-cod2x.sh
# Diagnostic executable: actual CoD2x DLLs and copied Steam assets, local map:
./tools/wine/play-cod2x.sh --reference --local --sync msync
# Comparison settings:
./tools/wine/play-cod2x.sh --reference --renderer gl --dx9 --fps 0 --resolution 1920x1080 --windowed
./tools/wine/play-cod2x.sh --reference --backend wine11 --renderer dxvk --dx9
# Review without starting Wine or changing prefix files:
./tools/wine/play-cod2x.sh --reference --local --dry-run
```

The script writes only the selected prefix: its renderer registry value, a small delayed startup CFG, and a copied native DXVK DLL when requested. Wine/game configs, logs and screenshots also stay in the prefix. It does not install dependencies or copy the licensed game. Environment overrides are documented in `--help`. Keep one game/benchmark process active at a time; switching a renderer during a live game is unsupported.

Fresh setup commands (the task has already run these; do not rerun the copy over an existing personalized prefix):

```sh
ROOT="$HOME/Library/Application Support/CoD2x-Wine"
mkdir -p "$ROOT/downloads"
curl -fL 'https://github.com/gauthierpiarrette/highball/archive/refs/tags/v0.10.3.tar.gz' -o "$ROOT/downloads/highball-0.10.3.tar.gz"
tar -xzf "$ROOT/downloads/highball-0.10.3.tar.gz" -C "$ROOT/downloads"
swift build -c release --product highball --package-path "$ROOT/downloads/highball-0.10.3"
HB="$ROOT/downloads/highball-0.10.3/.build/release/highball"
export HIGHBALL_HOME="$ROOT/highball"
"$HB" engine install "$ROOT/downloads/highball-0.10.3/spike/engine-manifest.json"
"$HB" bottle create cod2x --renderer wined3d
"$HB" bottle set cod2x keepfiles on
"$HB" bottle set cod2x dlloverrides 'mss32=n,b'
PREFIX="$ROOT/highball/bottles/cod2x"
GAME="$PREFIX/drive_c/Games/CoD2"
ditto "$HOME/Games/CoD2" "$GAME"
gh release download v1.4.6.8 -R callofduty2x/CoD2x -p '*windows.zip' -D "$ROOT/downloads"
unzip -o "$ROOT/downloads/CoD2x_1.4.6.8_windows.zip" mss32.dll mss32_original.dll -d "$GAME"
# Separate diagnostic executable, without replacing the Steam executable:
cp "$HOME/Projects/cod2-native-refs/CoD2x/bin/windows/CoD2MP_s.exe" "$GAME/CoD2MP_reference_s.exe"
mkdir -p "$GAME/main/players/ws8"
printf 'ws8' > "$GAME/main/players/active.txt"
printf 'seta name "WS8 baseline"\nseta com_introPlayed "1"\n' > "$GAME/main/players/ws8/config_mp.cfg"
# Windows Steam, from Valve, inside this prefix only:
curl -fL 'https://cdn.akamai.steamstatic.com/client/installer/SteamSetup.exe' -o "$ROOT/downloads/SteamSetup.exe"
"$HB" bottle set cod2x sync none
"$HB" run cod2x "$ROOT/downloads/SteamSetup.exe" --verbose -- /S
./tools/wine/play-cod2x.sh --steam
# After the Steam client has updated:
cp "$PREFIX/drive_c/Program Files (x86)/Steam/Steam.dll" "$GAME/Steam.dll"
```

Other runtime trials:

```sh
curl -fL 'https://github.com/Gcenx/macOS_Wine_builds/releases/download/11.18/wine-devel-11.18-osx64.tar.xz' -o "$ROOT/downloads/wine-devel-11.18-osx64.tar.xz"
tar -xf "$ROOT/downloads/wine-devel-11.18-osx64.tar.xz" -C "$ROOT/downloads"
WINE11="$ROOT/downloads/Wine Devel.app/Contents/Resources/wine/bin/wine"
WINEPREFIX="$ROOT/gcenx-prefix" "$WINE11" wineboot -u
ditto "$GAME" "$ROOT/gcenx-prefix/drive_c/Games/CoD2"
./tools/wine/play-cod2x.sh --reference --backend wine11 --renderer d9vk --dx9 --windowed --resolution 1920x1080
# Sikarugir's separate official legacy D9VK DLL on Highball's Sikarugir engine:
gh release download v1.10.3-20250511 -R Sikarugir-App/d9vk -p '*.tar.gz' -D "$ROOT/downloads"
tar -xzf "$ROOT/downloads/d9vk-macOS-async-v1.10.3-20250511.tar.gz" -C "$ROOT/downloads"
COD2_WINE_D3D9="$ROOT/downloads/d9vk-macOS-async-v1.10.3-20250511/x32/d3d9.dll" ./tools/wine/play-cod2x.sh --reference --renderer d9vk --dx9 --windowed --resolution 1920x1080
```

The Steam client bootstrap updated, exited with 42, and needed a second start before its sign-in window appeared. Authentication still needs the user. The vendor DLL resolved “Failed to find Steam”; running the client resolved “Problem starting up Steam”; the remaining subscription error is recorded above. The [Steam community guide](https://steamcommunity.com/sharedfiles/filedetails/?id=160163309) describes the vendor-DLL remedy.

For a new recorded benchmark, use `--reference --local`, then the game console commands `record ws8_baseline`, wait through the desired scene, and `stoprecord`. Restart the game before `timedemo ws8_baseline`: a listen server cannot play a demo. The matrix's exact local harness is `$ROOT/evidence/bench.py`:

```sh
python3 "$ROOT/evidence/bench.py" highball gl 1920x1080 window 333 dx7
# JSON, console log and CSV are copied to $ROOT/evidence automatically.
```

The diagnostic executable, demo and this harness are deliberately machine-local; obtain the demo from the existing evidence for a like-for-like native comparison. Do not substitute an unrelated demo and compare averages.

## Evidence and provenance

Raw evidence is local at `$HOME/Library/Application Support/CoD2x-Wine/evidence`. It contains completed CSVs, result JSONs with exact commands, saved `console_mp.log` copies, Wine and renderer logs, Steam error-dialog text, public-server replies and the local `bench.py` measurement harness. Ignore the name of the `-cxmoltenvk` trial as a runtime claim: its log still proves MoltenVK 1.4.1 was used. `highball-<renderer>-<resolution>-<mode>-<cap>-<dx>-result.json` records every matrix run. Screenshots are under each prefix's `drive_c/Games/CoD2/main/screenshots/`.

Requested `screencapture -x -o -l <game-window-id>` repeatedly failed with “could not create image from window”; Computer Use reported pending Accessibility and Screen Recording permissions. No desktop screenshot was taken. Used the game's own `screenshotJpeg` for game-only visual evidence instead and inspected the Toujane images. This proves rendered content, but not window decorations or fullscreen presentation. No `.app` wrapper/export was produced; the shell launcher is the delivered entry point.

| File | SHA-256 |
| --- | --- |
| Supplied Steam `CoD2MP_s.exe` (source and prefix) | `41124c4620c1e96b8b908611a4135e3ccef4f675f56338d7f85bea6684afdf0e` |
| Existing reference `CoD2MP_s.exe`, copied as `CoD2MP_reference_s.exe` | `8f482e647b250babe098c6e3deb10129f9a935d013e852c99689cf8dff0d12cc` |
| Release `mss32.dll` | `b994ae0ee542490bb16f28a217c4914da7fdea413232d4e43d1e28ff46917cac` |
| Steam original / release `mss32_original.dll` | `7855b8fbae917cb8449f2d4361ab61b5ecec4df0a11130d797cb0aa99b4260ea` |
| Steam and reference `gfx_d3d_mp_x86_s.dll` | `243505f56ec0f86122fc09b3a6c18f0daf49a00abb2d969147f595991584a6b2` |
| CoD2x Windows release ZIP | `c33f52037e67eaa948746a93967d27d8c70ab58623abab48d07803600a6a8555` |
| Official Gcenx Wine 11.18 archive | `aa0ea4c82e636ae7bca2076387cb0a5affa26509ad13f119ecd0d62bd7ba6f82` |
| Official Sikarugir D9VK 1.10.3 archive | `13a088e96c90501705c26326044ccc57e2b03adda3ed0dd7061e89249d4c176e` |

The [CoD2x README](https://github.com/callofduty2x/CoD2x) identifies its `bin/windows` files as original CD binaries patched to 1.3. Used that **existing read-only reference**, without downloading replacement Activision binaries or modifying DRM. Its provenance was not independently checked against an Activision patch installer. It is a diagnostic alternative, not proof that the user's PECompact2 Steam executable ran. [Issue #18](https://github.com/callofduty2x/CoD2x/issues/18) confirms Steam copies can work with CoD2x; it does not demonstrate macOS Wine performance or waive Steam authentication.

## Installs and removal

| Install / change | Location and removal |
| --- | --- |
| Highball 0.10.3 app ZIP | Extracted under `$ROOT/downloads/Highball.app`; launched for an initial CLI probe, then terminated; GUI automation was unavailable. Never installed in `/Applications`. Remove the task folder. |
| Highball 0.10.3 source + locally compiled CLI | `$ROOT/downloads/highball-0.10.3`, including SwiftPM checkout/build files. Dependencies: swift-argument-parser 1.8.2, Sparkle 2.10.0. Remove the task folder. SwiftPM may retain shared download caches; no shared caches were deleted. |
| Highball engine and manifest components | `$ROOT/highball/engines/x64-sikarugir10.0_6-r19`; Wine, Sikarugir runtime/frameworks, DXMT, legacy/modern D9VK, patched MoltenVK, focus/msync/audio libraries, timestamp shim and pinned winetricks script downloaded by the manifest. No winetricks verbs run. Runtime contains bundled D3DMetal files, but no D3DMetal license accepted or backend used. Remove the task folder. |
| Private Highball bottle | `$ROOT/highball/bottles/cod2x`; Windows registry/profile, copied game, CoD2x DLLs and logs. `keepfiles on` makes Documents private rather than linking host Documents. Remove the task folder. |
| Valve Windows Steam | `$PREFIX/drive_c/Program Files (x86)/Steam`, installed silently from Valve's installer; ~242 MB client update downloaded. Copied that client's own `Steam.dll` beside the game. No account signed in. Remove the prefix/task folder. |
| Gcenx Wine Devel 11.18 app | `$ROOT/downloads/Wine Devel.app`; not in `/Applications`. Its separate prefix is `$ROOT/gcenx-prefix`. Remove the task folder. |
| Official Sikarugir D9VK 1.10.3 archive/DLLs | `$ROOT/downloads/d9vk-macOS-async-v1.10.3-20250511`; no GUI wrapper installed. Remove the task folder. |
| CoD2x release and licensed data copies | `$ROOT/downloads/CoD2x_1.4.6.8_windows.zip` and both prefixes' `drive_c/Games/CoD2`. Remove the task folder; source `$HOME/Games/CoD2` is unaffected. |
| Temporary power assertion | `caffeinate -u -d` woke the display; `caffeinate -d -t 1800` kept it awake while measuring. Assertions end when that process exits; no global power configuration changed. |

`ROOT="$HOME/Library/Application Support/CoD2x-Wine"`. To undo, first stop this task's two prefixes with their matching `wineserver -k` and `WINEPREFIX`, close the task's Highball app if running, then remove **that exact task folder**. This also deletes the local evidence and copied profiles/demos, so save any wanted evidence first. No `brew install`, `brew upgrade`, system packages, Rosetta installation, global Wine prefix, launch agent or global pointer/display/power setting was added.

Python's Pillow module was absent when checking image dimensions. It was not installed; macOS `sips` provided those checks. UI permissions were unavailable, as described above.

## Checks and commands

```sh
bash -n tools/wine/play-cod2x.sh
shellcheck tools/wine/play-cod2x.sh
./tools/wine/play-cod2x.sh --help
./tools/wine/play-cod2x.sh --reference --local --dry-run
# Run on the installed private prefix; verified menu and Toujane with release DLLs:
./tools/wine/play-cod2x.sh --reference --local --sync msync
# Confirm the saved console evidence without private HWID or key values:
rg 'CoD2x 1.4.6.8 loaded|CL_InitCGame|Registered raw input|frames,' "$ROOT/evidence/"*-console.log
```

Syntax and shellcheck passed. The help and dry run passed; invalid fps and Wine 11/OpenGL combinations were correctly rejected. Source/prefix Steam executable SHA-256s still match, and the original Miles DLL remains identical to the release's `mss32_original.dll`. Sixteen short matrix trials and the windowed synchronization-default check produced complete fps banners/CSVs; fullscreen visibility remains unresolved as detailed above. This verifies the isolated launcher and recorded limitations, not the unachieved 333-fps/public-play goal.

## Merge and remaining work

Launcher commits: `4fe1250` (initial launcher), `66d61ab` (use visually verified windowed mode). The report is a separate final documentation commit.

Only `tools/wine/play-cod2x.sh` (executable) and this report should be merged. No shared build/header files conflict with native workstreams, and no binaries, game data, raw logs or screenshots are in git. The installed prefix and evidence are machine-local and do not travel with the commit.

Before claiming the user's requested outcome: authenticate the unchanged Steam executable; complete a real public 1.4 server connection and a round with the required legitimate multiplayer key; verify audible sound and physical mouse behavior; obtain permitted window captures/HUD logs; and find a renderer/runtime that sustains 333 fps. Resolve fullscreen/borderless presentation as well. The measured OpenGL fallback does not meet that performance target. A full native-port parity test must also use moving/player-heavy scenes and the same documented settings.
