# WS14 — online play on public servers

Date: 2026-10-03. Branch: `port/online`. Worktree:
`~/Projects/cod2-native-wt/online`. Base: `4ddccfd`; `port/main`
(`5f69688`, WS10) merged in `5f1cbc7`.

## Result

The native arm64 client now joins real internet servers, spawns, moves, fires,
reloads and stays connected:

| Build | Server | Version / protocol | Session |
| --- | --- | --- | --- |
| stock | `[DOGS]{UK} Clan COD2 Public server`, 46.30.12.39:28970 (TDM) | 1.3 / 118 | 6 min 40 s of play (7 min 12 s connected) on mp_downtown at `com_maxfps 250`: spawned with a Mosin-Nagant, moved, turned, fired, reloaded, ADS, crouch/prone, jumped, meleed; no error or disconnect (`final-stock-dogs`) |
| stock | `\|=HRC=\| Clan OG Public No Mod`, 37.44.215.192:28960 (SD, mp_leningrad) | 1.3 / 118 | 3 min 26 s connected after the crash fixes, joined allies and moved; the S&D `map_restart` re-opened the weapon menu, so no spawn in that run (`s8-hrc-lldb`) |
| CoD2x | `Kseftiles - Toujane - All Weapons 24/7`, 37.187.138.61:30055 (SD) | 1.4.6.8 / 120, `g_cod2x 6` | 6 min 40 s of play on mp_toujane at `com_maxfps 250`, `cl_maxpackets 125`, `snaps 40`, `rate 25000`; spawned, moved, fired, reloaded, survived an S&D round restart and respawned; no error or disconnect (`final-cod2x-kseft`) |
| CoD2x | `COD2x_HD_serv`, 5.130.157.156:28961 (zPAM 3.34, mp_toujane_fix) | 1.4.6.3 / 120 | downloaded 151 MB of zPAM IWDs over HTTP (two connects, see below), spawned, 2 min of play (`x4-hd`) |

Both builds pass stock CD-key authorization against the Activision authorize
server (challengeResponse within ~250 ms of the first `getchallenge`), with the
key read from `~/.cod2/preferences`. The key, its digest and the CoD2x HWID
never appear in the repository, the report or the saved logs: the driver
redacts them from every captured line.

Rendering defects already known from WS11 remain (white sky, over-bright
lighting, repeated HUD speaker icons, FX emitters without `flags`). They do not
affect connectivity.

## Builds and checks

```sh
cmake -S . -B build-macos/stock -DCOD2_X64=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos/stock -j12 --target cod2_macos
cmake -S . -B build-macos/cod2x -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos/cod2x -j12 --target cod2_macos
python3 tests/online/run.py build-macos/stock/compile_commands.json
cmake -S tests/platform -B build-macos/platform-tests -DCOD2_PLATFORM_SANITIZERS=ON
cmake --build build-macos/platform-tests -j12 && ctest --test-dir build-macos/platform-tests
sh tools/abi/check.sh build-macos/stock/compile_commands.json <out>
sh tools/abi/check.sh build-macos/cod2x/compile_commands.json <out>
python3 tools/macos-port/check_legacy_guards.py --base 5f1cbc7
```

- Both clients build with zero errors.
- `tests/online/run.py`: 17 production-function suites pass under ASan/UBSan
  (12 inherited, plus `challenge_resend`, `stream_spatialize`, `sprite_entity`,
  `www_download_begin`, `www_download`). Each new suite was run against the
  pre-fix source first and failed there (assertion, the live SEGV at 0x34, or an
  ASan stack-buffer-overflow).
- Platform CTest: 21/21 pass (the audio test now covers WAVE_FORMAT_EXTENSIBLE).
- ABI audit: stock 620 TUs, 0 errors, 0 mismatches, 218 renderer bindings with 0 mismatches; CoD2x 636 TUs, 0 errors, 0 mismatches, 218
  renderer bindings with 0 mismatches. After the WS10 merge the CoD2x build had
  7 mismatches (the demo uploader includes `<curl/curl.h>` while `dl_main.c`
  hand-declared libcurl); `35eea6f` fixes that.
- Legacy guards for this session's commits (`--base 5f1cbc7`): zero differences
  in the linux-i386, apple-i386, mingw-i386, clang-i386 and wasm bodies. The
  linux-x86_64 port-on body differs in `rb_tess.c` and `snd_driver.c` by design:
  those two fixes are LP64 bugs guarded by `COD2_X64`, not by an Apple guard.
  No 32-bit binary could be built on this Mac, so this is a source-level check.

## Servers tried

| Server | Build | Outcome |
| --- | --- | --- |
| 46.30.12.39:28970 DOGS (1.3 TDM) | stock | Plays; 4 min (`s9-dogs`) and the final session above. |
| 37.44.215.192:28960 HRC (1.3 SD) | stock | Before the fixes the client died right after joining (the previous agent's "hang"). Crash 1: SIGSEGV 0x34 in `MSS_SpatializeStreamImpl` (`s4-hrc/cod2_crash_59463.txt`). Crash 2: `__stack_chk_fail` in `RB_TessEntity` (`s7-hrc-lldb.log`). After both fixes it stayed connected for the whole 3.4 min run (`s8-hrc-lldb`). |
| 185.80.50.230:28960 Taszar_Lan (1.3 SD) | stock | Rejected with `error KEY_IS_GOOD`, with and without the PB digest. See "Not client bugs". |
| 217.154.160.67:28960 WMK_Ballerbude (1.3 DM) | stock | Auth OK; `Invalid Password.` (private server). |
| 37.187.138.61:30055 Kseftiles (1.4.6.8 SD) | CoD2x | Plays; 4 min (`x1-kseft`) and the final session above. Objective sprite "B" renders (the former `RB_TessEntity` abort path). |
| 5.130.157.156:28961 COD2x_HD_serv (1.4.6.3 zPAM) | CoD2x | Before: `DL_BeginDownload: could not open '<home>/main' for writing`. After `eb312ca`: 147 MB `zpam_maps_v4.iwd` downloaded over HTTP and installed. The second file stalled (server-side, below); a reconnect fetched the 4.3 MB `zpam334.iwd` and joined. |

The previous agent's candidate lists (`evidence/candidate-status.json`,
`cod2x-candidate-status.json`, `more-candidate-status.json`) were queried again
with `qstatus.py`; every server above had 0–1 players when joined. Sessions were
short and no chat or votes were sent. One lapse: a client from a failed lldb
attempt (`s6-hrc-lldb`, stdin not connected) was orphaned when lldb exited and
sat idle as a spectator on HRC for about 70 minutes before it was found and
stopped; it was probably the one player HRC later reported.

## Root causes and fixes

This session (all native-only behind `COD2_X64` / `COD2_APPLE_SDK`; ILP32
expressions retained):

| Symptom | Root cause | Fix |
| --- | --- | --- |
| Stock build's challenge request differed from 1.3 (the earlier `EXE_AWAITINGCDKEYAUTH` loop was the empty PB field, fixed by `214e7d0`) | The stock native client sent a bare `getchallenge`, so a 1.3 server asked the authorize server without `PB`; Mac 1.3 `CL_CheckForResend` (0x14ad5a) sends `getchallenge 0 "<seeded MD5>"`, which a 1.3 server forwards as `PB "<hash>"` in `getIpAuthorize`. It also resent every 3000 ms instead of 2000 ms, and `CL_BuildMd5StrFromCDKey` scanned 32 bytes past the key's NUL (1.3 scans min(strlen, 32), 0x14a90a). | `src/PC/client_mp/cl_main_mp.c:1915` (quoted digest), `:1890` (2000 ms), `:1752` (scan bound); `eb3f1ef` |
| SIGSEGV 0x34 after `MP_JOINED_ALLIES` on stock and CoD2x servers | `SND_StartAliasStreamOnChannel` passed the stream slot (channel − 0x20) to `MSS_SpatializeStreamImpl`, which indexes `chaninfo`, so it read an empty 2D channel's NULL alias. The helper also dotted with `origin[2]`/`axis[0]` instead of the listener's `axis[1]` and attenuated by the volume instead of the distance (Mac 0x57c52 / 0x56be0). | `src/PC/win32/snd_driver.c:1519`, `:809`; `251859d` |
| `__stack_chk_fail` in `RB_TessEntity` once objective icons appear | Screen-height sprites (renderFx 0x2000) fill `worldRadius[0..2]` but the array had two floats. | `src/PC/gfx_d3d/rb_tess.c:1281`; `2e2ffb6` |
| HTTP redirect download wrote to `<fs_homepath>/main` | `CL_BeginDownload` stored the names only in `clc`; `CL_ParseWWWDownload` reads `cls.downloadName/downloadTempName`, where Mac 1.3 keeps them (0x14d668). Completion then installed under `cls.downloadName`, which by then is the URL; 1.3 (0x1625cc) uses the saved original name. Finishes the previous agent's uncommitted fixtures. | `src/PC/client_mp/cl_main_mp.c:3189`, `src/PC/client_mp/cl_parse_mp.c:561`; `eb312ca` |
| zPAM FG42 reload sounds "invalid or corrupted format" | The native Miles replacement rejected WAVE_FORMAT_EXTENSIBLE PCM. | `src/platform/macos_audio.c:772`; `0e1eef3` |
| CoD2x ABI audit: 7 libcurl mismatches after the WS10 merge | `dl_main.c` hand-declared libcurl with `int` options. | `src/PC/qcommon/dl_main.c:19` uses the SDK header natively; `35eea6f` |

Earlier WS14 commits (previous agent, kept; tests in `tests/online/`):

| Commit | Fix |
| --- | --- |
| `07f0f5d` | LP64 pointer arguments for connection/browser text (`UI_ReplaceConversionString`, `ui_main_mp.c:1736`). |
| `7964b46` | No legacy CF preference indirection on quit (`MacPreferences.c`). |
| `76f6bb1`, `2669f11` | CoD2x identity uses the real CoreFoundation string APIs (`carbon_stubs.c/.h`). |
| `63336df` | Overlap-safe `Info_RemoveKey` (`q_shared.c:418`). |
| `c290c8c` | Pointer-width pure IWD name copy (`files.c:670`); the WS10 merge conflict resolved to this broader guard. |
| `3fc9cf8` | 32-bit MD4 words, so pure checksums match (`cod2_defs.h`). |
| `214e7d0` | Seeded-MD5 PB digest in `getKeyAuthorize` instead of an empty field. |
| `e7799c0` | Typed mantle transition records (`bg_mantle.c`). |
| `01bb6a0` | Active-connection timeout measured from the last packet (`EXE_ERR_SERVER_TIMEOUT` after `cl_timeout` on healthy sessions). |
| `3e40c39` | SIGTRAP included in the crash reporter. |
| `710a716` | Delayed effect records allocated and linked at native width (`FxScheduler.c`). |
| `918d0c8` | 1.3 reliable command opcodes (`d`/`x`/`y`/`z`/`B`/`n`/`w`) in `CL_GetServerCommand` (`cl_cgame_mp.c:423`); fixed "Server Disconnected - 13". |
| `b9c6727` | Master server replies parsed at pointer width (`cl_main_pc_mp.c:690`). |
| `787ef3e` | Time-delta averaging in 64 bits for servers with ~1.5e9 ms epochs (`cl_cgame_mp.c:1115`). |
| `83b562e` | Download requests name the `.iwd` files (`FS_CompareIwds`, `files.c:1088`). |

Checked and found correct: netchan XOR encode/decode keys (client and 1.3
server agree on challenge, sequence and acknowledged command text), fragment
reassembly and the 128 KB (`MAX_MSGLEN` 0x20000) receive buffer (11 KB
compressed / 11.4 KB decoded gamestates parse), reliable command sequencing and
acknowledgement, usercmd packing (`MAX_PACKET_USERCMDS` never hit at 125/250
fps and `cl_maxpackets` 125) and snapshot delta decoding (no delta, overflow or
illegible-message warnings in any session log).

## Not client bugs

- **`Unknown client game command: MP_CONNECTED…`.** Messages built by
  `iprintln(&"…", player)` arrive as ` "MP_JOINED_ALLIES\x15name^7"` with an
  empty opcode. `iprintln` passes `va("%c", 'f')` as `pszCmd`; the player
  token's own `va()` advances the server's two `va` buffers (`MAX_VASTRINGS` 2),
  so the final `va("%s \"%s\"", pszCmd, …)` in `Scr_MakeGameMessage` formats
  into the buffer it reads. glibc's `vsnprintf` clears the destination's first
  byte before formatting, so the `f` is lost on the Linux server. The decoded bytes (`x64_msg_trace.txt`: `04 20 00 00 00 20 22 4d 50…`)
  show the space before the quote on the wire, while `f "MP_TIMEHASEXPIRED"`
  (no player argument) arrives intact. The Mac 1.3 `CG_ServerCommand` would also
  print "Unknown client game command".
- **Taszar_Lan `error KEY_IS_GOOD`.** A stock 1.3 server only forwards the
  authorize server's reason as an error when the verdict is neither `accept`,
  `deny` nor `demo`; `KEY_IS_GOOD` accompanies `accept`. Other 1.3 servers
  accept the same client, key and packets, so this server's authorization hook
  is non-stock.
- **Second HTTP download stalls after a `map_restart`.** After an HTTP redirect
  the 1.3 server clears `downloadName`. If it restarts the map during the
  download, `SV_ExecuteClientMessage` takes the "pre map_restart" branch for the
  client's old `serverId` (high nibble equal): it calls `SV_ClientEnterWorld`
  and returns before reading client commands. The trace build showed exactly
  this: snapshots begin mid-download, and `wwwdl done` / `download
  main/zpam334.iwd` (reliable 4 and 5) are never acknowledged (ack stays 3).
  The client cannot learn the new `serverId` while the cgame is not loaded, so
  the original client is equally affected. Reconnecting fetches the rest.

## Evidence

Outside git, under `E=~/Library/Application Support/CoD2-native-ws14`:

- Driver: `E/ws14play.py` (connect, answer script menus again after
  `map_restart`, move/turn/fire/reload/ADS/stance/jump/melee, periodic
  `screenshotJPEG`, optional lldb with FIFO stdin via `WS14_LLDB=1`, optional
  CoD2x IWD copy via `WS14_COD2X_IWD`, key/HWID redaction). `E/qstatus.py`
  queries servers.
- Logs: `E/evidence/<tag>.log`, commands `E/evidence/<tag>-commands.txt`,
  results `E/evidence/<tag>.json` for the final runs.
- Screenshots (game-produced JPEGs): `E/<tag>/main/screenshots/`, and copies
  under `E/evidence/screenshots/`:
  `final-stock-dogs-spawn.jpg`, `-play04.jpg`, `-play09.jpg` (prone), `-final.jpg`
  (stock 1.3, DOGS); `s9-dogs-play03.jpg` (stock, Carentan, M1 Garand);
  `final-cod2x-kseft-spawn.jpg`, `-play10.jpg` (after the round restart),
  `-final.jpg` and `x1-kseft-play04.jpg` (CoD2x 1.4.6.8, objective sprite "B");
  `x4-hd-play02.jpg` (CoD2x 1.4.6.3 zPAM, mp_toujane_fix after the HTTP
  download); `s8-hrc-lldb-play02.jpg` (HRC after the two crash fixes; the S&D
  `map_restart` re-opened the weapon menu, which the first driver version did
  not answer).
- Crash records: `E/s4-hrc/cod2_crash_59463.txt` (stream SEGV),
  `E/evidence/s7-hrc-lldb.log` (sprite stack check), trace of the download
  stall `E/evidence/x6-hd-dl.log` (temporary instrumentation, never committed).
- Demos recorded during play: `E/<tag>/main/demos/`.

## Remaining issues

1. The second HTTP download after a server `map_restart` stalls (server-side,
   above). Reconnect works. A client-side workaround would have to adopt
   `sv_serverid` from queued systeminfo commands while downloading, which the
   original client does not do.
2. Taszar_Lan rejects with `KEY_IS_GOOD` (server-specific).
3. Known rendering defects: white sky, bright lighting, repeated speaker
   icons, FX `flags`, and a white quad where zPAM draws its HUD overlay
   (`x4-hd-play02.jpg`). `sound/misc/beep.wav` is still reported missing.
4. A full round with other human players, desync comparison against a
   reference client, and sustained 333 fps online were not measured.
5. `git stash` holds `stash@{0}: ws14-wip-www-download` (the previous agent's
   `run.py` lines, now committed in `eb312ca`); a safety hook blocked dropping
   it. `build/ws14-stock/` is an untracked build directory from the previous
   agent and can be deleted.

## Merge notes

`port/online` contains `port/main` at `5f69688`. The only conflict was
`FS_PureServerSetLoadedIwds` (kept the general `COD2_X64` guard). No pushes,
remotes, issues or PRs. No game data, binaries or keys were added.
