# CoD2x client compatibility matrix

Reference: read-only `~/Projects/cod2-native-refs/CoD2x`, commit
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`, version 1.4.6.8
(`src/shared/version.h:1`). Reviewed on 2026-10-03. Paths and line numbers below
refer to that snapshot, not opencod2. This records behavior, not copied code.
The reference has no project license file; its implementation must not be
transplanted. Windows binary addresses and layout assumptions are not portable.

This matrix describes requirements and effort, **not completed implementation**.
For delivered changes, checks, remaining limitations and live test instructions,
see `reports/WS4-cod2x.md`. No live game or server interoperability has been
verified on this Mac. Effort: S = local policy/limit change; M = several engine
integration points; L = substantial subsystem/state work. “None client” means
the remote server supplies the behavior; a listen server needs server work.

## Connection contract

“120 with fallback to 118” means accepting both advertised protocols and asking
masters for both lists. It does **not** mean trying `protocol=120` in `connect`:
the 1.4.6.8 server explicitly rejects any connect protocol other than **118**.
Both stock and CoD2x use the stock connection packet layout. CoD2x adds
`protocol_cod2x=6` and `cl_hwid2=<32 characters>` to its userinfo. The server
accepts extension versions greater than or equal to its own; the client must
only claim an extension version whose behavior it implements.

| Behavior | Reference evidence | Side | Classification | Effort / required behavior |
|---|---|---|---|---|
| Advertised protocol 120; stock 118 accepted | `src/shared/shared.h:50`; `src/shared/common.cpp:366`; `src/mss32/master_server.cpp:75` | Both | Required to connect through browser | S: accept exactly 118 and 120 in info responses; retain 118 connect encoding. |
| Discovery requests for both versions | `src/mss32/master_server.cpp:41` | Client | Required for both server lists | S: request `getservers 118 full empty` and `getservers 120 full empty`; direct IP connect does not require a master. |
| Connect protocol and extension version | `src/shared/server.cpp:388`; `src/shared/server.cpp:405`; `src/mss32/cgame.cpp:62`; `src/shared/version.h:3` | Both | Required to connect | S: stock protocol 118 plus ROM USERINFO `protocol_cod2x=6`; zero or less than server extension version is rejected. |
| HWID2 | `src/shared/server.cpp:427`; `src/mss32/hwid.cpp:766`; `src/mss32/hwid.cpp:793`; `src/mss32/hwid.cpp:1046` | Both | Required to connect | M: client produces 32-character MD5-style text; server checks length, not the hardware algorithm or hex alphabet. macOS must deterministically hash its own IOKit IOPlatformUUID, never invent or imitate another machine identity. Do not send raw UUID. |
| Legacy numeric HWID and bans | `src/mss32/hwid.cpp:672`; `src/mss32/hwid.cpp:693`; `src/mss32/hwid.cpp:1045`; `src/shared/server.cpp:437` | Both | Required identity semantics; legacy key optional | S: reference sends `cl_hwid`, but server ignores it for admission and computes nonzero 32-bit FNV-1a from HWID2 for existing GUID bans. Keep arithmetic explicitly unsigned 32-bit and avoid the reference's aliasing cast. |
| Challenge request and reply | `src/shared/server.cpp:797`; `src/shared/server.cpp:836`; `src/shared/server.cpp:844`; `src/shared/server.cpp:871` | Both | Required to connect | S/M: preserve challenge retransmission, server-address matching and `challengeResponse <integer>` handling. A three-token getchallenge supplies CD-key hash as token 2; absence is not rejected here. LAN can answer immediately; public auth timeout is server-side, 5 seconds. |
| CD-key authentication | `src/shared/server.cpp:640`; `src/shared/server.cpp:684`; `src/shared/server.cpp:741`; `src/shared/server.cpp:787` | Both | Required to connect to authenticated servers | M: preserve legitimate stock auth and `needcdkey` / rejection handling. Server forwards optional PBHASH to the original auth routine and stores auth status. This is distinct from HWID2. No new client auth opcode is introduced. Stock PBHASH is seeded MD5 (seed 0xB684A3; verified in `src/other/Call_of_Duty_2_Multiplayer_MAC_1.3.c:260725` / `:375944`, Windows `src/other/CoD2MP_s.c:22160`), not standard MD5. Do not substitute HWID for the real CD-key hash. |
| Server opt-in key policy | `src/shared/server.cpp:688`; `src/shared/server.cpp:1297` | Server | Connection policy | None client: `sv_cracked` changes server acceptance; a compatible client should not assume it is enabled or bypass authentication. |
| Version and error strings | `src/shared/common.cpp:322`; `src/shared/common.cpp:330`; `src/shared/common.cpp:361`; `src/shared/server.cpp:390` | Both | Connect diagnostics; branding optional | S: version/shortversion display 1.4.6.8 in reference, while admission checks numeric protocol keys. Preserve stock OOB error parsing, including localized error marker 0x15. An independent port should identify itself truthfully rather than claim to be the Windows patch. |
| Existing userinfo keys | `src/shared/server.cpp:291`; `src/shared/server.cpp:303`; `src/shared/server.cpp:324`; `src/shared/server.cpp:346`; `src/shared/server.cpp:352`; `src/shared/server.cpp:448` | Both | Required to connect/play | S: retain name, rate, snaps, cl_voice, cl_wwwDownload, challenge and ordinary stock connect fields (qport/password, etc.). New fields must fit the existing bounded info string. |
| Server-selected compatibility state | `src/shared/game.cpp:9`; `src/shared/server.cpp:1094`; `src/shared/server.cpp:1211`; `src/mss32/cgame.cpp:81` | Both | Required for correct play and stock fallback | M: consume SYSTEMINFO `g_cod2x` and its ordinary server cvar command on ClientBegin (`src/shared/cod2_server.h:138`); reset to 0 on leaving server. Most gameplay corrections start at version 3. Do not leave 1.4 behavior active on a later 1.3 connection. |

PBHASH caveat: opencod2's baseline `src/PC/client_mp/cl_main_mp.c` sends bare
`getchallenge`. The reference server tolerates that at its own request handler,
but delegates internet authorization to original binary code. Source inspection
alone does not establish authenticated internet connectivity with the bare
request. Test it against a non-cracked server with a valid owned CD key; do not
claim successful authentication from a unit test or LAN challenge response.

## Gameplay and packet semantics

| Behavior | Reference evidence | Side | Classification | Effort / required behavior |
|---|---|---|---|---|
| 40 snapshots/second | `src/mss32/cgame.cpp:241`; `src/shared/server.cpp:323`; `src/mss32/competitive.cpp:68` | Both | Required for correct play | S: allow `snaps=40`; server clamps to 1..40 and sets `snapshotMsec=1000/snaps`. Competitive mode forces 40. This changes cadence, not encoding. |
| 40 server ticks/second | `README.md:124`; `src/shared/server.cpp:338` | Server | Required for correct play | S/none client: configure live server `sv_fps 40`. README claims a raised cap, but no sv_fps registration/patch was found in shared/mss32 current source. opencod2 already permits 10..1000 (`src/PC/server_mp/sv_init_mp.c:429` at baseline); do not invent a new packet field or an absent patch. |
| 125 client packets/second | `src/mss32/cgame.cpp:244`; `src/mss32/competitive.cpp:73` | Client | Required for correct play | S: allow `cl_maxpackets=125`; competitive mode forces 125, allowing one packet per two frames at 250 FPS. |
| Legacy `com_maxfps_limit` | `README.md:111` | Both | Required legacy policy compatibility | M: README describes server-controlled, cheat-protected restriction to 125..250. There are **no** occurrences of this name in current shared/mss32 sources; the current implementation is `g_competitive` below. Legacy semantics must be labelled as inferred from README, not verified current wire behavior. |
| Current competitive FPS cap | `src/shared/server.cpp:1313`; `src/mss32/competitive.cpp:35`; `src/mss32/competitive.cpp:56` | Both | Required for correct play | M: `g_competitive` SYSTEMINFO restricts FPS to 125..250; an out-of-range current value becomes 250. Restore original limits on disable/disconnect; ignore restriction during demos. |
| Competitive network/render/sound controls | `src/mss32/competitive.cpp:63`; `src/mss32/competitive.cpp:78`; `src/mss32/competitive.cpp:100` | Both | Required for correct play when enabled | M: force rate=25000, snaps=40, maxpackets=125, sc_enable=0, fx_sort=1 and mss_q3fs=1. Restore domains on disable. Audio implementation may differ on macOS, but the policy must be accounted for. |
| Competitive wait command | `src/mss32/competitive.cpp:15` | Client | Required for correct play when enabled | S: disable `wait` when competitive and primed/active; allow while connecting and during demo playback. |
| Stance animation classification and timing | `src/shared/animation.cpp:1101`; `src/shared/animation.cpp:1127`; `src/shared/animation.cpp:1160`; `src/shared/animation.cpp:1374` | Both | Required for correct play | L: expanded crouch movement classification; match first-person stance timing, including faster crouch→prone animation blend. Disable scripted transition events while correction is enabled. Keep original transitions on stock servers. |
| Stance controller offsets / peeking | `src/shared/animation.cpp:496`; `src/shared/animation.cpp:713` | Both | Required for correct play | L: timed body/head position and angle correction through crouch/prone transitions, controller interpolation and separate listen-server/client per-player state. Timing-only changes do not implement this row. |
| Prone high body / reload and movement posture | `src/shared/animation.cpp:367` | Both | Required for correct play | L: prone pelvis/spine/origin corrections vary by direction, lean, firing and reload. Verify visible pose against server hit model. |
| Torso/weapon alignment, diagonal and leg swing | `src/shared/animation.cpp:325`; `src/shared/animation.cpp:359`; `src/shared/animation.cpp:859`; `src/shared/animation.cpp:950`; `src/shared/animation.cpp:1005` | Both | Required for correct play | L: align yaw and remove inappropriate movement offsets, immediate swing response. The special “ignore legs yaw direction” case is only extension 3 (`:979`), disabled since 1.4.4.1; do not blindly apply that old behavior to 6. |
| Ladder head rotation | `src/shared/animation.cpp:439` | Both | Required for correct play | M: controller pitch/yaw tracks view direction during climb up/down. |
| Cancel grenade stance lock | `src/shared/animation.cpp:479` | Server | Required for correct play | S/none remote client: server clears legsTimer when throw animation no longer fits crouch/prone state. Client must render received state correctly; listen server needs matching rule. |
| Backward movement direction | `src/shared/animation.cpp:1295`; `src/shared/animation.cpp:1331` | Both | Required for correct play | M: validate backward state against actual velocity/view direction, not just BACK input, for matching prediction and animation. |
| `+smoke` / empty offhand event flood | `src/shared/server.cpp:1393` | Both shared movement path; authoritative fix server | Required for correct play | S: suppress repeated EV_EMPTY_OFFHAND production when an unavailable offhand is held, preventing four-event ring overflow from hiding fire events. No new event type or ring size. Client prediction and listen-server PM code should agree. |
| PVS-hidden player sound | `src/shared/server.cpp:1248`; `src/shared/server.cpp:1310` | Server | Required for correct play | None remote client / S listen server: broadcast living player entities regardless of PVS while player count is at most sv_playerBroadcastLimit (default 15; 0 disables). Client consumes ordinary entities/events. A client cannot synthesize entities/sounds never sent by a stock server. |
| Consistent shotgun pellet pattern | `src/shared/weapons.cpp:48`; `src/shared/weapons.cpp:247`; `src/shared/weapons.cpp:256` | Server | Required for correct server gameplay; no new client logic | None remote client / M listen server: `g_shotgun_spread_fix` chooses server bullet spread. This hook replaces G_BulletFireSpread, not client packet decoding. |
| Changed packet contents | `src/shared/server.cpp:388`; `src/shared/game.cpp:14`; `src/shared/server.cpp:1049`; `src/shared/server.cpp:1274`; `src/shared/server.cpp:1393` | Both | Required for correct play | M validation: extra text userinfo/systeminfo keys, different ordinary events/entities and cadence. No replacement usercmd/entity/playerstate delta codec, netfield table, extra binary field or changed netchan framing was found. NET_SendPacket logs and forwards the same bytes. Retain stock little-endian/bit encoding. opencod2 uses signed 8-bit playerstate movementDir (`src/PC/qcommon/msg_mp.c:48`) and ordinary entity angles2[1] (`:248`); the corrected movement yaw fits -90..90 and requires no netfield change. |
| Larger info-string consistency | `src/shared/common.cpp:380` | Both | Optional IWD robustness; may matter on heavily modded servers | S: reference changes Info_SetValueForKey_Big length check from 1024 to its actual 8192-byte capacity. This is not a wider binary length field. Audit the existing port helper before changing anything. |

## Optional client-visible behavior

These are outside the mandatory connection/gameplay implementation. Some servers
can impose additional match or mod requirements, so “optional” does not promise
access to every private match service.

| Behavior | Reference evidence | Side | Classification | Effort / scope |
|---|---|---|---|---|
| Match login, credentials, match lifecycle / stats | `src/shared/match.cpp:462`; `src/shared/match.cpp:750`; `src/shared/gsc_match.cpp:159` | Both | Optional | L; `match_login` is additional USERINFO used for match access, not general DirectConnect. Preserve privacy of supplied credentials. |
| Automatic demo recording/upload and quit deferral | `src/mss32/demo.cpp:43`; `src/mss32/demo.cpp:230`; `src/mss32/demo.cpp:487` | Both (server dvars/client upload) | Optional | L; cl_demoAutoRecordName and upload URL drive client recording/HTTP workflow. |
| vMIX spectator camera integration | `src/mss32/vmix.cpp:65`; `src/mss32/vmix.cpp:177` | Both (script metadata/client HTTP) | Optional | L; no game wire extension. |
| Radar/spectator calibration | `src/mss32/radar.cpp:195`; `src/mss32/radar.cpp:646` | Client | Optional | L; spectator/cheat/demo restrictions apply. |
| `cod2x://` launch links | `src/mss32/url_protocol.cpp:15`; `src/mss32/url_protocol.cpp:94` | Client | Optional | M; macOS URL registration is separate platform work. Do not reuse arbitrary command execution semantics without explicit scope. |
| IWD filtering and pure-file selection | `src/shared/iwd.cpp:477`; `src/shared/iwd.cpp:545`; `src/shared/iwd.cpp:571`; `src/shared/iwd.cpp:606` | Both, principally client | Optional | L; initial stock-only load, server/demo referenced files, movie assets, latest listen-server zPAM selection. Never bundle game data. |
| Embedded CoD2x IWD extraction/cleanup | `src/shared/iwd.cpp:230`; `src/shared/iwd.cpp:326`; `src/shared/iwd.cpp:798` | Both | Optional; excluded assets | M; do not import proprietary/redistribution-unclear archives or perform destructive cleanup in this port. |
| Shared config path and write switch | `src/shared/iwd.cpp:710`; `src/shared/iwd.cpp:758`; `src/shared/iwd.cpp:838` | Client | Optional | M; main/config_mp.cfg and com_writeConfig. |
| Demo developer/cheat convenience | `src/mss32/cgame.cpp:110`; `README.md:114` | Client | Optional | S; demo start enables developer=2 and cheats, restores developer afterward. README also describes cheats on disconnect; do not confuse demo relaxation with live-server policy. |
| Third-person orbit mode | `src/mss32/cgame.cpp:65`; `src/mss32/cgame.cpp:148` | Client | Optional | M; mode 1 bypasses world collision; original path retained below extension 3 unless explicitly selected. |
| LOD detail control | `src/mss32/hook.cpp:149` | Client | Optional | S; r_lodScale 0..1 and archive flag. |
| Windowed/borderless and focus/cursor handling | `src/mss32/window.cpp:596`; `src/mss32/window.cpp:624`; `src/mss32/window.cpp:793` | Client | Optional CoD2x parity; platform goal | L, WS3; native macOS/SDL implementation instead of Windows hooks. |
| Raw mouse and refresh statistics | `src/mss32/rinput.cpp:33`; `src/mss32/rinput.cpp:131`; `src/mss32/rinput.cpp:333` | Client | Optional CoD2x parity; required overall port goal | L, WS3; native raw deltas, not copied Windows threads/hooks. |
| CPU affinity | `src/mss32/affinity.cpp:28`; `src/mss32/affinity.cpp:54` | Client | Optional | M; Windows-specific policy need not be reproduced on Apple Silicon. |
| Startup dialogs / microphone black-screen workaround | `src/mss32/hook.cpp:330`; `src/mss32/hook.cpp:338`; `src/mss32/hook.cpp:344` | Client | Optional/platform-specific | S/M; reference disables microphone initialization and startup dialogs. Native audio belongs to WS3. |
| Sound initialization failure detection | `src/mss32/window.cpp:702` | Client | Optional/platform robustness | S; report actual native backend failures. |
| Write access / VirtualStore handling | `src/mss32/admin.cpp:76`; `src/mss32/admin.cpp:103`; `src/mss32/registry.cpp:120` | Client | Optional/Windows-only | None on macOS; use normal user-owned data paths, no elevation requirement. |
| Crash reporter / freeze watcher | `src/mss32/exception.cpp:1179`; `src/mss32/freeze.cpp:147` | Client | Optional | L; macOS diagnostics separate from Windows minidumps. |
| Dvar pool 4096 / clearer exhaustion error | `src/shared/dvar.cpp:10`; `src/shared/dvar.cpp:52`; `src/shared/dvar.cpp:84` | Both | Optional mod robustness | M; source arrays/allocation must be widened coherently, never copy pointer patches. |
| Developer pose offsets / animation diagnostics | `src/shared/animation.cpp:740`; `src/shared/animation.cpp:1423` | Both | Optional | M; cheat/debug controller offsets and player_debug are omitted; their default disabled state adds no gameplay requirement. |
| Higher memory budget | `src/shared/common.cpp:386` | Both | Optional | S; minimum/default com_hunkMegs=512 in reference. |
| Console completion and quieter messages | `src/mss32/hook.cpp:358`; `src/shared/server.cpp:1412`; `src/mss32/cgame.cpp:95` | Both | Optional | S; more cvars shown, warnings suppressed, compatibility mode printed. |
| Smaller download/recording text | `src/mss32/downloading.cpp:22`; `src/mss32/cgame.cpp:248` | Client | Optional | S. |
| Killfeed custom colors / bullet debug | `src/mss32/drawing.cpp:173`; `src/mss32/drawing.cpp:277` | Client | Optional | M; con_printDoubleColors, cheat-protected cg_debugBullets. |
| Increase/decrease console commands | `src/mss32/cgame.cpp:28` | Client | Optional | S. |
| Unsigned port formatting | `src/shared/common.cpp:355` | Both | Optional diagnostics | S; fix display, not byte order or network representation. |
| URL-friendly command tokenizer | `src/shared/common.cpp:132`; `src/shared/common.cpp:189` | Both | Optional | M; preserves http:// tokens and expands command-line handling; not a new packet field. |
| Configurable master / public IP / heartbeat | `src/mss32/master_server.cpp:41`; `src/shared/server.cpp:492`; `src/shared/server.cpp:1293`; `README.md:137` | Both | Optional beyond dual-protocol discovery | M; current client code resolves two fixed masters despite README listing client cvars. Server exposes sv_master1..3; keep this discrepancy explicit. |
| UDP rate limiting and packet tracing | `src/shared/server.cpp:76`; `src/shared/server.cpp:967`; `src/shared/server.cpp:1049`; `src/shared/server.cpp:1299` | Server; tracing both | Optional server operations | M; client must retransmit normally rather than flood requests. |
| GSC HTTP/WebSocket/player/match helpers | `src/shared/gsc_http.cpp:24`; `src/shared/gsc_websocket.cpp:1`; `src/shared/gsc_player.cpp:30`; `src/shared/gsc_match.cpp:14` | Server | Optional | L for listen-server mod parity; remote client receives ordinary game commands/configstrings. |

## Never implement in this port

| Behavior | Reference evidence | Side | Classification | Effort / decision |
|---|---|---|---|---|
| Auto-update DLL download, replacement, forced update flow | `src/mss32/updater.cpp:45`; `src/mss32/updater.cpp:158`; `src/mss32/updater.cpp:297` | Client | Never | Excluded by task. No updater endpoint, binary replacement, downloaded code loading or execution. |
| Automatic zPAM/mod updates | `src/shared/iwd.cpp:134`; `src/shared/iwd.cpp:204` | Both | Never for this workstream | Excluded: automatic script/mod update is outside compatibility and can introduce executable game scripts. IWD filtering may be implemented independently without downloads. |
| DLL hot reload / patch-loader machinery | `src/mss32/hotreload.cpp:14`; `src/mss32/hotreload.cpp:73` | Client | Never as port mechanism | Architecture-neutral compiled source replaces binary injection; never implement download-and-run behavior. |

## Evidence and live verification boundary

The reference was inspected with `nl -ba`/`sed` and focused `rg` searches over
`src/shared`, `src/mss32` and `README.md`. In particular:

```sh
rg -n 'protocol|PBHASH|cl_hwid2|g_competitive' \
  ~/Projects/cod2-native-refs/CoD2x/src/shared/server.cpp
rg -n 'com_maxfps_limit|sv_fps' \
  ~/Projects/cod2-native-refs/CoD2x/src/shared \
  ~/Projects/cod2-native-refs/CoD2x/src/mss32 --glob '!symbols*'
rg -n 'WriteDelta|ReadDelta|netfield|MSG_Write' \
  ~/Projects/cod2-native-refs/CoD2x/src/shared \
  ~/Projects/cod2-native-refs/CoD2x/src/mss32 --glob '!symbols*' --glob '!mongoose/**'
```

Absence of a source hook is evidence about this reference, not a live packet
capture. When the client runs, capture both server connections, compare ordinary
snapshot/usercmd decoding, observe 25 ms snapshots and 125 packets/s under suitable
conditions, and exercise stock→CoD2x→stock reconnects and demos. Compare every
stance/controller row with a reference client from the same observer position;
unit tests of durations alone cannot prove pose or hitbox agreement. Test the PVS
sound case with two clients on opposite sides of a portal and both below/above
the server broadcast threshold. Test held empty smoke/frag while shooting and
confirm remote fire events remain audible/visible. Verify competitive restrictions
and restoration, including values already outside the permitted range. All such
runtime verification remains pending until the macOS client can run.
