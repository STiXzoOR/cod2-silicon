# WS79 — native CoD2x UDP operations and statistics

2026-10-07. Isolated native base
`4b63491863da216b03373239e822c30078597a0b`, branch
`codex/cod2x-udp-operations`. Read-only CoD2x reference
`/Users/stix/Projects/cod2-native-refs/CoD2x`, clean pinned HEAD
`d8c54695a5239ac99d1dfe96212b809b1a54bfee` (v1.4.6.8).
This implements WS69's current-release UDP operations contract. It does not
start 0.3 or change host revision, admission/authentication, identity, master
defaults, lifecycle ownership, update support, Config Studio or Metal.

## Verified reference contract

Read `src/shared/server.cpp:47–235,907–1024,1240–1246,1290–1301` and
`src/shared/cod2_dvars.h:24–29` at the pinned reference. The policy uses
16,384 address buckets, keyed by the four IP bytes rather than source port.
**All operations share that address table**, including rcon and disconnect.
Each class has its own global bucket:

| Operation | Address burst / drain period | Global burst / drain period |
| --- | --- | --- |
| getstatus, getinfo, getchallenge | 10 / 1000 ms | shared 10 / 100 ms |
| rcon | 10 / 1000 ms | independent 10 / 1000 ms |
| unknown-client disconnect reply | 2 / 1000 ms | independent 64 / 1000 ms |

The address check runs first. A global rejection still consumes an allowed
address burst. LAN sources bypass only the address check. Connect,
ipAuthorize, voice and other dispatch paths do not consume these buckets.
Direct master status generation remains outside the query dispatcher.

The integer leaky bucket drains `elapsed / period` requests, preserves the
fractional remainder, and accepts only while the remaining burst is strictly
less than the ceiling. It resets its time anchor when drained requests are
**greater than** the stored burst; equality retains the remainder. Ten queued
address requests therefore regain one allowance per second, not ten per second.
A full table refuses new addresses. A different address can reclaim a bucket
only after the current operation's `burst * period` window has been exceeded,
or after a backwards clock step. Disconnect has the reference's shorter
2000-ms reclaim window even though its address table is shared with queries.

`sv_rateLimiter` defaults true with changeable-reset flag `0x1000`.
`sv_rateLimiterRequestCount` and `sv_rateLimiterUniqueIPCount` are ROM (`0x40`)
integers with `[0, INT_MAX]` domains. Once at least 1000 ms has elapsed since
the last update, statistics count address entries whose aligned bucket time
is at most 1000 ms old and sum their stored bursts. Statistics do not
passively drain idle entries, track LAN addresses, or count only accepted
replies. The resulting numbers are the reference's recent-bucket approximation.
Disabling the policy stops consumption and statistics updates; it preserves
the existing rcon password handling and 500-ms throttle.

## Native implementation and activation

`cod2x_udp_operations.h` declares the three operations and server boundaries.
`cod2x_udp_operations_impl.h` independently implements private bounded state
and the integer policy; `sv_main_mp.c` includes it once. No new source object
or CMake change is needed, and the implementation participates in the ABI
auditor's normal header-cache digest. Its hash and index-based chains are
independently authored rather than copied from the reference. Storage is fixed
at approximately 324 KiB; no allocation or shared engine layout changes occur.

`SV_Init` registers/initializes the policy. `SV_ConnectionlessPacket` guards
the three queries. `SVC_RemoteCommand` guards rcon before the existing throttle
and password boundary. `SV_PacketEvent` guards only its unknown-client
disconnect reply. Both existing game-frame call sites refresh statistics,
with a shared once-per-second timer. Existing optional hardening query calls
are excluded in a native CoD2x build so a query cannot consume two unrelated
policies; disabled CoD2x policy also preserves unthrottled query dispatch.
The rest of network hardening retains its existing behavior.

Activation requires both numeric `COD2_X64` and `COD2_CODX` gates. Within that
build, the operator can toggle `sv_rateLimiter`. It is intentionally active
with the current hosted revision **zero**: UDP operational limits do not
advertise movement, animation, identity or admission authority. Discovery
remains protocol 118 under the unchanged revision-zero host policy. A compiled
remote CoD2x client does not become host revision authority through this unit.

Two explicit clock-safety choices preserve the policy without reproducing
reference timestamp hazards: modulo-32-bit elapsed arithmetic handles both
the signed millisecond boundary and unsigned rollover; a backwards step
larger than the positive signed interval resets the affected bucket. An
explicit active flag recognizes legitimate activity at time zero rather
than using the reference's `lastTime > 0` sentinel. The tests exercise both.
The original rcon throttle arithmetic is preserved; the clock rollover tests
exercise the new query/disconnect policy and statistics, not a rcon-clock fix.

## Behavioral red/green and actual Release compilation

The first production fixture was written and run before production edits:
baseline `4b63491` produced **45 behavioral failures per target**. The final,
expanded fixture produces **77 failures per target** on that same baseline.
They are missing registration, absent query/rcon/disconnect suppression and
statistics failures rather than compilation errors or sanitizer crashes.
Current stock and CoD2x fixtures report **zero failures** for both listen and
dedicated targets, including CoD2x with optional network hardening enabled.

The runner reads actual engine compile entries from the frozen
`/Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos/compile_commands.engine.json`
and `build-macos-codx/compile_commands.engine.json`, remapping source/include
roots to the owned worktree. It never edits those databases. It retains `-O3`
and the actual target flags. Fixture compilation adds `-UNDEBUG` and
ASan/UBSan; `--compile-engine` separately compiles the complete production
translation units with their original Release `-DNDEBUG` flags.

The final Release checks compile three complete server units × two targets
for stock and CoD2x: **12 production objects** with unchanged engine flags.
The explicit hardening variant adds **six production objects** with the
hardening feature definition. All compile successfully. No full-engine link
or server process is claimed by these object checks.

The real fixture bodies are `SV_Init`, `SVC_Status`, `SVC_Info`,
`SV_ConnectionlessPacket`, `SV_PacketEvent`, `SVC_RemoteCommand` and
`SV_RunFrame`. Dvars and LAN classification are production code. Cases cover
every protected operation; cross-operation and global sharing/separation;
multiple IPs and ports; LAN address bypass with global exhaustion; operator
disable/re-enable; refill just before/at the interval; drain equality with a
fractional remainder; full 16,384-address exhaustion; strict query/disconnect
reuse windows; backwards time, signed-boundary and unsigned rollover; time
zero; statistics cadence, approximation and nonnegative domains; valid-client
packet routing; disconnect response suppression; and untouched connect/auth/
voice dispatch. Explicit status/info checks catch optional hardening calls in
both enabled and disabled CoD2x mode.

Reproduction commands, run with a fresh output path each time:

```sh
taskpolicy -b nice -n 19 python3 tests/cod2x/udp_operations.py \
  --compile-db /tmp/cod2-ws79-engine/codx/compile_commands.engine.json \
  --baseline 4b63491 --output /tmp/cod2-ws79-reproduce-red
taskpolicy -b nice -n 19 python3 tests/cod2x/udp_operations.py \
  --compile-db /tmp/cod2-ws79-engine/codx/compile_commands.engine.json \
  --compile-engine --output /tmp/cod2-ws79-reproduce-codx
taskpolicy -b nice -n 19 python3 tests/cod2x/udp_operations.py \
  --compile-db /tmp/cod2-ws79-engine/codx/compile_commands.engine.json \
  --hardening --compile-engine --output /tmp/cod2-ws79-reproduce-hardening
taskpolicy -b nice -n 19 python3 tests/cod2x/udp_operations.py \
  --compile-db /tmp/cod2-ws79-engine/stock/compile_commands.engine.json \
  --compile-engine --output /tmp/cod2-ws79-reproduce-stock
```

Final behavioral evidence is outside git in `/tmp/cod2-ws79-complete-red`,
`/tmp/cod2-ws79-complete-codx`, `/tmp/cod2-ws79-complete-codx-hardening` and
`/tmp/cod2-ws79-final-stock`. Complete production compile commands and object
logs are in `/tmp/cod2-ws79-final-codx`,
`/tmp/cod2-ws79-final-codx-hardening` and `/tmp/cod2-ws79-final-stock`.
The remapped temporary databases retain the
frozen engine entries and use this owned worktree as their source root.

## Regression, ABI and inactive gates

- Existing production admission/auth/discovery fixtures: both CoD2x targets,
  zero failures. The advertised hosted revision remains zero/protocol 118.
- Existing production lifecycle fixtures: listen and dedicated CoD2x,
  zero failures. Their extraction seam conditionally includes the new policy
  only when the selected source contains its initialization boundary; policy
  headers are copied from that selected current/historical source as well.
- Existing small CoD2x identity/protocol/CD-key/FPS, runtime policy, demo,
  reconnect, immutable identity and animation helper suite: passes.
- OOB parser fuzzer: production parsing/query/rcon paths with ASan/UBSan,
  seed 79, **10,000 runs each** for stock and CoD2x, no crash. CoD2x resets
  the production limiter each input so earlier traffic cannot mask parsing.
  The harness supplies synthetic dvar registration and LAN services; the
  deterministic fixture separately exercises real dvars and LAN classification.
  The build now supplies the real native trace-state object rather than leaving
  a Mach-O weak reference unresolved. Parser outputs remain a synthetic sink.
- Focused cross-TU ABI audit: the three changed server units plus real dvar,
  clock, LAN and runtime definitions, both targets, **14 CoD2x translation
  units and 12 stock units; zero errors, zero mismatches**. This is a focused function-signature
  check, not the full retail import/callback/ABI gate owned by the parent.
- The focused CoD2x raw-offset gate reports one unreviewed key in the untouched
  `src/PC/universal/dvar.c`: `cursor = outBufferEnd - 1;`, two occurrences.
  The same baseline-source check against `4b63491` gives the identical
  seven-source/26-site finding. The manifest has no matching entry. This is a named gate failure; WS79 does
  not change that source or the reviewed manifest. Parent must review its
  existing source/layout evidence as part of the composed raw-offset gate.
- Inactive comparison against `4b63491`: four modified C files × seventeen
  configurations, **zero mismatches**. The new implementation also emits no
  tokens in four direct inactive preprocessor controls. Shared layouts and
  legacy source/object selection remain unchanged. `git diff --check` passes.
  These checks do not replace the Linux multilib byte-for-byte artifact gate.

The OOB campaigns are bounded local synthetic parsing checks, not public
flood testing or a performance benchmark. No engine/game/GUI/benchmark process,
licensed game input, external authenticated session or public listing test was
run. Simulation, packet readers/tokenizer, query info serialization, filesystem,
challenge/connect/auth, redirects and network output are synthetic in the
deterministic fixture. It does not execute the full `SV_Frame` loop; that
second statistics call site is covered by actual engine compilation, while
`SV_RunFrame` dynamically exercises the shared publishing boundary. Parent
owns independent review, composed full builds/ABI gates and runtime acceptance.

## Owned files and handoff

Production: `src/PC/server_mp/cod2x_udp_operations.h`,
`cod2x_udp_operations_impl.h`, `sv_init_mp.c`, `sv_main_mp.c` and
`sv_main_pc_mp.c`. Tests/docs: `tests/cod2x/udp_operations.c`,
`udp_operations.py`, the narrow `host_lifecycle.py` extraction adaptation,
`tests/fuzz/targets/oob.c`, `tests/fuzz/build.py`, `tests/cod2x/README.md`
and this report. All changes remain unstaged and uncommitted for parent review.
The primary historical checkout and the parent's tree/databases were untouched.
