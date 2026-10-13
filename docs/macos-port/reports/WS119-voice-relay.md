# WS119 — Server-relayed voice, observation timing and spectator routing

2026-10-09 wave; verification continued on 2026-10-10. Worktree
`readiness-fixups`, branch `codex/readiness-fixups`, clean starting HEAD
`5fc9812`. This already includes the native server `"v"` repair;
release has the equivalent `3b5b69b`. Code commit: 02a4cba.

The post-command-fix relay failure was a **measurement artifact**. Repeated
runs prove that forwarded frames decode and play; the old driver sampled the
talker after its original 299 ms timeout. Its synthetic producer also left
seven frames queued because it made no later transmit call after server time
advanced. WS119 separately found and repaired a real routing
defect: spectators were incorrectly subject to the team filter when
`voice_global` was off.

Stock and CoD2x dedicated/listen live receipts now prove byte-preserving relay
of all eight independently encoded synthetic frames. Each observed talker
decoded and queued 2560 PCM bytes at 8192 Hz; its actual AudioUnit callbacks
consumed 7501 output frames, with 7401 nonzero voice contributions. In listen mode,
both the host and the remote client's original local echo played the remote
talker. This proves this bounded relay/playback path, not microphone capture,
physical speaker acoustics, original decoded PCM parity or whole-game fidelity.

Evidence remains outside Git:
`~/Library/Application Support/CoD2-native-current-readiness/WS119.noindex/`.
The report, five-line engine repair and synthetic regression fixture are the
only changes for composition. No raw disassembly, original PCM, game data,
private configs, screenshots, QA executables or generated shaders are committed.

## Reference findings and causal chain

I read `CONTRIBUTING.md`, `docs/macos-port/PLAN.md`, the existing voice/host/UDP
fixtures and WS112 before editing engine source. The owned full-STABS Mac 1.3
binary has SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
I inspected symbols with `nm -a` and `otool -arch i386 -tV -p <symbol>` without
saving raw disassembly. `reference-findings.json` records readable conclusions.

| Step | Mac 1.3 entry/evidence | Finding |
| --- | --- | --- |
| Eligibility | `Voice_SendVoiceData`, `0x147ae8` | Requires `sv_voice`, `cl_voice`, rate >= 5000, client state 8 and talking or pending frames. Native conditions agree. |
| Enqueue/transmit | `Client_SendVoiceData`, `0x14a868`; `CL_VoiceTransmit`, `0x147a9e` | Send only with pending data and server-time difference >199 ms or count >9; clear the queue and update last transmit. Native cadence agrees. |
| Client wire | `CL_WriteVoicePacket`, `0x187fac` | `v` + NUL, short qport, byte count, repeated byte length and encoded payload. One frame after an idle interval is valid; the remaining seven wait for a later flush. |
| Server admission | `SV_VoicePacket`, `0x159e36` | Match base address and qport, repair translated port, choose active `SV_UserVoice` versus pregame path. |
| Active input | `SV_UserVoice`, `0x1bba24` | Byte count and byte frame sizes; each frame reaches the talker's game entity. |
| Game routing | `G_BroadcastVoice`, `0x192d0c` | Original checks listener session state 3 at `0x192d71` before the global/team rule. That spectator bypass was absent. Dead-chat, local echo, server mute and `sendVoice` checks still apply. |
| Talker identity | `G_BroadcastVoice`, `0x192da5..0x192dd3` | Queue the original `talker->s.number`. `voice_localEcho` permits the same entity to receive its own frame; it does not remap the talker. |
| Relay output | `SV_SendClientVoiceData`, `0x1bb770` | `v` + NUL, byte count, repeated talker, byte length and unchanged payload. The existing command repair agrees. |
| Client receive | `CL_VoicePacket`, `0x1478f2` | Read up to 40 frames, validate talker/length, honor local mute and `cl_voice`, then call `Voice_IncomingVoiceData`. |
| Decode/play | `Voice_IncomingVoiceData`, `0x1ea43c` | Update the talk timestamp, decode and submit returned PCM bytes to that talker's sample. Existing 160-sample/8192 Hz playback is retained; see WS112. |
| Indicator | `Voice_IsClientTalking`, `0x1ea584` | Active at elapsed 299 ms, inactive at 300 ms. No timeout extension is justified. |

The previous private receipts show `sendvoice` to `voiceobserve` acknowledgement
deltas of **505 ms** (`ded-codx2`), **510 ms** (`listen-stock2`) and **456 ms**
(`listen-codx2`), despite the driver's nominal 100 ms sleep. They captured only
one client-to-server 48-byte datagram and one 47-byte relay datagram: one
38-byte encoded frame. Immediate `Client_SendVoiceData` sent the first frame
after the idle interval, then queued seven; the same-frame `CL_VoiceTransmit`
could not cross the 199 ms threshold. Native capture is unavailable, so the
synthetic producer had no later recorder callback to flush that tail. This
was not evidence of a wrong talker number, client mute, codec rejection or
loss of seven already-transmitted frames.

CoD2x 1.4.6.8 `src/shared/server.cpp:241` delegates voice admission to the stock
implementation. Its userinfo handler retains `cl_voice` and rate policy.
`src/mss32/hook.cpp:327–336` disables Windows microphone registration and voice
receive for the Windows mixer hang. This supplies no alternate Mac routing or
rate contract; the existing WS110/WS112 native playback adaptation remains.
`COD2X_HOST_REVISION=0` is unchanged.

## Product repair and regression fixture

`src/PC/game_mp/g_client_mp.c` now permits
`SESS_STATE_SPECTATOR` through the team filter before checking `voice_global`,
behind `defined(COD2_X64)`. Everything after that filter retains the original
mute, subscription, session/dead-chat and echo rules. No cadence, timeout,
codec, PCM rate, movement, rendering or microphone behavior is changed.

The spectator omission is already present in the reconstructed upstream
`410342a` body. Existing playback fixtures began at client receive and could
not catch server/game routing or the command mismatch. The new production
relay fixture covers those previously missing boundaries.

The new `--relay-contract` option of `tests/voice/playback_init.py` reuses its
controlled output-device/capture fixture. `tests/voice/relay.c` connects actual
client enqueue/write/transmit, production message code, server admission/user
voice, game broadcast, server queue/send, client receive, Speex, native queues
and `Render`. UDP transport, entity/team fixtures, dvar registration and device
creation are boundaries. No game data, real socket or audio device is needed.
Actual Release flags, `-UNDEBUG`, ASan and UBSan remain enabled.

Before the engine edit, `relay-red-stock5/result.json` and
`relay-red-codx/result.json` are red, exit **-6**, specifically at
`spectators must receive other teams' voice with voice_global off`. Their
ten-frame relay, original local echo, one/seven-frame cadence and timeout
checks pass first. `relay-green-stock` and `relay-green-codx` both exit **0**
after the repair. The fixture also checks team/global/dead-chat rules,
server mute and `sendVoice`. Earlier stock setup attempts are retained as
adapter/extraction/link failures, not counted as the intended red regression.

```sh
E="$HOME/Library/Application Support/CoD2-native-current-readiness/WS119.noindex"
taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py --relay-contract \
  --compile-db build-macos/compile_commands.engine.json --output "$E/relay-green-stock"
taskpolicy -b nice -n 19 python3 tests/voice/playback_init.py --relay-contract \
  --compile-db build-macos-codx/compile_commands.engine.json --output "$E/relay-green-codx"
```

Use fresh output names to rerun. `--baseline 5fc9812` reproduces the defect.
The only changed engine file passes **exact byte equality** after
`unifdef -UCOD2_X64` on both the `git show 5fc9812:<path>` source and current
source, followed by `cmp`. `off-proof.json` records both hashes, byte counts
and `cmp` exit 0. This is supplementary source preservation evidence; the
x86 Linux object/binary comparison remains unavailable on this Mac.

## Live observation and results

`make_observer.py` recompiles private copies of two production translation
units with their unchanged Release flags and relinks the client against all
other original Release objects. Counter calls are added around production
incoming voice/decode/queue and the native AudioUnit `Render` mixer. They record
successful decodes, accepted bytes, rates, talker activity immediately after
decode and consumed/nonzero output frames for the particular voice handle. The
nonzero count tests that handle's dry contribution after its sample volume
and dry gain, above 0.00001. Master dry gain defaults to 1.0; its only engine
setter call (`snd_driver.c:714`) also supplies 1.0.
No receive function is replaced, no codec result is synthesized, and no PCM
is saved. The private observation executable is identified honestly; it is
not byte-identical to the uninstrumented Release executable. Exact commands,
source hashes and binary hashes are archived in `observer-records/stock/`,
`observer-records/codx/` and `production-provenance.json`.

`network_relay.py` submits eight mode-5 frames from the unchanged independent
`tests/voice/fixtures.json` corpus to `Client_SendVoiceData`. It performs no
direct client `v` injection or receive-helper call. A localhost UDP proxy
records count, talker, length and SHA-256 per frame in both directions. A
later private `flushvoice` command calls production `CL_VoiceTransmit` after
normal server time advances; it does not force time or bypass the threshold.
The resulting wire counts are **1, 7** in each direction, with all eight frame
hashes equal. Reception passes through normal connectionless `v` dispatch.

| Receipt | Client/server/proxy ports | Talkers observed | Decoded/queued PCM per talker | Output frames/nonzero voice contributions | Driver exit |
| --- | --- | --- | --- | --- | --- |
| `ded-stock` | 31260 / 31261 / 31262 | Echoed sender 0 | 2560 / 2560 bytes | 7501 / 7401 | 0 |
| `ded-codx` | 31266 / 31267 / 31268 | Echoed sender 0 | 2560 / 2560 bytes | 7501 / 7401 | 0 |
| `listen-stock-r1` | 31320 / 31321 / 31322 | Remote 1 on host and remote echo | 2560 / 2560 bytes each | 7501 / 7401 each | 0 |
| `listen-codx-r1` | 31323 / 31324 / 31325 | Remote 1 on host and remote echo | 2560 / 2560 bytes each | 7501 / 7401 each | 0 |

All six talker observations have **8 decoded frames, 0 decode failures and
8 checks showing an active talker immediately after decoding**. The receipt
field `packets` counts calls to `Voice_IncomingVoiceData`, one per encoded
frame. `activations` increments for each successful immediate
`Voice_IsClientTalking` check. Queue/render rate is **8192 Hz**, with
**15–16 render callbacks**.
1280 decoded PCM samples should occupy 7500 output frames at 48 kHz; the
driver requires 7500–7508 for possible queue-underrun rounding. Later
`active=0` snapshots are retained alongside the immediate activity and PCM
counters; the original talk timeout remains unchanged.

All runs used an unlocked session monitored every 0.5 seconds, the shared
`bench.lock` lease, short 640×480 windowed sessions and `timeout -k 5` bounds
(90 seconds client, 100 seconds host). Private `fs_homepath` directories and
unique loopback ports were used. The existing shader cache was read via
`COD2_MAC_SHADER_CACHE`. The lease was removed as soon as the GUI runs ended.
No stale lease takeover was necessary.

Two initial listen runs remain **red**: snapshot acknowledgement timed out
at the unchanged five-second limit. The driver accepted the listen host's
`CS_ACTIVE` log line as readiness for the remote join, then requested snapshots
while the joining client was loading. The corrected private driver gives the
host/remote distinct test names and waits specifically for
`CS_ACTIVE for WS119Remote`; both fresh listen retries pass without increasing
any timeout. `live-commands.json` and `live-retry-commands.json` retain exact
commands, exits, timings and lock observations. Dedicated passes preceded
that readiness-only correction. Clients in all four successful cases exit 0; both successful
listen hosts exit 0. Dedicated servers are stopped by the owning driver.

The four `screenshotJPEG ws119-relay` captures were inspected at original
640×480 resolution: Toujane is visible behind the Deathmatch Click to Continue
overlay. `visual-inspections.json` records this bounded observation. This does
not establish voice-icon timing, a fully played match or broader visual parity.
The live logs also retain a non-voice missing `sound/misc/beep.wav` diagnostic.

To reproduce, rebuild the private observer/probe from the retained scripts,
take the GUI lease and use fresh labels:

```sh
taskpolicy -b nice -n 19 python3 "$E/make_observer.py" build-macos
taskpolicy -b nice -n 19 python3 "$E/make_observer.py" build-macos-codx
taskpolicy -b nice -n 19 python3 "$E/compile_input.py"
# Lease must be owned before running either topology; the session must be unlocked.
taskpolicy -b nice -n 19 python3 "$E/network_relay.py" --build build-macos \
  --label ded-stock --port 31260 --server-port 31261 --proxy-port 31262
taskpolicy -b nice -n 19 python3 "$E/network_relay.py" --build build-macos-codx \
  --label ded-codx --port 31266 --server-port 31267 --proxy-port 31268
taskpolicy -b nice -n 19 python3 "$E/network_relay.py" --listen --build build-macos \
  --label listen-stock-r1 --port 31320 --server-port 31321 --proxy-port 31322
taskpolicy -b nice -n 19 python3 "$E/network_relay.py" --listen --build build-macos-codx \
  --label listen-codx-r1 --port 31323 --server-port 31324 --proxy-port 31325
```

## Builds, existing checks and remaining gates

Both stock and CoD2x client/dedicated Release targets built successfully:

```sh
taskpolicy -b nice -n 19 cmake -S . -B build-macos -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -b nice -n 19 cmake --build build-macos \
  --target cod2_macos cod2_macos_ded --parallel 2
taskpolicy -b nice -n 19 cmake -S . -B build-macos-codx -DCOD2_X64=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
taskpolicy -b nice -n 19 cmake --build build-macos-codx \
  --target cod2_macos cod2_macos_ded --parallel 2
```

The stock targets were rebuilt after the engine edit. The CoD2x build already
contains it. Exact engine compile databases are preserved separately; source
fixtures enable their assertions without changing Release engine flags.

`verify_mode.py stock` and `verify_mode.py codx` each exit 0. Their
`checks-<mode>/commands.json` records these exact suites, outputs and durations:

- `tests/voice/run.py --sanitizers`: 55 checks, 0 failures per mode.
- `tests/voice/playback_init.py`: initialization, all 64 talkers, local gates,
  unavailable/synthetic available capture, shutdown and device-failure cases.
- The same runner with `--rate-contract`: default/zero/negative cases all pass,
  each 60001 rendered frames at 512 Hz for the independent 1.25-second tone.
- `tests/cod2x/host_lifecycle.py`, listen and `--dedicated` contexts.
- `tests/cod2x/run_host_protocol.py`, both client and dedicated targets.
- `tests/cod2x/udp_operations.py`, both targets.
- `tests/cod2x/run_player_broadcast.py`, both targets.

Both full engine compile databases pass the following ABI audits. Exact
commands, exits and durations are in `abi/commands.json`; `run_py_2.py` caps
executor concurrency where the tools lack a suitable two-worker option.

| Check | Stock | CoD2x |
| --- | --- | --- |
| Raw offsets | 391 sources, 1197 sites, 0 unsafe/unreviewed | 404 sources, 1203 sites, 0 unsafe/unreviewed |
| Function ABI | 624 TUs, 0 errors/mismatches/new findings | 641 TUs, 0 errors/mismatches/new findings |
| Callback tables | 218 bindings; 0 table/cast mismatches or unprototyped floating calls | Same |
| Strict bit conversions | 0 violations | 0 violations |
| Import ABI against Mac 1.3 STABS | 1779 active sites, 0 errors or proven extra/missing dereferences | 1782 active sites, 0 errors or proven extra/missing dereferences |

Function and callback baselines contain zero entries. The four ABI unit
suites pass **17/17** tests. Trace guards report **0 unguarded diagnostics in
726 files**. Unit-only `tools/abi/legacy.py --base 5fc9812` reports **0 differences
across five native-OFF configurations** for the one changed engine file;
`tests/fixes13/legacy.py --base 5fc9812` reports **0 across ten configurations**.
These complement the exact `unifdef`/`cmp` proof above.

The common native audio/voice API fixtures pass **2/2** under ASan/UBSan. This
is one Debug platform-fixture configuration; the production voice fixtures
and live runs above separately cover both engine modes. Exact commands are
also retained in `platform/commands.json`:

```sh
taskpolicy -b nice -n 19 cmake -S tests/platform -B build-macos/platform-tests \
  -DCMAKE_BUILD_TYPE=Debug -DCOD2_PLATFORM_SANITIZERS=ON
taskpolicy -b nice -n 19 cmake --build build-macos/platform-tests \
  --target macos_audio_test macos_voice_test --parallel 2
taskpolicy -b nice -n 19 ctest --test-dir build-macos/platform-tests \
  -R '^(audio|voice)$' --output-on-failure
```

Known red gates remain red, with unchanged acceptance logic:

- Release `tools/abi/legacy.py --base 664f1a1`: **10 differences** across 97
  files and five configurations. Five are generated `build/lp64_gen/data_native.c`
  differences excluded by CMake OFF, and five are test-only
  `tests/platform/server_control.c` differences.
- Release `tests/cod2x/check_inactive_gates.py --base 664f1a1`: **479 differences**
  across 98 files and 17 configurations. The brief's **436** is the historical
  red count. The starting branch's inherited count is **472**, derived by
  subtracting this unit's seven new file/configuration pairs. The full
  `g_client_mp.c` source was identical at `664f1a1` and `5fc9812`, and its seven
  current release mismatch pairs exactly equal the unit-only pairs;
  `abi/inactive-delta.json` records this proof. The pre-unit full gate was not
  separately rerun.
- Unit-only inactive check against `5fc9812`: **7 differences**, all in
  `g_client_mp.c`: two native stock configurations (Linux x64 and arm64 Mac),
  plus five synthetic configurations defining `COD2_X64=0`. Stock native
  intentionally receives the original-game repair; actual CMake OFF omits
  the definition. This explanation does not convert the check to green.

The full unrelated release merge-gate matrix is not rerun by this voice unit;
these passing fixtures/live checks are bounded claims. The brief's known red
legacy/inactive gates remain visible above and are not waived by a local source
comparison. The required x86 Linux multilib object/binary comparison cannot run
on this Mac. No human playtest is needed for the checks completed here.

## Cleanup and composition

Every build/heavy test command used `taskpolicy -b nice -n 19`; full builds used
`--parallel 2`. Two aggregate-concurrency deviations are recorded in
`scheduling-deviations.json`: an early fixture setup compilation overlapped
a two-job stock build, and the final two-job platform build (2.035 seconds)
started while serial release inactive-gate preprocessing was still running.
Either overlap could allow three compiler processes. The private audit wrapper
caps larger executors at two without changing comparisons or baselines.

The original ABI supervisor exited 143 after I stopped it at a private
log-opening FIFO before its legacy stage. No gate was running at that stop;
all 14 completed unit/audit receipts were retained. `verify_abi_tail.py`
completed the six remaining gates with the legacy executor capped at two.
`abi-supervisor-stop.json` records the controlled stop; `abi/commands.json`
records each actual gate result. No repository audit implementation or
acceptance baseline was changed.

Private compiled observers, probe dylib, fixture executables/objects and
symbol directories were removed after retaining hashes and receipts:
**62 compiled artifacts and 17 scratch directories**, including both observer
build directories and both full audit caches. `scratch-artifact-manifest.json`,
`scratch-directory-manifest.json` and `scratch-cleanup.json` record this;
`compiled-artifact-cleanup-check.json` finds zero remaining compiled QA
artifacts outside the worktree. Source drivers, logs, JSON and inspected JPEGs
remain in WS119 evidence. Engine and platform builds inside this worktree are
retained. The private scheduling FIFO was removed. No QA `.app` was created,
so no LaunchServices unregister was needed. All owned test/game processes
have ended, and the GUI lease was released.

Compose 02a4cba and this report commit onto the current release
lineage. Do **not** cherry-pick `5fc9812`: its command fix already exists as
`3b5b69b`. Private observer/driver changes remain only in WS119 evidence and
are not product telemetry. Natural microphone transmission remains outside
this completed synthetic relay proof; native microphone capture is still
unavailable, as documented by WS110. Nothing was pushed, tagged, released,
merged or written to another worktree or the real CoD2 Silicon data folder.
