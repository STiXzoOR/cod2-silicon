# WS69 — CoD2x host protocol, admission and identity

2026-10-07. Native base `8a1cb60`; read-only reference
`/Users/stix/Projects/cod2-native-refs/CoD2x`, clean pinned HEAD
`d8c54695a5239ac99d1dfe96212b809b1a54bfee` (1.4.6.8).
This unit addresses WS56 CR07 current-release contracts. It does not start
0.3, Config Studio, Metal, match services or platform updater work.

## Source contract and bounded design

The reference's discovery protocol is **120**, while a `connect` userinfo
still has **118** (`src/shared/common.cpp:366–375`,
`src/shared/server.cpp:388–402`). Native `SV_Init` registers 118 and native
`SVC_Info` hardcodes it (`sv_init_mp.c:424`, `sv_main_mp.c:599`). `SVC_Status`
and `SVC_GameCompleteStatus` use the serverinfo dvars. All three responses and
the serverinfo `protocol` must agree with the authoritative host revision.

The extension admission fields are `protocol_cod2x >= 6` and `cl_hwid2` of
exactly 32 bytes (`src/shared/server.cpp:405–434`). A newer revision is
accepted. HWID2 admission checks **length only**; the reference does not
require lowercase hex. The native client's own lowercase-hex validation is
not the server admission rule. Apply these requirements only when the native
host owns a complete supported extension revision; compiled CoD2x client
support alone is not server authority.

The legacy server GUID becomes the 32-bit FNV-1a hash of the exact HWID2 bytes,
with zero replaced by one (`:437–465`). Preserve signed 32-bit bit patterns
for existing `getGuid`, `ban.txt` and temporary bans without pointer punning.
For a remote client, locate the actual address/challenge entry before changing
its GUID or clearing a rejected entry. Ban rejection targets the incoming
address, including local clients; do not reproduce the reference's
uninitialized challenge index in its local-address ban path. Internal bots
retain a zero identity and never receive a fabricated hardware identifier.
The reference bypasses its wrapper for the internal bot connect path
(`server.cpp` hooks DirectConnect from the connectionless dispatcher, while
the original bot function is unhooked).

Authorization remains independent of HWID. The reference accepts replies only
from its authorization server and distinguishes accept, deny, demo and invalid
responses (`:640–781`). Its optional `sv_cracked` path replaces a denial with
acceptance (`:688–707`); that behavior is explicitly excluded here. Its normal
LAN exemption and five-second authorization timeout predate this unit and
must not be presented as verified CD-key admission (`:844–886`). This unit
does not infer authorization from an HWID hash or a client-supplied digest.

There is a concrete digest forwarding gap: the native client sends its real
CD-key MD5 digest in `getchallenge` token 2, and native `SV_AuthorizeRequest`
can encode `PB "<digest>"`, but native `SV_GetChallenge` supplies NULL
(`cl_main_mp.c:1978–1985`, `sv_client_mp.c:246–258,1130`). The reference
captures token 2 before authorization and retains it (`server.cpp:836–842,
886`). Preserve the digest in a private native sidecar keyed to the actual
challenge slot, address and challenge value; never widen `challenge_t` or
`client_t`. Forward only a valid 32-hex digest to avoid injecting untrusted
text into the authorization request. Missing or malformed digest follows
the existing request path and creates no accepted status.

The native server uses authorization port 20966 (`sv_client_mp.c:1121`),
whereas both the native CoD2x client and the pinned host use 20700
(`cl_main_mp.c:1895`, reference `shared.h:30–31`). The bounded native CoD2x
branch selects 20700; inactive feature/native gates retain the original path.
This is source-backed routing, not a receipt from a live service.

The implementation owner is exclusively admission/authorization portions of
`sv_client_mp.c`, discovery responses in `sv_main_mp.c`, and the protocol
registration expression in `sv_init_mp.c`, plus a private helper, tests and
this report. The separate CR03 worker owns runtime/host lifecycle and all
ClientBegin callsites in those same source files. Root serializes integration.

## Readiness and dependencies

CR03 provides server-owned revision and lifecycle APIs. Its readiness constant
stays **0** until the combined implementation and evidence support enabling
revision 6. Consequently current production discovery stays 118 and extension
admission remains inactive; bounded production fixtures exercise the revision
6 path with explicit simulated host state. A remote-client version 6, reliable
cvar, demo or client disconnect cannot activate local host advertisement.

Before changing that readiness constant, root must combine CR03 lifecycle,
CR04 authoritative grenade/stance cancellation, CR05 player broadcast,
CR06 shotgun distribution, CR07 snapshots/admission/discovery and existing
shared correction paths. WS64 already supplies the 1–40 snapshot interval
branch in this base; it is not reimplemented here. Required host validation
includes dedicated/listen start, soft/full restart, host shutdown, stock and
CoD2x client transitions, challenge mismatch, ban rejection and a legitimately
authenticated join with an owned key/reference server. Source fixtures cannot
certify the last item or measured snapshot cadence.

## Other host capabilities and optional services

Native dedicated heartbeat support already has five configurable masters,
180-second heartbeat and 600-second status periods
(`src/platform/macos_server.c:66–130`), so WS56's blanket missing-multiple-master
wording is stale for this path. Its default second master is empty; the pinned
host defaults master2 to `master.cod2x.me`, with optional master3
(`server.cpp:1293–1295`). Listing in the CoD2x master still needs a default or
configured master2 and real acceptance. The pinned public-IP `getIp`/
`getIpResponse` path is an operator convenience, not a player wire requirement
(`:518–530,995–1004`). Native listen hosting uses the separate original master
path; it does not send public dedicated heartbeats. Updater packets remain
platform/service exclusions.

Pinned UDP limiting covers getstatus/getinfo/getchallenge/rcon/disconnect,
with limiter dvars and statistics (`:907–919,957–983,1017–1024,1299–1301`).
Native optional `COD2_FEATURE_NET_HARDENING` limits info/status/rcon paths;
it does not establish equivalent default coverage or statistics. Audit/implement
those operations separately before claiming complete server-management parity.

Pinned local GSC methods `getHWID`, `getCDKeyHash`,
`getAuthorizationStatus`, view/stance/turret methods and the pre-stop gametype
callback are local mod contracts (`gsc.cpp:32–39,96–97`). Their absence is
not excused by optional external backend work. The identity accessor methods
need private per-client persistence; `getAuthorizationStatus` must expose
actual reply state, never an invented `KEY_IS_GOOD`
(`gsc_player.cpp:55–104`). HTTP/WebSocket and match methods are additionally
required by mods that call them (`gsc.cpp:63–76`); the corresponding private
match service is optional integration. This bounded CR07 implementation must
not claim either complete GSC support or backend acceptance from `match_login`.

## Validation receipt

The bounded implementation is complete in the assigned isolated tree,
uncommitted. All new production code has both `COD2_X64` and `COD2_CODX`
gates. No shared layout, runtime/client setter or ClientBegin callsite changed.
The discovery/registration/admission branch depends on the separate CR03
header/runtime API; root must integrate that unit first. No production
readiness constant is raised here.

Existing tests inspected: CoD2x identity/runtime and info fixtures, online
CD-key hash/getchallenge resend, WS64 server snapshots, player broadcast
production extraction, and inactive-gate checks. Added
`tests/cod2x/run_host_protocol.py` and `test_host_protocol.c`, which extract
the actual host functions using each real engine target's compile flags and
link the real info-string/formatting implementation. Their host version is
explicit fixture state, not an accepted production identity.

- Before implementation, the initial fixtures failed nine intended assertions
  on each CoD2x listen/dedicated target. Re-running the expanded fixtures
  against `8a1cb60` fails 27 expected assertions per target (final baseline
  receipt `/tmp/ws69-host-protocol-final-baseline`). The initial
  fixture setup had compile/link errors before reaching this behavioral red
  result; those are not counted as proof of a production failure.
- Final ASan/UBSan fixtures pass with zero failures for stock and CoD2x,
  listen and dedicated: `/tmp/ws69-host-protocol-final-codx` and
  `/tmp/ws69-host-protocol-final-stock`. They cover revision 0/6, connect
  118/120, extension 0/5/6/7, HWID length 31/32/33, non-hex identities,
  signed hashes, an independently calculated FNV zero vector clamped to 1,
  local/bot identity, exact address/challenge data, malformed/missing and
  uppercase digests, bans/expiry, authorization source/challenge matching,
  deny/demo/invalid replies and denial preventing ClientConnect, late accept
  replies preserving HWID, and all three discovery/status replies.
- All 12 changed server translation units compile using actual stock/CoD2x
  listen/dedicated flags. Local compile receipt
  `/tmp/ws69-host-source-compile2` temporarily supplies
  `-DCOD2X_HOST_REVISION=0` because the separate CR03 header has not yet been
  integrated. This is not full link evidence; root will compile/link the
  composed tree with its real header and no temporary definition.
- `check_inactive_gates.py --base 8a1cb60`: three source files, 17 inactive
  configurations, zero mismatches. This preprocessing comparison does not
  replace the original i386 binary equality gate.
- Unchanged online `cdkey_hash` and `challenge_resend` production fixtures
  pass under both stock and CoD2x flags with ASan/UBSan. Existing SDK MD5
  deprecation/size-conversion warnings remain. `git diff --check` passes.

To reproduce, run the new host fixture with `--compile-db` pointing to either
real `compile_commands.engine.json` and a fresh private `--output` directory;
add `--baseline 8a1cb60` for characterization. Prefix execution with
`taskpolicy -b nice -n 19`. No game, GUI, live authorization or reference
server has been run. No third-party implementation is copied; the hash uses
the public FNV-1a algorithm and the wire contract above.

## Concrete host follow-up contracts

The numerical revision is a client negotiation contract; the wire cannot
express whether a host implements optional mod callbacks, directory defaults
or operator statistics. For this project's requested **full current-release
CoD2x hosting**, the three local/host units below are mandatory parity work,
not exclusions justified by an unavailable match backend. Under the current
readiness policy, keep revision 0 until root decides the composed mandatory
contract and its validation are sufficient. Source inspection establishes no
reason that these units need a private external backend.

### Local GSC identity and player methods — mandatory full host parity

**Files:** `g_client_script_cmd_mp.c` for guarded method implementations,
`g_scr_main_mp.c` for extension method lookup, `sv_client_mp.c` for private
per-client metadata capture/clear, and a new native-only identity metadata
module/interface if needed. Do not enlarge `client_t`, `challenge_t`,
`scr_data_t` or the legacy builtin tables. `Player_GetMethod` is currently
case sensitive (`g_client_script_cmd_mp.c:1691–1704`); add a guarded extension
lookup with the reference's case-insensitive canonical-name behavior after
the original resolvers (`g_scr_main_mp.c:2635–2665`, reference
`gsc.cpp:124–145`).

**Contract:** `getIp` returns dotted IPv4; `getHWID` returns the actual admitted
32-byte userinfo identity, empty for internal bots; `getCDKeyHash` returns the
client's supplied digest, which is explicitly not evidence of authorization;
`getAuthorizationStatus` returns the actual authorization reply's reason,
empty for a local/LAN connection lacking a reply. Retain those values from the
matched challenge into the accepted client metadata, clear on slot reuse/free
and host shutdown, and preserve on legitimate reconnect/map restart only as
the inspected lifecycle requires. Never synthesize `KEY_IS_GOOD` after LAN
exemption, timeout, missing digest or HWID admission. Complete `getViewOrigin`
through real `G_GetPlayerViewOrigin`, `getStance` with crouch-before-prone
precedence and strings `crouch`/`prone`/`stand`, and `isUsingTurret` using the
actual authoritative entity MG flag (reference `gsc_player.cpp:108–168`).

**Tests:** extend the actual script binding/compiler/VM fixtures under
`tests/lp64/script/` and `tests/lp64/game/script_api.c`; prove the named GSC
calls compile, resolve and produce VM values, not merely that a C helper
returns a string. Add a host metadata chain from real GetChallenge/Authorize/
DirectConnect into script accessors: accepted/denied/timeout/LAN/bot cases,
two client slots, reconnect, slot reuse, free, restart and shutdown. Verify
invalid entity/class/argument handling follows the real player method boundary,
all values use synthetic identities, and stock/inactive builds retain their
original symbol/layout behavior. This builds on CR03 and WS69; it is not
implemented by the digest challenge sidecar alone.

### StopGameType callback — mandatory when a host mod supplies it

**Files:** `g_scr_main_mp.c` for optional callback-handle lookup plus a private
native handle, `sv_ccmds_mp.c` for map/devmap/map_restart/fast_restart/map_rotate
entry points, `sv_init_mp.c` for spawn/shutdown orchestration, and narrow
`g_main_mp.c` script-origin transition context. Load
`maps/mp/gametypes/_callbacksetup::CodeCallback_StopGameType` as optional;
the existing native loader only reads original callbacks
(`g_scr_main_mp.c:4602–4616`). Calling it only in `G_ShutdownGame` is too late:
that function destroys entities and the script system (`g_main_mp.c:1298–1325`).

**Contract:** before the operation mutates the running level, invoke the
callback once with arguments `(fromScript, bComplete, shutdown, source)`;
source is `map`, `fast_restart`, `map_restart`, `map_rotate` or `shutdown`
(reference `server.h:14–21`, `gsc.cpp:212–257`). Integer zero cancels a map
transition/restart; undefined/non-integer does not. Shutdown ignores the veto.
Clear output parameters, preserve persistent-round semantics, and suppress
duplicate callback invocation when map_rotate invokes map internally
(`server.cpp:1126–1147`). Audit the one-time permitted level-change mechanism
used by service deferral separately before introducing it. Absence of the
optional callback must leave stock scripts loading and map operations intact.

**Tests:** extend `tests/fixes13/script_load.c` for absent/present callback
lookup and the real script VM fixtures for return interpretation. Add
production command/spawn/shutdown fixtures recording call order, all five
sources, script/console and complete/persistent variants, zero/undefined/
non-integer returns, canceled devmap cheat state and map_rotate nesting.
Prove cancellation precedes entity/script destruction, shutdown runs exactly
once, and ordinary stock controls remain identical. A live synthetic mod
should exercise one allowed and one vetoed transition after fixtures pass.

### Default CoD2x master and UDP operations — mandatory host operations parity

**Master files:** `src/platform/macos_server.c` default master registration
and `sv_main_pc_mp.c` only if shared listen/operator commands require it.
Keep native dedicated's existing five configurable masters. Under the actual
host policy, default master1 remains Activision, master2 becomes
`master.cod2x.me:20710`, master3 remains optional; preserve operator overrides,
empty suppression, explicit ports, DNS refresh, public-dedicated-only
heartbeat/status and flatline shutdown. A stock or incomplete host still
advertises 118. Adding the master must not alter the authorization endpoint
or authority. `getIp` is an optional operator convenience: if included, trust
only a configured master's `getIpResponse` and validate the response shape.

**Master tests:** extend `tests/platform/server_control.c` to assert the
actual defaults and packet recipients/payloads, both fixed masters plus a
configured third, DNS failure/name changes, explicit ports, empty entries,
the 180000/600000-ms timers and shutdown. Follow with an owned public dedicated
server's real master listing receipt; local UDP fixtures cannot certify
external directory acceptance.

**Limiter files:** independently authored policy/state in a native CoD2x
network helper, `sv_init_mp.c` registration, `sv_main_mp.c` connectionless
dispatcher/disconnect response path, and `sv_main_pc_mp.c` rcon boundary.
Coordinate with existing `qcommon/net_hardening.c` rather than enabling its
different global policy implicitly. Register `sv_rateLimiter` true and ROM
nonnegative `sv_rateLimiterRequestCount`/`sv_rateLimiterUniqueIPCount`.
Reference address/global `(burst, period-ms)` arguments are `(10,1000)` /
`(10,100)` for shared getstatus/getinfo/getchallenge; rcon uses its own global
`(10,1000)`; disconnect uses `(2,1000)` / `(64,1000)`
(`server.cpp:957–983,1017–1024`). Characterize refill from the actual leaky
bucket algorithm, not its informal rate comments. LAN bypass applies to the
address bucket; auth/connect processing is not silently rate-limited by these
query buckets. Statistics refresh at most once per second and count recent
active address buckets/sum of their decaying bursts, an approximation rather
than a precise request counter (`:189–235`). Disabling the policy preserves
original dispatch and does not manufacture authenticated state.

**Limiter tests:** extend actual `tests/fuzz/targets/oob.c` dispatcher
coverage with deterministic-clock production fixtures for every protected
action, shared versus independent global buckets, multiple source IPs/ports,
LAN, burst exhaustion/refill, disabled mode, bucket reuse, clock rollover,
stats update cadence/approximation and disconnect suppression. Run ASan/UBSan
and stock/inactive gates; ensure the existing hardening option does not consume
two unrelated buckets for one packet. UDP fixture traffic is sufficient for
local dispatch behavior; public flood testing is unnecessary.

HTTP/WebSocket and match GSC methods remain a separate, explicit mod/service
API unit. A mod that invokes them needs those methods; inability to contact a
private match service must be represented as a real failure/unavailable result.
Actual match backend, uploads and vMIX service acceptance are optional external
integrations, not prerequisites for local identity/callback/limiter/master
implementation. Windows injection/updater/registry mechanics remain platform
exclusions; none authorizes omitting the local units above.

## Parent composition receipt

WS68 was composed as `9b5a0d3` after bounded WS69 commit `abc159c`. The single
merge conflict was two equivalent spellings of the CoD2x header include; the
shared source-root include was retained. The protocol runner now consumes the
actual shared revision definition instead of a temporary command-line zero.
Parent reruns pass stock/CoD2x admission/auth/discovery for client and dedicated
flags, plus the composed real-dvar host lifecycle fixture. Actual unchanged
Release commands compile six stock and eight CoD2x affected server/runtime
translation units with no temporary revision definition. These are focused
composition checks; complete links, host6 activation and live authentication
remain unclaimed. Private receipt: `CoD2-native/ws69-composed-*` and
`CoD2-native-protocol/validation.noindex/ws69-composed/`.
