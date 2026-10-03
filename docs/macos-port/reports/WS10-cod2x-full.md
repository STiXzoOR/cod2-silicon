# WS10 — full native CoD2x client

Branch: `port/cod2x-full`. Worktree: `/Users/stix/Projects/cod2-native-wt/cod2x-full`.
Base: `231d6be5d1f49864862a98a3e72d7b6ddbe840fa`. Verified on 2026-10-03.
Reference: the read-only CoD2x 1.4.6.8 snapshot identified in
[`cod2x-compat.md`](../cod2x-compat.md). Behavior was independently implemented;
reference code, binaries, IWDs and decompiler dumps were not imported.

The requested client features in priorities 1–9 now have source implementations
or the native/server-owned equivalents described below. The match service's
authoritative backend is outside the client implementation. vMIX was skipped as
allowed by priority 10. **The full game still cannot link, so live feature parity,
authenticated server admission and sustained 333 fps are not proven.** Both
enabled and disabled arm64 client source sets compile with zero errors. Both
links stop at the inherited typed-data `_dlText` alignment error. The dedicated
source set also compiles, then fails on generated renderer/data roots.

No sibling worktree was accessed. No package was installed, remote changed,
branch pushed, PR/issue opened, global configuration changed, or licensed game
file written. Test binaries, synthetic archives and fixture app bundles were
temporary and removed. Build logs remain in ignored `build/ws10-on/`.

**Delivered changes and decisions.** Enable the existing gate with
`COD2_X64=ON` and `COD2_FEATURE_CFLAGS=-DCOD2_CODX=1`; there is no CMake
`COD2_CODX=ON` option. Either inactive gate removes the new engine hooks. CMake
also omits all nine added translation units from the disabled native build,
avoiding additional empty objects/debug inputs. No `src/headers/*` file changed.

IWD selection starts with stock archives, selects server references on connection,
includes movie archives during demos, chooses the latest numeric zPAM/map package
for a listen server, and permits the selected `fs_game` folder. The CoD2x archive
is stock from extension revision 5 onward. Systeminfo changes mark the selection
dirty even when the checksum feed stays unchanged; disconnect resets the names.
Archive filtering runs before existing localization and pure checks. Filesystem
pointer-list copies use native pointer width only behind the gate. The writable
homepath's `main/` is added to the search path, so the separately extracted IWD
and shared config can coexist with the read-only game install.

Both direct and profile-prefixed `config_mp.cfg` accesses map to
`fs_homepath/main/config_mp.cfg`, bypassing mod archives. This follows the task's
explicit single shared file requirement. `com_writeConfig=0` suppresses writes;
turning it off after an enabled write persists the switch once. Starting with it
disabled produces no write. The pool is 4096 with a named exhaustion diagnostic;
`com_hunkMegs` has default/minimum 512 MiB. The gated default frame cap is 333;
inherited competitive server restrictions still take precedence.

Visual controls use the engine's existing renderer and dvar APIs. Increase/decrease
uses external setters, retaining cheat/ROM/latch permissions. Bullet and pose
diagnostics also check live cheat/demo permission. Radar loads/saves `.radar`
definitions, displays player/team numbers, fire pulses and objectives, and uses
four-stage mouse calibration. The native pose diagnostics show compact numeric
conditions, animation IDs, angles and timers rather than Windows hook-specific
labels. Neutral pose reset is confined to live player entities; a corpse sharing
its owner's client number cannot reset that owner's state.

`match login <hash>` sets NOWRITE USERINFO `match_login`. It survives the implicit
disconnect that precedes `connect`, so a login then connect sequence works.
Inspection of `src/shared/match.cpp:462,750` and `gsc_match.cpp` found the client
login control, with match creation, JSON data, lifecycle and stats owned by the
server/GSC backend. There is no separate client match-data packet codec or native
match UI in that reference. Existing server-driven menus/configstrings and the
user-owned IWD are retained; an actual match service/UI session is unverified.

Native `m_rinput` is latched/archived: 0 disables the CoD2x raw path, 1 delivers
GameController deltas on the serial callback queue, 2 uses the main queue. The
existing `in_rawmouse` preference and focus handling still apply; SDL motion is
suppressed while raw motion supplies aim. `m_rinput_hz`, `m_rinput_max`, and the
reference-compatible `m_rinput_hz_max` alias measure received motion callbacks
over ten 100 ms buckets. Delayed reads do not create a false peak. These values
are observations of callbacks, not a claim about a USB device's hardware polling
rate. Physical mouse/acceleration behavior remains unverified.

The extension master uses archived `cl_masterServer`/`cl_masterPort`, defaulting
to `master.cod2x.me:20710`. Activision remains the other master. Discovery asks
for 118 and 120; connect still encodes 118. No 119 advertisement was introduced.
The bounded tokenizer retains URL slashes and quoted strings while handling
comments. Big info strings use their actual 8192-byte capacity; ordinary info
strings and packet encodings were not widened.

Automatic demos use sanitized names and collision suffixes. Server-supplied
strings never enter a recording command buffer. Upload markers persist under
the writable demo directory, including directories remembered across mod changes.
The SDK libcurl multi API sends HTTPS-only, certificate/hostname-verified,
chunked `application/octet-stream` POSTs. Redirects are disabled; 200/201/409
finish the upload. Three bounded attempts retain failed markers and the demo;
failures are skipped for the remainder of the run so quitting can complete.
Regular-file checks and `O_NOFOLLOW` reject marker/demo symlinks. The upload cap
is 5 MiB/s; active transfer progress appears in the small recording-style HUD.
Quit closes recording and keeps the frame loop responsive until queued uploads
finish/fail. It can drain the queue even if a connection is stalled. Error quits
bypass this deferral. Demo playback enables developer 2/cheats before its first
message and restores both original values on completion/disconnect.

The URL handler parses only a hostname/IPv4 endpoint with an optional bounded
port and password. It rejects arbitrary commands, separators, control characters,
invalid/duplicate connect requests, paths and userinfo-style hosts. AppKit events
queue only the parsed connect/password commands. The bundle tool emits
`CFBundleURLTypes` plus an external licensed game path, copying only the caller's
arm64 executable. It does not install/register anything globally. Browser to
LaunchServices to a real game process remains unverified until the game links.

`com_freezeWatch` defaults on. A pthread observes the main-thread heartbeat and
requests one native context report after a 12-second stall; loading keep-alives
and disabling the dvar prevent false reports. The process remains running.
Freeze/crash reports contain real arm64 registers and backtraces, are created
with mode 0600 in homepath, and omit the launch command line. Reporting uses the
existing best-effort signal reporter; it is not guaranteed async-signal-safe or
capable of recovering from corrupted stacks. The existing sound driver already
prints the native AudioUnit failure through `AIL_last_error`.

**Parity table.** “Done” means implementation/engine integration with local
evidence, including inherited WS4 logic; it does not mean a live game passed.
“Native-equivalent” replaces a Windows mechanic or diagnostic presentation.
“Partial” identifies remaining validation or backend scope. “Skipped” identifies
an explicit exclusion or a server-only implementation outside this workstream.
This table covers the compatibility matrix as well as the WS10 feature list.

| CoD2x behavior | Status | Evidence / reason |
| --- | --- | --- |
| Advertised 118/120 acceptance | Done | Inherited WS4 protocol tests; no protocol 119. |
| Dual-protocol master discovery | Done | Existing dual requests preserved; configurable extension master added. |
| Connect protocol 118 / extension 6 | Done | WS4 encoder/userinfo tests retained. |
| Stable 32-character HWID2 | Native-equivalent | Inherited IOKit identity, hashed independently; raw machine ID is not sent. |
| CD-key hash / authenticated internet admission | Partial | Seeded PBHASH tests pass; valid-key internet admission requires a running client/server. |
| Server cracked/key policy | Skipped | Remote server policy; no authentication bypass. |
| Version strings, banner and OOB errors | Done | Native 1.4.6.8 branding; inherited stock error parsing. |
| Ordinary userinfo keys | Done | Stock connect encoder preserved; bounded match login added. |
| Server-selected state / stock reconnect fallback | Done | WS4 tests plus IWD-name reset on disconnect. |
| 40 snapshots/sec | Done | WS4 registration/policy tests; live cadence pending. |
| 40 server ticks/sec | Skipped | Remote server configuration; existing listen registration already permits 40. |
| Legacy `com_maxfps_limit` | Done | Inherited README-based compatibility; not represented as a verified current wire key. |
| Competitive 125–250 fps limit | Done | WS4 policy/demo/reconnect tests; domains restore. |
| Competitive rate/snaps/packets/render/audio controls | Done | Existing policy remains; native AudioUnit does not reproduce Miles DSP internals. |
| Competitive `wait` restriction | Done | Inherited active/primed policy with demo exception. |
| Stance classification / first-person timing | Done | Inherited typed helper tests; rendered/hitbox comparison pending. |
| Stance controller offsets / peeking | Done | Inherited controller/state tests; observer comparison pending. |
| Prone high body / reload / movement posture | Done | Inherited posture fixtures; live geometry pending. |
| Torso/weapon alignment / diagonal / leg swing | Done | Inherited tests, including extension-3-specific legacy behavior. |
| Ladder head rotation | Done | Inherited typed controller calculations; live view pending. |
| Grenade stance-lock cancellation | Skipped | Authoritative remote-server fix; an opencod2 listen server is not made a full CoD2x server. |
| Backward movement direction | Done | Inherited displacement-based prediction/controller behavior. |
| Empty offhand event flood | Done | Inherited shared movement suppression; legitimate final-ammo event retained. |
| PVS-hidden player sound broadcast | Skipped | Remote CoD2x server supplies entities/events; no local broadcast-policy implementation. |
| Consistent shotgun pellet spread | Skipped | Authoritative server bullet spread; ordinary client packet handling retained. |
| Packet/usercmd/entity codec | Done | Stock encoding retained; no invented binary fields. Live captures pending. |
| Larger big info-string limit | Done | Production helper passes 8000-byte list and overflow/replacement checks. |
| Match login / credentials | Done | Production command fixture verifies USERINFO flags and login→connect retention. |
| Match data, lifecycle, stats and UI | Partial | Server/GSC-owned data and existing UI path; no match backend or live UI session verified. |
| Automatic demo recording | Done | Production client hooks, safe names, collision handling and stoprecord protection. |
| Demo upload / progress / quit deferral | Done | Persistent queue tests and real local HTTPS POST; stalled-connect regression covered. |
| vMIX spectator camera integration | Skipped | Optional last priority; no vMIX HTTP/session work added. |
| Radar / spectator calibration | Native-equivalent | Native draw/save/calibrate hooks and geometry/reader ownership tests; rendered inspection pending. |
| `cod2x://` links | Native-equivalent | Parser, AppKit callback, command queue, generated arm64 bundle and plist tests. |
| Initial IWD filtering / pure-file selection | Done | Stock/server/mod/latest-zPAM fixture tests and actual filesystem hooks. Live pure server pending. |
| Demo IWD / movie selection | Done | Stored gamestate names before the demo early return; movie directory integration. |
| Embedded client IWD extraction | Done | Owned DLL and release ZIP extract identical validated IWD bytes in memory; no asset committed. |
| Destructive IWD cleanup | Skipped | Compatibility does not require deleting the user's files; excluded by the plan. |
| Shared `main/config_mp.cfg` / write switch | Done | Path policy tests and gated filesystem/common hooks; real config persistence awaits launch. |
| Demo developer/cheat convenience | Done | Production playback fixture verifies enforcement and restoration of both values. |
| Third-person orbit mode 1 | Done | Pure orbit tests and collision-bypass integration; original default branch retained. |
| `r_lodScale` | Done | Gated 0..1 archive registration; live LOD appearance pending. |
| Windowed/borderless/focus/cursor | Native-equivalent | Inherited WS3 implementation/tests; full game display validation pending. |
| Raw mouse modes and refresh statistics | Native-equivalent | Production input routing plus counter/rate tests; physical GameController delivery pending. |
| CPU affinity | Native-equivalent | Uses native macOS scheduling instead of Windows CPU masks; no performance benefit is claimed. |
| Startup dialogs / microphone black-screen workaround | Native-equivalent | WS3 native startup/audio; legacy microphone capture remains unavailable. |
| Sound-init failure message | Native-equivalent | Existing native backend's actual failure text; no fictitious Windows device diagnostics. |
| Write access / VirtualStore / administrator policy | Native-equivalent | User-owned homepath; no elevation/registry/VirtualStore dependency. |
| Crash reporter | Native-equivalent | Real SIGABRT fixture verifies arm64 registers/backtrace and private report file. |
| `com_freezeWatch` | Native-equivalent | Real heartbeat, disabled-watch and 12-second-stall fixture passes. |
| Dvar pool 4096 / exhaustion error | Done | Production allocator registers/finds 4096, reports exhaustion and reuses after shutdown. |
| Developer pose offsets / animation diagnostics | Native-equivalent | Cheat-protected eight-controller offsets, local neutral reset and native overlays; corpse regression covered. |
| `com_hunkMegs` 512 | Done | Gated minimum/default 512, original off-gate registration retained. |
| 333 default frame cap | Done | Registration default 333; actual steady frame rate unverified. |
| Console completion / quieter messages | Done | 60-match display, wider names; reference local warnings/game banners suppressed; absent 999-ping print needed no change. |
| Smaller download / recording text | Done | Gated layout/font changes compile; screenshots pending. |
| Killfeed `con_printDoubleColors` | Done | Caret/color fixtures and production death-message writer. |
| Cheat-protected `cg_debugBullets` | Done | External permission tests plus native debug-line integration. |
| `increase` / `decrease` | Done | Production callback fixture and overflow/float step tests. |
| Unsigned port formatting | Done | Gated unsigned host-port formatting; wire byte order unchanged. |
| URL-friendly tokenizer | Done | Bounded URL/quote/comment/overflow fixtures and production command hook. |
| `cl_masterServer` / `cl_masterPort` | Done | Archived resolver controls, range 1..65535; live discovery pending. |
| Public IP / server heartbeat / third server master | Skipped | Server operations; client retains ordinary retransmission/discovery. |
| UDP rate limiting / packet tracing | Skipped | Server operations; no additional client flood/tracing layer. |
| GSC HTTP/WebSocket/player/match helpers | Skipped | Authoritative server/listen-server extensions, not a new remote-client wire contract. |
| Auto-updater / binary replacement | Skipped | Explicitly forbidden; no updater or download-and-run mechanism. |
| Automatic zPAM/mod updates | Skipped | Explicit plan exclusion; local selection only. |
| DLL hot reload / patch loader | Skipped | Explicit exclusion; native source replaces Windows injection. |

**Commands and observed results.** Run from this worktree:

```sh
sh tests/cod2x/run_full.sh
COD2X_SANITIZERS=1 UBSAN_OPTIONS=halt_on_error=1 sh tests/cod2x/run_full.sh
python3 tools/macos-port/check_legacy_guards.py \
  --base 231d6be5d1f49864862a98a3e72d7b6ddbe840fa
python3 tests/cod2x/check_inactive_gates.py \
  --base 231d6be5d1f49864862a98a3e72d7b6ddbe840fa
git diff --check
```

Both runners exit 0. The normal run includes real TLS, bundle metadata, 14 seconds
of keep-alives, 14 seconds with the watch disabled, a 14-second stall and a
SIGABRT child. The reports contain `pc=` and `x28=` and have mode 0600. Sanitizer
mode covers added C logic/production integration fixtures and excludes deliberate
signal tests. The unchanged WS4 runner and standalone TLS subprocess retain
their original compile flags. The legacy comparison reports **29 files × 6
inactive configurations, zero mismatches**. The expanded comparison includes
Objective-C and explicit `COD2_X64=0`, undefined X64 with CODX enabled, and
`COD2_CODX=0`: **30 files × 17 configurations, zero mismatches**. These compare
preprocessed bodies with includes removed, not binary ABI or emitted debug data.
Actual 32-bit byte-for-byte binary parity could not be run with this Mac's SDK;
it remains an orchestrator/WS5 cross-build check. No missing tool was installed.

```sh
cmake -S . -B build/ws10-on -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build/ws10-on --target cod2_macos -j12
cmake --build build/ws10-on --target cod2_macos_ded -j12
cmake -S . -B build/ws10-off -DCOD2_X64=ON -DCOD2_FEATURE_CFLAGS=
cmake --build build/ws10-off --target cod2_macos -j12
```

All three source sets have **zero compiler errors**. These build commands exit 2
at link, not success. Enabled and disabled client links report:

```text
ld: pointer not aligned in '_dlText'+0xC (.../build/x64_gen/data_native.c.o)
```

The dedicated link has 149 undefined generated renderer/data roots, including
`CColorArray_Disable`, `CG_DObjCalcPose` and `CIN_PlayCinematic`. Client-only new
modules are excluded from that target. No renderer/data stubs were added to hide
the typed-data problem. `build/ws10-off/compile_commands.json` was also checked:
all nine new translation units are absent. Existing LP64 warnings remain; zero
errors does not mean a warning-free or runnable engine.

The extraction tool was additionally checked against both licensed reference
release inputs in memory:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 - <<'PY'
import hashlib, importlib.util
from pathlib import Path
spec = importlib.util.spec_from_file_location('iwd', 'tools/cod2x/extract_iwd.py')
iwd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(iwd)
ref = Path.home() / 'Projects/cod2-native-refs/CoD2x'
expected = (ref / 'src/embedded/iw_CoD2x_01.iwd').read_bytes()
for name in ('mss32.dll', 'CoD2x_1.4.6.8_windows.zip'):
    payload = iwd.extract_payload((ref / 'zip/windows' / name).read_bytes())
    assert payload == expected
    print(name, len(payload), hashlib.sha256(payload).hexdigest())
PY
```

Both produced 11907 bytes, SHA-256
`fcb20f972cea3ed57eb2479976626fbe34bc1a58cea2d626135e379b51c2fa16`.
No licensed output was written. For actual use after integration, extract the
user's release into a writable homepath, keeping `~/Games/CoD2` read-only:

```sh
python3 tools/cod2x/extract_iwd.py /path/to/owned/mss32.dll \
  "$HOME/Library/Application Support/CoD2-native"
# Once a real client links:
python3 tools/cod2x/make_macos_app.py build/ws10-on/cod2_macos \
  build/ws10-on/CoD2x.app --game-dir "$HOME/Games/CoD2"
```

These two usage commands were not run against the game install or an engine
executable. Only owned/synthetic fixtures were used for write/bundle tests.

**Verification provenance.** Existing WS4 tests were inspected and retained.
The independently delegated visual, demo and native units ran their local
fixtures and supplied evidence; controller verification ran the integrated
runners and builds. Reuse, quality and efficiency reviews produced concrete
fixes: dvar modified-state APIs, radar empty-buffer ownership, per-draw material
and team-color lookup, avoiding unchanged per-frame dvar setters, allowing quit
uploads during a stalled connection, and protecting corpse owners from neutral
pose reset. No external model/worker service or shipping workflow was used.

| Unit | Changed behavior / tests / observed evidence |
| --- | --- |
| IWD/config/extractor | No earlier selection/extractor tests existed. Initial new-helper/tool checks failed while absent; synthetic fixture tests pass. Actual release bytes match read-only reference. Config writer persistence has no standalone UI fixture; guarded production compile plus path tests, live check pending. |
| Pool/hunk/info/network | Production pool fixture failed at the old 1280 limit; now passes 4096, exhaustion, reuse. Production big-info fixture failed against the baseline 1024 check; now passes. Tokenizer fixtures exercise URLs, comments, quotes and bounds. Hunk/version/master/port registration are simple gated integrations verified by source/compile and gate comparisons. |
| Visual/commands/radar | New helper checks initially failed without stepping/color/orbit/calibration functions. Production command fixture verifies external setter source and cheat/userinfo flags. Geometry, radar temp-buffer ownership and color parsing pass. Rendering/layout changes intentionally use compile verification instead of mock pixel tests; live screenshots are blocked. |
| Raw mouse | Worker added counter/rate and actual `IN_Frame` fixtures: latch/modes, raw-vs-SDL suppression, focus reset, aliases and timing math pass. Physical device behavior is a separate pending check. |
| Demos | New policy/client/queue fixtures cover safe names/URLs, retries, playback restore, protected stoprecord and deferred quit. Actual TLS POST preserves exact fixture bytes and hostname rejection sends none. Review regression fixtures verify no repeated setters and stalled-connection drain. |
| Native URL/watch/crash | Parser edge cases, actual AppleEvent callback and bundled metadata pass. A real stalled thread and SIGABRT produce arm64 context reports; deliberate signals excluded from sanitizer mode. Browser/real client dispatch is blocked by link. |
| Developer pose | Added during final matrix audit; there was no existing engine fixture. Direct production-module characterization, permission, vector backing, neutral toggling, diagnostic routing and corpse regression checks pass. No pre-implementation red result is claimed for this late unit. |
| Build/docs | Source lists, ARC/AppKit linkage and disabled-module omission checked. Reports/runner documentation require no separate behavioral test. Both guard comparison tools and whitespace/shell syntax checks pass. |

**What remains.** WS2 must fix typed-data alignment and dedicated roots before a
real launch. Then validate stock→CoD2x→stock reconnects with an owned CD key,
same-checksum mod changes, large pure IWD lists, startup/shared-config persistence,
movie/demo archives and competitive restoration. Verify first-person and observer
poses, hit model, ladder/prone/lean, empty offhand/fire events, colored killfeed,
LOD/orbit, radar calibration/objectives, download/record/upload HUD, real match
menus and public master discovery. Test recording/upload against the actual
service, quit from loading, completed/failed queues across restarts, native URL
registration from a browser, physical raw input and sustained 333 fps where the
server permits it. WS3's unavailable microphone/voice initialization and native
Miles/EAX quality differences are inherited limitations, not fixed here.

**Merge information.** Apply the implementation commits in order:
`cdfefd2` (IWD/config), `79de02c` (limits/network), `0a54839` (visual/radar/pose),
`8f25077` (demos), `3bfa70b` (native input/URL/diagnostics), followed by this
verification/report commit. The shared CMake changes are nine lines in
`CMakeLists.txt` and eight in `cmake/macos-arm64.cmake`; preserve source omission,
client-only dedicated exclusions, ARC and AppKit when resolving integration.
Existing-body changes are small and gated. There are no shared-header edits and
no data-generation changes. Merge the full ordered series before building; do
not solve the inherited link error with opaque blobs, foreign binaries or dummy
functions. No push or external publication is needed from this branch.
