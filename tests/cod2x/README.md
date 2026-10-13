# Native CoD2x logic tests

From this worktree on macOS:

```sh
./tests/cod2x/run.sh
```

The runner uses plain Apple `clang` and SDK frameworks, creates temporary binaries
inside this directory, and removes them on exit. It does not need the engine,
game data, system package installation, or a server.

- `test_cod2x.c`: advertised 118/120 selection, stable 32-character HWID2,
  canonical UUID hash vector, actual IOKit identity lookup, seeded CD-key digest
  vectors, stock connect-packet framing and text-field decoding, FPS limits.
- `test_runtime.c`: actual policy adapter with a minimal dvar fixture; checks
  immutable connection identity, stock-server fallback, competitive settings,
  demo exemptions, legacy FPS restriction and restoration on disconnect.
- `test_animation.c`: actual controller helper with typed fixtures; checks
  expanded crouch classification including bit 39, first-leg initialization,
  stance blend durations, prone posture/reload/fire offsets, diagonal alignment,
  stance bounce, separate animation users and time-rewind reset.

The authoritative grenade stance fixture needs an actual native engine compile
database and a private output directory. Run it for both stock and CoD2x builds:

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/animation_stance.py \
  --compile-db build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/ws62-stock"
taskpolicy -b nice -n 19 python3 tests/cod2x/animation_stance.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/ws62-codx"
```

`animation_stance.c` exercises the full production animation translation unit,
the production `G_PlayerController` body, and the production server playerstate
stride calculation under ASan/UBSan. It checks actual timer writes for exact
standing/crouch throw names, unrelated and near-matching names, null legs with
a throw on the torso, standing/crouched/prone flags, both animation users,
revisions 0/3/6, and mounted controls. A nonzero client index and neighboring
sentinels check that only the selected player's legs timer changes. The version
and animation ownership are synthetic inputs; this fixture does not establish
the pending CR03 host revision lifecycle. `--baseline <commit>` reads older
production source without changing the checkout.

The bounded host lifecycle fixture uses the real dvar implementation and
production server registration, map spawn, soft/full map restart, shutdown,
client enter-world, systeminfo, and reliable dvar setter bodies:

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/host_lifecycle.py \
  --compile-db build-macos-codx/compile_commands.json \
  --output "$HOME/Library/Application Support/CoD2-native/ws68-host-client"
taskpolicy -b nice -n 19 python3 tests/cod2x/host_lifecycle.py \
  --compile-db build-macos-codx/compile_commands.json --dedicated \
  --output "$HOME/Library/Application Support/CoD2-native/ws68-host-dedicated"
taskpolicy -b nice -n 19 python3 tests/cod2x/host_lifecycle.py \
  --compile-db build-macos/compile_commands.json \
  --output "$HOME/Library/Application Support/CoD2-native/ws68-host-stock"
```

It checks clientless dedicated registration without identity lookup, native
protocol 118, host revision zero/domain ownership, remote revision-6 and empty
systeminfo, reliable updates, disconnect, restart, begin publication, and remote
stock/CoD2x/demo transitions. The dedicated variant selects the actual dedicated
compile entry. The original gameplay begin must precede its reliable revision
command. A source check requires all four begin sites to use that same boundary;
only enter-world is dynamically exercised as a begin caller. The production
grenade fixture also checks that client animation reset preserves server user 1.

Services for maps, gameplay scripts, networking, frame execution, identity and
systeminfo serialization are synthetic. There is no engine process, licensed
input, or authenticated admission. The hosted extension remains revision zero;
these tests establish lifecycle ownership, not complete CoD2x hosting. Add
`--baseline 8a1cb60 --dedicated` with a new output directory to reproduce the
missing dedicated registration before this correction. See WS68 for the
remaining revision activation requirements.

The UDP operations fixture executes production server registration, query
responses, connectionless dispatch, invalid-client packet handling, rcon and
frame statistics, with real dvars and the production LAN classifier:

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/udp_operations.py \
  --compile-db build-macos-codx/compile_commands.engine.json --compile-engine \
  --output "$HOME/Library/Application Support/CoD2-native/ws79-udp-codx"
taskpolicy -b nice -n 19 python3 tests/cod2x/udp_operations.py \
  --compile-db build-macos/compile_commands.engine.json --compile-engine \
  --output "$HOME/Library/Application Support/CoD2-native/ws79-udp-stock"
```

Add `--hardening` and a fresh output path to check that native CoD2x packets
consume one policy when optional network hardening is also enabled. Replace
`--compile-engine` with `--baseline 4b63491` to reproduce the missing behavior. The fixture checks
shared address/query buckets, independent global query/rcon/disconnect buckets,
LAN/global behavior, ports, disabled mode, exact refill/expiry boundaries,
16,384-address exhaustion/reuse, clock rollover, nonnegative statistics and
the original rcon password/throttle boundary. `--compile-engine` also compiles
the three complete changed server units for both targets with their unchanged
Release flags; fixtures enable assertions and ASan/UBSan separately.

Clock, packet readers/tokenizer, game/challenge/connect/auth, redirect and
network output services are synthetic here. The existing OOB fuzzer retains
the real packet/tokenizer/query/rcon parsing paths and resets production
limiter state for each input. Neither suite runs a server process or certifies
public traffic capacity. Statistics count recent address buckets and sum
their already-drained bursts, including globally rejected packets; they are
an approximation, not an accepted-packet total. See WS79 for the pinned
contract and release evidence.

The standalone helper fixtures model only the fields needed for their
calculations. These fixtures do not verify engine layout, networking,
rendering, real hitboxes, or admission to a
live authenticated server. The report at
`docs/macos-port/reports/WS4-cod2x.md` records the engine syntax-check boundary
and the live-server test procedure.

The HWID2 domain is `opencod2:cod2x:hwid2:v1:` followed by the lowercase canonical
UUID. The first 16 bytes of SHA-256 become lowercase hexadecimal. Changing this
rule changes this machine's server identity; keep it stable. Tests never print
or store the actual UUID or machine ID.

The CD-key hash uses the stock seeded MD5 initial state and uppercase alphanumeric
normalization of at most the first 32 source characters. The fixed vectors were
cross-checked with a separate MD5 round implementation. This legacy protocol
hash is distinct from the HWID2 hash.

For the full native-client workstream, run:

```sh
sh tests/cod2x/run_full.sh
COD2X_SANITIZERS=1 UBSAN_OPTIONS=halt_on_error=1 sh tests/cod2x/run_full.sh
python3 tests/cod2x/check_inactive_gates.py --base b834082bc95cf5fdb44f62bffa0f31dad21a6592
```

The full runner first repeats the identity test with the engine's SDK-named
function/data stubs linked in (`test_identity_sdk.c`), checking that installed
framework lookup survives those symbol collisions without changing the hash
domain. It then adds IWD/config selection, tokenizer and extraction fixtures;
4096 registrations and exhaustion through the production dvar allocator; the
production big-info helper; visual commands, colors, orbit and radar geometry;
production radar buffer ownership and cheat-protected pose controls; raw-input
statistics and production `IN_Frame` routing; safe URL parsing and actual native
AppleEvent/bundle metadata; demo playback restoration, protected recording,
persistent upload queues and deferred quit.

It also posts owned fixture bytes to an ephemeral local HTTPS server using the
production libcurl uploader. A test-only CA override trusts the fixture; a
hostname mismatch must deliver no bytes. Nothing changes the system trust store.
The extractor tests use synthetic archives. Licensed release validation and
real-client startup/IWD/shared-config checks are recorded separately from live
server admission and physical input/rendering in
`docs/macos-port/reports/WS10-cod2x-full.md`.

The normal runner exercises a real 12-second watchdog stall, disable/heartbeat
cases and SIGABRT reports, taking about 45 seconds. Sanitizer mode excludes those
deliberate signal cases. Its added C fixtures use ASan/UBSan; the unchanged WS4
runner and standalone HTTPS subprocess retain their original compile flags.
The fixtures and temporary app bundles are created under the system temporary
directory and removed on exit. The inactive-gate comparison checks preprocessed
source bodies against integrated main, including explicit zero-valued macros;
it cannot prove byte-level 32-bit binary parity.

The bounded authoritative shotgun fixture uses an existing engine compile
database and a new private output directory:

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/run_shotgun_spread.py \
  --compile-db /absolute/path/to/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/shotgun-new" \
  --release-tus
```

Repeat with the stock and CoD2x databases and distinct output directories. It
uses ASan/UBSan and the complete production weapon source to check eight-pellet
patterns, rotation, RNG state, weapon ranges, hip/ADS fire dispatch, actual
trace/trigger/damage processing, stock/off/non-eight controls, anti-lag
restoration, glass recursion, and normal server dvar registration. Trace and
registration services are synthetic; live hitboxes, collisions, networking,
and full CoD2x host lifecycle remain outside this fixture. `--baseline <commit>
--skip-registration` characterizes the spread path before the switch exists.
See `docs/macos-port/reports/WS66-cod2x-shotgun-spread.md` for its evidence bounds.

The bounded production gameplay revision matrix selects remote revisions
0/3/4/6 through the real runtime and typed dvars, then repeats the gameplay
checks under the current release's actual hosted-zero ownership:

```sh
taskpolicy -b nice -n 19 python3 tests/movement/run.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/movement-matrix-new" \
  --revision-matrix
taskpolicy -b nice -n 19 python3 tests/movement/weapons.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/weapons-matrix-new" \
  --revision-matrix
taskpolicy -b nice -n 19 python3 tests/cod2x/animation_stance.py \
  --compile-db build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/animation-matrix-new" \
  --revision-matrix
```

Repeat with the stock database and distinct private outputs. The movement
matrix preserves every original fixture, including 20,000 complete player-state
prediction/server comparisons per selection, and adds independent backward
displacement/velocity/yaw and stance-event expectations. The weapon matrix
preserves WS53 and runs unavailable smoke/frag competition and valid final
throws through production event-ring insertion, player/entity conversion and
receiving-client event delivery. The animation matrix runs the complete native
animation TU: angle calculation, lerp frames/rates and controller/tag output for
both animation users, including revision-3-only prone yaw and stock mounted
flag handling. `--baseline <commit>` on the animation runner can reproduce the
mounted-mask regression before WS74.

Collision worlds, weapon/animation definitions, script services and XAnim/DObj
endpoints are synthetic. Event delivery uses in-memory entity state, without
packet serialization or network transport. No nonzero hosted revision is
enabled or certified by these fixtures. See
`docs/macos-port/reports/WS74-production-gameplay-matrix.md` for reference
contracts, the bounded stock correction and remaining runtime acceptance.
