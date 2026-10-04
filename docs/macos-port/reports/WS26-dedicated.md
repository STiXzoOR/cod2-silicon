# WS26 — native dedicated server on macOS arm64

Date: 2026-10-04. Branch: `port/dedicated`. Base: `3ef770ba088808f87805ba748839db2ca53e9dad`.
All source work stayed in this worktree. No sibling worktree was read or written,
no system package was installed, and no push, PR, issue or remote change was made.
Game data, screenshots, packet text and disassembly stayed in ignored private
`output/ws26/`; none is committed or included in the server archive.

## Result

`cod2_macos_ded` links and runs stock 1.3/protocol 118 without a renderer, SDL
window, OpenGL or audio initialization. Stock and CoD2x native clients connect,
spawn, move, fire/reload/aim, reload after `map_restart`, and rotate Toujane →
Carentan. The operator guide is [docs/server.md](../../server.md). A user launchd
service, daemon template, example config and separate server tarball script are
in `scripts/server/`.

The native builds, runtime checks and full ABI audits pass at zero mismatches.
The feature-only CoD2x guard checker reports intentional native stock changes,
listed in the handoff below.
The remaining release gate is the byte-identical i386 comparison: this Mac
cannot run that gate, which requires an x86_64 Linux multilib host. The native
preprocessor checks pass but do not replace that binary comparison.

## Headless dependency boundary

The dedicated target had already excluded client/render/audio translation units,
but generated data/import slots carried Darwin `used` retention annotations.
Those annotations rooted otherwise unused client imports and C++ vtables, so
`-dead_strip` pulled renderer/UI/voice owners into the link graph.

`tools/server/dedicated_data.py` makes a build-directory-only copy of the verified
LP64 data with just those retention annotations removed. It preserves alignment,
sections, contents, types and imports. The client still uses the original verified
blobs; the generator and committed typed snapshot are unchanged. A Darwin linker
fixture proves that an unused unresolved client import is stripped, while an
unresolved live server import still fails the link. Unused client imports keep
their client owners; live server references still require their real owners.

Read-only Mac 1.3 STABS checks established the excluded globals' actual owners:

| Symbol | Type / i386 size | Client owner / decision |
| --- | --- | --- |
| `g_current_bandwidth_setting` | `int`, 4 bytes | groupvoice `encode.cpp`; exclude voice graph |
| `g_editingField` | integer/qboolean, 4 bytes | UI shared editing state; exclude UI graph |
| `s_sundvars` | 21 `char *`, 84 bytes | renderer sky module; exclude renderer graph |
| `vec3_colorintensity` | three floats, 12 bytes | renderer BSP module; exclude renderer graph |

Native dedicated main omits the client `g_gfxV60DllActive` diagnostic reference;
it adds no replacement global declaration or definition.

CPU FX templates, parsing, lifetime and server visibility remain required by
scripts/visibility queries. Collision, script VM, xanim/DObj and sound-alias
metadata remain server dependencies; device output and rendering do not.
`str_002b3f60` is the shared HUD alignment word `fullscreen`; the headless adapter
provides that constant because server HUD scripts need the vocabulary normally
owned by UI. No renderer simulation stubs were added.

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-macos --target cod2_macos cod2_macos_ded --parallel 3
cmake -S . -B build-macos-codx -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
cmake --build build-macos-codx --target cod2_macos cod2_macos_ded --parallel 3
otool -L build-macos/cod2_macos_ded
nm -u build-macos/cod2_macos_ded
```

Both client variants and stock dedicated build pass; the CoD2x-configured
dedicated target also links. The live baseline uses the stock configuration.
The stock dedicated binary directly
loads only system zlib, libc++, IOKit, CoreFoundation and libSystem. Its undefined
symbols contain no SDL, GL, audio-device or Mac display entry points. It boots a
real map while the console session is locked. A physically detached display and
a boot-time system daemon were not tested. Building the target still shares the
native CMake configuration's development dependency discovery; the packaged
server runtime needs no SDL installation.

## Runtime fixes uncovered by the live test

- Stock dedicated now searches its separate writable `fs_homepath/main`, so
  `exec server.cfg` works without placing configuration in the owned data tree.
- The dedicated frame limiter waits on network/console readiness for the
  remaining interval instead of spinning with `NET_Sleep(0)`. Exploratory empty
  server CPU fell from about 83% before the fix to about 0.1% after it.
- SIGTERM/SIGINT set a `sig_atomic_t` request. The engine thread executes normal
  `Com_Quit_f`, including game/server shutdown; no teardown happens in a handler.
- Native `sv_master1`…`sv_master5` are configurable and cached. Public listing
  requires `dedicated 2`; native dedicated defaults to LAN mode (`1`). Mac 1.3
  disassembly at `0x1941ea` confirms the due-time comparison is `>=`, not the
  reconstructed reversed comparison. Timers are 180 s heartbeat / 600 s status.
- A second spawned client exposed hard-coded i386 client-info strides/offsets:
  native `clientInfo_t` is 1232 bytes, not 1208, with its animation-tree pointer
  at 1208. Native player/corpse accesses now use typed records and `offsetof`.
  The baseline regression fixture aborts; the corrected fixture exercises all
  64 player records under ASan/UBSan and preserves each record's tree and flags.
- `loadingnewmap` must unconditionally set `CA_CONNECTED`. Mac 1.3 at `0x14cec6`
  stores 5 unconditionally. Leaving a formerly active client in `CA_ACTIVE`
  stopped acknowledgements while its loading screen suppressed game input.
  The packet fixture tests every connected/loading/active state and rejects
  a packet from another base address. Live restart/rotation now reload clients.
- `leaks` found a 16-byte startup `net_port` value leaked during string-to-integer
  dvar promotion. Symbolized stack logging in a private, development-entitled
  copy pinpointed `CopyStringInternal` → dvar promotion. Mac 1.3 at `0x52c48` and
  `0x52c67` compares the live slots after clearing them. Native cleanup now does
  the same, releasing aliased strings once. The allocation fixture covers all
  five alias partitions, and the baseline aborts on its allocation count.
- The standard client recorder sampled only outer `Com_Frame` calls and skipped
  ticks when `SV_Frame` caught up with several simulation steps. A native-only
  hook now records each completed server tick; the existing frame-number guard
  suppresses duplicate outer records. The recorder passes 100 frames. Its new
  optional `--port` argument lets this smoke use reserved UDP 29230; callers
  without that option retain their original command. An earlier recorder run
  used default 28960 and was replaced by this isolated final run.

Every change to an existing engine path is native guarded, including explicit
`COD2_X64=0` handling. The new headless platform file is only a native target input.
No shared header or top-level `CMakeLists.txt` changed. Reference reconstructions
were read for behavior only; their code was not copied.

## Live tests and soak

Owned data: `/Users/stix/Games/CoD2`, read separately from private server/client
homes. Final live tests used distinct reserved UDP ports. All processes were
launched by this workstream; only those processes were stopped. The runner wraps
each engine in `timeout -k 10`.
The synthetic LAN key is a test value, not the user's retail key.

```sh
python3 tools/server/soak.py \
  --server build-macos/cod2_macos_ded \
  --client build-macos/cod2_macos --client build-macos-codx/cod2_macos \
  --data /Users/stix/Games/CoD2 \
  --shader-cache '/Users/stix/Library/Application Support/CoD2-native-ws15/shaders' \
  --codx-iwd /Users/stix/Projects/cod2-native-refs/CoD2x/src/embedded/iw_CoD2x_01.iwd \
  --output output/ws26/soak-active-final --port 29226 --duration 900 --test-key
```

The final 900-second active session kept both clients connected and playing.
`map_restart` ran at 240 s, rotation at 480 s, and `sv_fps 40` at 720 s.
All three engine exits were 0, and
`leaks` returned 0 with **0 leaks / 0 leaked bytes**. The unsigned binary warns
that memory contents cannot be shown, but reports its allocation graph and leak
count. No ASan whole-engine soak is claimed; the native regression/platform
fixtures run with ASan/UBSan.

Both clients reached `CL_InitCGame` three times, selected weapons on all three
loads, moved/fired/reloaded/aimed, and had visible weapon HUDs on Carentan at
502 s and 862 s. All 29 status samples contained both players. The runner retries
menu requests until the new map's menu table reaches the log; an earlier version
consumed that request too soon and left clients at team selection. The final
90-second smoke and the final 900-second active soak use the corrected runner.
No connection timeout or engine crash occurred in these final runs.

Exploratory measurements from the final active soak:

| Setting | Recorded single-step intervals | Median | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| `sv_fps 20` | 14,636 | 50.030 ms | 50.841 ms | 51.125 ms | 54.544 ms |
| `sv_fps 40` | 7,280 | 24.817 ms | 25.931 ms | 26.060 ms | 88.224 ms |

The trace spans server boot through quit; the active two-player segment is
900 seconds. Four initialization/catch-up batches and the two frame-number
resets at map changes are excluded from the single-step statistics, rather than treated as
ordinary tick jitter. The 88 ms single-step outlier remains included; these
results do not establish a hard scheduling bound. CPU samples every ~30 seconds
were 0.6–1.9%, median 1.0%. RSS started at 48,624 KiB and ended at 14,832 KiB,
peaking at 74,224 KiB near reload. `leaks` reported physical footprint `44.8M`,
peak `69.4M`. RSS was affected by host memory pressure and does not establish
a small working-set requirement.

**Measurement conditions:** this agent held `_agents/bench.lock`. IOKit reported
`IOConsoleLocked=True` / `CGSSessionScreenIsLocked=True`. Other workstreams were
building and running AST ABI audits; this workstream also ran brief fixtures.
All CPU/RSS/tick numbers are **exploratory**, not a release benchmark. No result
at more than two real clients, on a second Mac, or across a physical LAN is claimed.

The authenticated rcon `status`, `map_restart`, `map_rotate` and FPS change work;
an invalid password is rejected. `net_ip 127.0.0.1` binds the selected loopback
socket, and `sv_maxRate 25000` is registered/advertised in `getstatus`. The stock
LAN send path bypasses bandwidth throttling, so WAN rate enforcement was not
measured. Public/authorize service reachability was not tested.

The private loopback master receiver saw **zero packets** in LAN mode; public
mode with only `sv_master3` set to that receiver sent initial heartbeat/status,
a forced heartbeat and shutdown `flatline`. Both server exits were 0. The
server-control fixture also checks all five slots, DNS cache/error behavior,
due-time boundaries, game-complete status and deferred signal handling. No test
sent heartbeats to a public master.

All 16 IWD checksums advertised by the native server match an independent
ZIP-entry CRC → CommonCrypto MD4 → four-word XOR calculation, and stock/CoD2x
clients join with `sv_pure 1`. The existing production-MD4 fixture independently
checks keyed/unkeyed digests against CommonCrypto. The official Linux 1.3 control
binary matches the recorded research hash:
`05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`.
It cannot run natively on this Mac; a live comparison of its checksum inventory
still needs the Linux control host. Do not describe the hash check as that live
comparison.

## Service and packaging verification

Install/start/stop/status/logs/rotate/uninstall are implemented by
`scripts/server/cod2-silicon-server`. The normal install uses the user's
`~/Library/LaunchAgents` and a separate writable home; it preserves existing
configuration. It does not start until `start` is called. Install writes private
files and rejects engine path delimiters. A template is available for a system
`LaunchDaemon` under a non-root service account; administrator setup and daemon
log rotation are documented but were not installed or tested here.

The real launchd test used a worktree-private home/agent directory, label
`io.github.stixzoor.cod2silicon.ws26`, loopback port 29229 and the native dedicated
binary. It loaded the config/map, answered UDP status, restarted after SIGKILL
of the PID created by this test, then shut down normally on helper `stop`
(SIGTERM). `uninstall` removed those definitions; configs/logs were retained.
The status output confirms 2,048/4,096 open-file limits and core limit 0.

Hourly log rotation checks console and game logs at 10 MiB, keeps five backups,
and copies/truncates the existing inode. The helper fixture holds a descriptor
open, rotates a 10 MiB file, and proves subsequent writes reach the same inode.
It also verifies XML escaping, full launch arguments with spaces, private config
permissions and config preservation. Copy/truncate has a small concurrent-write
loss window and does not impose a hard disk quota.

```sh
python3 -m unittest tools.tests.test_server_service tools.tests.test_dedicated_data
shellcheck scripts/server/cod2-silicon-server scripts/server/package-server.sh
/usr/bin/plutil -lint scripts/server/*.plist
scripts/server/package-server.sh build-macos/cod2_macos_ded output/ws26/package
```

The tarball contains only the native server, guide/license, service helper,
configuration and templates. The packaging script rejects non-arm64 and direct
non-system dylibs. No client app packaging file, release workflow, remote or
published asset was changed. Release signing/notarization remains untested.

## CoD2x baseline

The native `COD2_CODX=1` client logs `CoD2x: using legacy CoD2 1.3 behavior` and
negotiates stock protocol 118 with this server. In the final 90-second smoke
and 900-second active soak, its owned `iw_CoD2x_01.iwd` was present only in
that client's writable `main/`;
the stock pure server accepted it, and the client spawned with British and then
American weapons after the map change. The short run and long active soak
exit cleanly, with no reconnect timeout after the packet-state fix.

This establishes the native CoD2x 1.4 client-build fallback baseline. It does not
establish Windows CoD2x binary compatibility or protocol-120/HWID/CoD2x server
extensions. No server-side CoD2x features were implemented.

## Merge gate and handoff

The complete `CONTRIBUTING.md` suite was executed from this worktree. Initial
stock timing and CoD2x online fixtures could not link/extract their optional
helpers; the fixture runner now uses the proper feature view/helper and all
those suites pass with their assertions enabled. No production assertion was
removed or baseline relaxed.

- Both native Release client builds and stock dedicated: pass.
- Tool/datagen/ABI checker unit tests; shader setup; Wine trace fixtures; CoD2x
  full (sanitized)/native/identity; stock and CoD2x online/fixes13; perf fixtures;
  LP64 game/script/renderer/raster/mips/volume; HUD/marks/FX/primitives/cloud/
  dedicated FX: pass.
- Platform CTest: 28/28 with ASan/UBSan, 30/30 without, including new headless
  controls, all-player records and dvar lifetime. Portable fixture tests are
  separate from the whole-engine live soak.
- Datagen round trip, private game layouts and 1.3 reference facts: pass.
- Full stock/CoD2x function/callback/import ABI: pass. Stock: 621 translation
  units; CoD2x: 638. Both have zero compiler errors, zero function mismatches,
  218 renderer bindings with zero table/named-cast mismatches and zero
  unprototyped floating calls. Both retail-import scans check 514 symbols,
  with zero extra/missing pointer dereferences and zero errors (1,776 active
  stock sites / 1,778 CoD2x sites). Reviewed baselines remain empty.
- `tools/abi/legacy.py --base 3ef770b…`: 11 files, five off configurations,
  zero mismatches. `tests/fixes13/legacy.py`: nine changed source files, ten
  legacy configurations, zero mismatches.
- CoD2x inactive-gate checker against the pre-workstream base sees intentional
  native stock fixes with `COD2_CODX=0`; it is a feature-only guard check, not a
  prohibition on native stock repairs. Explicit-zero/undefined 32-bit views
  remain identical. It reports 10 differences: `cg_players_mp.c`,
  `cg_snapshot_mp.c`, `cl_main_mp.c`, `sv_main_mp.c` and `dvar.c`, each in the
  `linux-x86_64-port-on-codx-zero` and `arm64-codx-off` native configurations.
  These are the multiplayer layout, loading, tick-recording and lifetime fixes
  described above, intentionally available to both native client builds.
- `tools/ci/compare-x86.sh 410342a HEAD …`: unavailable (exit 2, requires x86_64
  Linux/multilib). Native/off preprocessor identity is supplementary evidence,
  not byte-identical i386 binaries.
- Client smoke: `tools/parity/record.py --frames 100 --port 29230`, 100 complete
  contiguous populated-world records after the native per-tick hook; pass.
- Server + both client smoke: real 90-second restart/rotation/40-Hz runs, pass.
- `git diff --check`: pass.

The ABI gate commands, with the private Mac 1.3 STABS input available:

```sh
sh tools/abi/check.sh build-macos/compile_commands.engine.json output/ws26/abi-stock
sh tools/abi/check.sh build-macos-codx/compile_commands.engine.json output/ws26/abi-codx
python3 tools/abi/legacy.py --base 3ef770ba088808f87805ba748839db2ca53e9dad
python3 tests/fixes13/legacy.py --base 3ef770ba088808f87805ba748839db2ca53e9dad
```

The final standard client smoke used this environment and command; use fresh
output directories when repeating it:

```sh
mkdir -p output/ws26/client-local-home
env HOME="$PWD/output/ws26/client-local-home" \
  CFFIXED_USER_HOME="$PWD/output/ws26/client-local-home" \
  COD2_SETUP_NONINTERACTIVE=1 COD2_SETUP_GAME_DIR=/Users/stix/Games/CoD2 \
  COD2_SETUP_CD_KEY=000000000000000086D3 \
  COD2_MAC_SHADER_CACHE='/Users/stix/Library/Application Support/CoD2-native-ws15/shaders' \
  timeout -k 10 200 python3 tools/parity/record.py \
  --binary build-macos/cod2_macos --data /Users/stix/Games/CoD2 \
  --output output/ws26/client-smoke-port-final --frames 100 --port 29230
```

Merge the focused signed-off commits on `port/dedicated`; do not copy private
output. Shared edits are limited to the native CMake target's generated-data
view and native-guarded engine sections. `src/headers/*`, top-level CMake,
`macos_display.c`, Metal and `make_macos_app.py` are untouched. The orchestrator
must resolve the small native target additions alongside packaging work and run
the Linux object/binary comparison before calling the i386 promise verified.
Remaining hardware/operational checks are boot-time daemon setup, physically
unplugged display, another Mac/physical LAN, larger player loads, public master/
authorize reachability and WAN rate enforcement. Public packet-parser hardening
remains the roadmap work; the verified baseline is a trusted LAN/loopback server.
