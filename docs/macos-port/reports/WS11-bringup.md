# WS11 — native menu, listen server, and local play

Date: 2026-10-03. Worktree: `~/Projects/cod2-native-wt/bringup`.
Branch: `port/bringup`; starting commit: `391870a`.

## Result

The arm64 client renders the main menu, loads `mp_toujane` in a local deathmatch
listen server, joins Allies with an Enfield, predicts movement, turns, jumps,
fires, rechambers, reloads, and aims down sights. The final gameplay probes
reached `CS_ACTIVE` and completed their input sequences without a crash or
script error. The port is **not finished**: quitting a listen server after
joining still hangs in script cleanup, and rendering/effect/HUD defects remain.

The requested ancillary fixes are implemented: real memory/CPU detection,
a unified-memory texture budget, native display resolutions including
1280x720, home directory creation, JPEG screenshots, immediate completion of
unsupported cinematics, and the observed channel-mixer parse/marshal failure.

No sibling worktree was read or written. No packages were installed, remotes
changed, branches pushed, or PRs/issues opened. Only recorded child processes
from this workstream were terminated. Licensed data stayed read-only at
`~/Games/CoD2`; no data, binaries, screenshots, or decompiler dumps
were added to git. Temporary diagnostics were removed before commits.

## Build and verification

From this worktree, the final native build succeeds:

```sh
cmake -S . -B build-macos -DCOD2_X64=ON
cmake --build build-macos -j12 --target cod2_macos
```

Final build log: `~/Library/Application Support/CoD2-native-ws11/evidence/ws11-final-build.log`.
Existing deprecation, duplicate-library, and linker alignment warnings remain.

The following checks passed:

```sh
cmake -S tests/platform -B build-macos/platform-tests -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build build-macos/platform-tests -j12
ctest --test-dir build-macos/platform-tests --output-on-failure
sh tests/lp64/renderer/run.sh
python3 tests/lp64/script/run.py
git diff --check
```

- Platform CTest: **21/21 passed**, including new sound-channel, JPEG, hardware,
  cgame layout, random-number, client FX, slide movement, FX-call, and script
  notify fixtures. Portable adapters use ASan/UBSan; the window smoke target is
  created before the sanitizer options and is not instrumented.
- Renderer: **7 fixtures passed**, including a zero-reflection-table shader
  with both a sampler and `constant.materialColor` argument.
- Script: **6 source fixtures, 7 compiler checks, and 15 VM semantics cases
  passed**. This does not cover a live player's complete shutdown state.
- Shared production source comparison against `391870a`: **165 comparisons,
  zero mismatches** for five inactive-port configurations (Linux i386, Apple
  i386, MinGW i386, wasm, clang i386). The checker omits includes and normalizes
  whitespace. Native-only `src/platform` adapters are excluded because they
  are not selected by legacy builds. The exact driver is preserved outside
  git at `~/Library/Application Support/CoD2-native-ws11/evidence/ws11-legacy-guards.py`;
  run it from this worktree with `--base 391870a`.

The source comparison proves guard discipline, **not byte-for-byte binary
parity**. A working 32-bit build/run environment was not available on this
Mac, so actual i386 binaries were not compared. `CMakeLists.txt` and
`src/headers/*` have no WS11 changes. The OFF source paths retain their original
expressions, allocations, prototypes, and constants.

## Runtime reproduction and evidence

Runtime files live in the private home:
`~/Library/Application Support/CoD2-native-ws11`.
The evidence directory below is abbreviated **E**:
`~/Library/Application Support/CoD2-native-ws11/evidence`.
All JPEGs were produced by the game through `screenshotJPEG`; no screen capture
permission was required.

Launch flags used for local play:

```sh
./build-macos/cod2_macos \
  +set fs_basepath "$HOME/Games/CoD2" \
  +set fs_homepath "$HOME/Library/Application Support/CoD2-native-ws11" \
  +set r_fullscreen 0 +set r_mode 1280x720 +set developer 1 \
  +set com_introPlayed 1 +set com_maxfps 333 +set cg_drawfps 1 \
  +exec ws11-map-only.cfg
```

The external `raw/ws11-map-only.cfg` contains only:

```text
set dedicated 0
set sv_pure 0
set g_gametype dm
set sv_maxclients 1
map mp_toujane
```

Do not put screenshot/quit commands immediately after `map` in this cfg:
command processing can reach them during `CG_Init`, before local play is ready.
The external driver waits for `Going from CS_PRIMED to CS_ACTIVE`, queries
`sv_serverId` and `configstrings`, then sends the stock menu responses on stdin.
For these runs the queried server ID was 16, and menu indices minus 0x4de were
4 (`serverinfo_dm`), 1 (`team_britishgerman`), and 2 (`weapon_british`):

```text
cmd mr 16 4 close
cmd mr 16 1 allies
cmd mr 16 2 enfield_mp
```

The drivers are preserved at `E/ws11-play-final.py` and `E/ws11-play-tour.py`.
They use this worktree's absolute binary path and the private home above.
Reproduce from this worktree:

```sh
python3 "$HOME/Library/Application Support/CoD2-native-ws11/evidence/ws11-play-final.py" repeat-play
python3 "$HOME/Library/Application Support/CoD2-native-ws11/evidence/ws11-play-tour.py" repeat-tour
```

Run them sequentially. They operate only their own child PID. Their forced
termination after `quit` is a **failed shutdown check**, not a successful exit.
Final runtime logs are `E/ws11-final-play.log` and `E/ws11-final-tour.log`.
Neither has `CRASH REPORT`, script runtime errors, weapon assertions, unknown
input commands, `color_channel_mixer.tech` errors, or `32->64 marshal failed`.
They do contain the known missing-beep and emitter warnings listed below.

The client binary's dedicated path also passed a final regression: start with
the same fs_basepath/fs_homepath, `+set dedicated 1 +set developer 1
+set com_maxfps 333 +map mp_toujane`, remain alive for 45 seconds, then send
`quit` on stdin. It exited **0**, with no crash or script runtime error.
Reproduce with `python3 "$HOME/Library/Application Support/CoD2-native-ws11/evidence/ws11-dedicated-probe.py"`;
log: `E/ws11-final-dedicated.log`.

Screenshots inspected after copying to unique filenames:

| Evidence | What it shows |
| --- | --- |
| `E/final-menu.jpg` | Complete 1024x768 main menu, after explicitly setting `com_introPlayed 0` and running `cinematic atvi`; clean exit 0. |
| `E/menu-720p-fixed.jpg` | Complete 1280x720 main menu after `vid_restart`; viewport and screenshot dimensions agree. |
| `E/final-spawn.jpg` | Enfield in the loaded Toujane map, ammunition `10\|60`. |
| `E/final-fired.jpg` | Changed player view after movement and five shots, ammunition `5\|60`. |
| `E/final-play.jpg` | Aim-down-sights view after jumping/reloading, ammunition `10\|55`. |
| `E/final-tour.jpg` | Turned and moved through the Toujane courtyard; geometry, textured buildings, rifle, and HUD. |

Menu reproduction uses `raw/ws11-final-menu.cfg`: set intro flag 0, run
`cinematic atvi`, wait 120 frames, `screenshotJPEG ws11-final-menu`, wait 120
frames, quit. Command:

```sh
python3 "$HOME/Library/Application Support/CoD2-native-ws11/evidence/ws11-run.py" repeat-menu +exec ws11-final-menu.cfg
```

Hardware logs now report **24576 MB RAM**, **4.79 GHz maximum CPU frequency**,
**1024 MB video/available texture memory**, and **picmip 0**. RAM comes from
`hw.memsize`. M6 did not expose `hw.cpufrequency_max`; the fallback reads the
maximum kHz value from IOKit's `voltage-states1-sram`/`voltage-states5-sram`
tables under `IODeviceTree:/arm-io/pmgr-child` (with a `pmgr` fallback).
This is a hardware maximum, not a measured current clock.
The reported video memory is a policy budget: one eighth of unified RAM,
capped at 1 GiB, with bytes passed to the texture allocator and MB to the
hardware/autoconfigure interfaces. It is not dedicated physical VRAM.
The r_mode names combine common window sizes with all SDL-reported display
resolutions, deduplicated with storage that lasts for the process lifetime.

`main/players/default/config_mp.cfg`, `main/qconsole_mp.log`, and
`main/games_mp.log` are now written under the private home. The original logger
was retrying failed file opens; fixing directory creation stopped that behavior
without changing logger policy. The cinematic implementation deliberately ends
unsupported QuickTime playback immediately and marks the intro played; it does
not implement QuickTime or new RoQ playback.

## Reproduced failures and fixes

Locations below point to the final source's relevant fix or affected function.
Logs and macOS crash records are in E unless marked `/tmp`.

| Failure / root cause | Source and fix | Evidence / commit |
| --- | --- | --- |
| Silent exit immediately after `SCR_UpdateFrame`: SIGABRT from the stack protector. Menu script temporary was 672 bytes, but native `itemDef_t` is 856 bytes and `parent` ends at byte 704. | `src/PC/ui_mp/ui_shared_mp.c:10`, `:2251`; use native sizeof for all five script temporaries, keep 0x2a0 on OFF. | `ws11-abort.ips`, `ws11-menu-baseline.log`; **354b33b**. This was not an intentional `exit()` path. |
| Menu MP3 stream seek consumed an IWD through `FS_Read(NULL, skip)`; inflate could not advance, and the fallback opened a NULL filename, faulting in `FS_SanitizeFilename`. The Miles callback also wrote a 32-bit handle into an uninitialized native unsigned long. | `src/PC/universal/com_files.c:734`, `src/PC/win32/snd_driver.c:199`, `src/platform/macos_audio.c`; seek through a scratch buffer, widen an initialized fileHandle_t, reject NULL stream names. | `ws11-menu-after-ui.log`, `ws11-menu-stream-diag*.log`, `cod2_crash_86942.txt`; **187732a**. |
| Stream playback then dereferenced NULL `chaninfo[index].pAlias0`; `SND_SetChannelInfo` was still a no-op stub. | `src/PC/snd.c:720`, `src/stubs/link_stubs.c`; implement native channel state, entity-relative origin, aliases, flags, volume, pitch, and timing. | `ws11-menu-after-stream.log`; sound-channel ASan/UBSan fixture; **4f5c0d9**. |
| Screenshot exited with `JPEG parameter struct mismatch: library thinks size is 520, caller expects 372`. Reconstructed JPEG structs/constants did not match native libjpeg. | `src/PC/gfx_d3d/r_jpeg.c:5`, `src/platform/macos_jpeg.c:8`; native ImageIO encoding/decoding. | `ws11-menu-jpeg.log`, native JPEG roundtrip/dimension test; **87cbe97**. |
| First ImageIO screenshot crashed creating CF properties: local `CFNumberCreate` returned NULL; CFData access and CFRelease placeholders could also shadow SDK functions. | `src/stubs/corefoundation_stubs.c:14`, `src/stubs/link_stubs.c:141`, `src/stubs/carbon_stubs.c:154`; exclude placeholders from native SDK builds. | `ws11-menu-jpeg2.log`, `ws11-menu-jpeg3.log` exits 0; **87cbe97**. |
| Hardware probe SIGBUS jumped into the `IORegistryEntryCreateCFProperty` data placeholder. | `src/stubs/link_stubs.c:626`, `src/stubs/iokit_stubs.c`; use actual IOKit functions on native SDK builds. | `ws11-hardware.log`; **316f551**. |
| Hardware still said 0 MB textures / 128 MB RAM / 0.80 GHz and rendered at 640x480 despite a larger mode. Native dxGlobals device is not at legacy +8; caps compared MB against a byte constant; device init ignored the requested resolution. | `src/platform/macos_system.c:55`, `src/platform/macos_display.c:315`, `:409`, `src/PC/gfx_d3d/r_texturemem.c`, `r_caps.c`, `r_image.c`, `r_dvars.c`, `r_init.c`, `src/Mac/DirectX_9/CDirect3DDevice.c`; real hardware, unit-correct budget, high-memory native picmip, dynamic mode names, parsed size and actual viewport. | `ws11-settings-final.log`, hardware fixture and 720p JPEG; **316f551**. |
| Map error cleanup crashed in XAnim with an ASCII-looking tree pointer. `cg_entitiesArray` still allocated 548-byte entities; clears/strides and client animation loops used legacy sizes. | `src/blobs/bss.c:335`, `src/PC/cgame_mp/cg_main_mp.c:1158`, `:1182`; native typed 1024-entity storage/clears and sizeof(clientInfo_t) loops. Sound registration now writes typed native media arrays. | `ws11-map-baseline.log`, cgame ASan/UBSan fixture (centity 568, clientInfo 1232); **33be47c**, spelling-only OFF preservation **0f88ab8**. |
| GSC spawnpoint shuffle exhausted all 65533 script variables. Native flrand(-1,1) produced 272.563538 because `UInt32`/unsigned long were 64-bit and the LCG did not wrap at 32 bits. | `src/PC/universal/com_math.c:26`; explicit native uint32_t state and arithmetic, preserve OFF arithmetic. | `ws11-map-random.log`; bounded 10000-value random fixture and exact seed-1 sequence; **d52f76c**. No script allocator limit was raised. |
| Map client FX loading trapped in libmalloc with corrupt free-list contents. A primitive allocated 0x2a4 bytes, channels used legacy offsets, and material handle lists allocated/copied four-byte slots. | `src/PC/EffectsCore/FxScheduler_load_obj.c:96`, `:197`, `:279`, `src/PC/EffectsCore/FxTemplate.c:372`; native primitive/channel/media layouts and full-width TMediaElement slots. | `map-trap.ips`, `ws11-map-freelist.log`; client and dedicated FX fixtures; **660c529**. |
| First forward movement hit stack-protector SIGABRT in PM_SlideMove. Arrays held five planes although the collision loop permits up to seven. | `src/PC/bgame/bg_slidemove.c:23`; eight native clip-plane slots, as verified in the 1.3 server reference. | `play2-abort.ips`; four-bump synthetic collision reproduces the old ASan index-5 overflow; **d18690a**. |
| First shot SIGBUS in FX_GetBoneOrientation. Caller declared the scheduler function variadic; Apple arm64 put axis/bolt on the stack while the definition read argument registers. A forward vector was also passed as a complete axis. | `src/PC/EffectsCore/Fxexport.c:9`, `:59`, `:72`, `src/PC/cgame_mp/cg_ents_mp.c`; exact nonvariadic prototype and full orientation matrices. Impact tables also retain pointer-width effect slots (`src/PC/cgame/cg_effects_load_obj.c:165`, `cg_weapons.c`). | `ws11-play3.log`; FX call fixture verifies axes and NULL bolt; **d02a7f5**. |
| After shooting, CG weapon assertion read weapon 24 when only 21 were loaded. Player-event transition addressed predictedPlayerEntity 24 bytes before its native location. | `src/PC/cgame_mp/cg_playerstate_mp.c:39`, `:276`; native offsetof for all affected CG field macros. | `ws11-play4.log` / subsequent probes; player-event fixture; **3783294**. |
| Rechambering then SIGSEGV in Com_PickSoundAliasFromList on pointer `0xffffffff80d9a0a0`. CG_EntityEvent truncated alias pointers to int and used a 436-byte weapon record stride. | `src/PC/cgame_mp/cg_event_mp.c:131`, `:1129`; native intptr_t alias loads and sizeof(weaponInfo_t) stride. | `ws11-play7.log`; final firing/reload probes survive; **3783294**. |
| `color_channel_mixer.tech` parsed a constant as a sampler under the zero-reflection ARB fallback, leading to the observed `32->64 marshal failed`. | `src/PC/gfx_d3d/r_material_load_obj.c:915`, `src/Mac/DirectX_9/D3DXShader.c:434`; parse native code constants and route materialColor weights to env[0] in an authored ARB channel mixer. | shader argument fixture, no matching errors in final menu/map logs; **c232e50**. This is not an audit of all 3535 material assets. |
| Quit after joining repeatedly cancelled the same endon record. Native candidate waittill/endon registration used levelId, while cancellation used pauseArrayId; timed termination also looked in pauseArrayId instead of timeArrayId. | `src/PC/script/scr_vm.c:296`, `:1502`, `:1533`, `:6926`, `:7017`; native table selection, preserve OFF table expressions. | `ws11-shutdown5.log`; notify fixture fails before and passes after; **8eacc39**. Cancellation now advances, but deeper shutdown corruption remains. |

## Remaining failures and merge notes

1. **Listen-server shutdown after joining remains blocked.** `quit` finishes
   client shutdown and entity/HUD frees, then spins in script cleanup.
   Before joining, shutdown exits 0 (`ws11-shutdown3.log`); after joining,
   final probes require killing their own PID. After the table correction,
   a deliberately sent SIGSEGV to the hung owned process captured
   `VM_TrimStack -> Scr_KillThread -> MakeVariableExternal`. This was a
   diagnostic signal, not a spontaneous gameplay crash. With that matching
   executable, `atos -o build-macos/cod2_macos -l 0x102628000 0x1027e9bf8
   0x1027ecb20 0x1027f1444` resolved `scr_variable.c:1755`, `:2389`, and
   `scr_vm.c:744`. See `ws11-play10.log`. The old executable has since been
   rebuilt, so those addresses should not be symbolized against the new one.
   A safe fix for the remaining hash/child cleanup corruption is not established.
2. Many FX `Emitter` groups still reject their `flags` key. The load succeeds,
   but affected effects are absent. Generic ARB shader fallbacks still produce
   white sky and imperfect lighting. Channel mixing is corrected only for the
   observed fallback family; full shader parity remains work.
3. The HUD shows repeated speaker icons; `sound/misc/beep.wav` is reported
   missing. Broad footstep/pickup/voice event behavior and all menu widgets have
   not been audited. Basic menu rendering and stock menu-response paths were
   tested, not a manual click-through of every menu.
4. This is local stock deathmatch bring-up, not proof of a full round, remote
   stock/CoD2x connectivity, desync parity, or stable 333 fps. Debug spam remains
   and no timing performance claim is made. WS9 timing/receive-buffer/debug
   gating and WS10 CoD2x features were left alone.

Merge the focused WS11 commits after `391870a` in order (16 implementation
commits, then this report). Shared header and top-level CMake changes were
avoided. The main possible conflicts are the small additions in
`cmake/macos-arm64.cmake`, native platform adapters/stub guards, and guarded
client sound/FX/cgame paths. Native JPEG support links ImageIO/CoreGraphics;
hardware queries link IOKit/CoreFoundation. These are SDK frameworks, not new
system packages. Several shared COD2_X64 layout fixes also benefit other LP64
targets; Apple-specific behavior is behind COD2_APPLE_SDK/direct native guards.
Keep the old OFF branches when resolving conflicts.

The final screenshot/log evidence stays outside git in E. The remaining script
shutdown failure should be the next runtime investigation after integration.
