# WS56 — Current-release CoD2x readiness audit

2026-10-07. Audited native base `3cdb7a3` on `codex/cod2x-readiness`.
Reference: read-only `/Users/stix/Projects/cod2-native-refs/CoD2x`, clean HEAD
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`, version 1.4.6.8. The orchestrator
independently checked the remote HEAD against this pin. All reference paths
and line numbers below refer to that exact checkout; native paths refer to
this report's base unless another Git object is explicitly named.

This is a bounded source and fixture audit for the **current release**. It
does not start 0.3 or Config Studio work. Only this report was written; no
production source, test, build configuration, private profile, or sibling
checkout was edited. No build, fixture compilation, game, screenshot, install,
authentication, network gameplay, or external review service was run. No
third-party implementation was copied. Findings below are source-backed
implementation gaps or missing evidence, not newly observed runtime failures.

## Result and release policy

Keep the CoD2x-enabled release and preserve intentional CoD2x behavior while
restoring original Mac 1.3 fidelity underneath it. The packaging helper already
uses `-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1` (`scripts/macos-common.sh:63`). Stock
and inactive-gate builds remain essential comparison controls.

The native remote client has substantial connection, gameplay, competitive,
demo, input, and convenience implementations. This audit found **two concrete
client additions absent from the old compatibility matrix and native source**:
the CoD2x item-configstring overflow guard and `sensitivity_MG`. Native hosting
also lacks several actual CoD2x gameplay fixes and the server lifecycle that
activates existing shared fixes. Thus WS10's client implementation receipt
does not establish complete CoD2x parity for a native listen/dedicated server.

The original-fidelity fixes already in this base—WS48 keyed-command masks,
WS50 environment/reverb, WS53 mantle weapon decisions, WS54 fog/objectives,
and WS55 script order/initial registration publication—should remain. None
contradicts the pinned CoD2x hooks inspected here. Their reports expressly
limit their proof to focused compilations, probes, and fixtures. WS52 collision
work and the uncommitted WS45 diagnostic/harness patch are independent work;
neither is counted as implemented or accepted by this base.

## Concrete implementation units

Priorities indicate current-readiness order, not an authorization to implement
inside this audit. P1 means a concrete client robustness issue or local gameplay
agreement gap; P2 means a missing enhancement or host contract to complete
after its prerequisites. Owners below are **exclusive production file sets**;
sequence the shared files instead of dispatching overlapping edits.

### CR01 — P1: guard item configstrings before registration

**Missing, direct source evidence.** CoD2x
`src/mss32/cgame.cpp:133–139` rejects CS_ITEMS strings with length at least
`0x108` before invoking original item registration. Its patch function hooks
both callers at `:252–253`. Native
`src/PC/cgame_mp/cg_weapons.c:900–906` has `char items[0x109]` and an unchecked
`strcpy` from configstring 8. The normal configstring-update dispatch calls
this production function (`cg_servercmds_mp.c:889`). A sufficiently long
server/mod item string can overflow the native stack buffer; the safe threshold
must follow the pinned guard, not simply truncate to the array's capacity.

**Owner:** `src/PC/cgame_mp/cg_weapons.c`; focused fixture under the existing
game/fixes13 tests. Implement the guard under native/CoD2x gates, with an
independently authored error. Test lengths `0x107`, `0x108`, `0x109`, and a
larger synthetic configstring through the production registration entry point;
the rejected cases must invoke no item/model registration. Run ASan/UBSan,
stock/native controls, and inactive legacy gates. This is a missing current
CoD2x fix, not a proposed feature or a claim that a live crash has occurred.

### CR02 — P2: implement the current CoD2x MG mouse sensitivity control

**Missing, direct source evidence.** CoD2x
`src/mss32/mouse.cpp:97–106` applies the MG sensitivity/acceleration factor
before the existing horizontal/vertical multipliers. `:151` registers
`sensitivity_MG` with default 1, range 0.01–100 and ARCHIVE/CHANGEABLE_RESET;
`:158` hooks the mouse call in command creation. Native
`src/PC/client_mp/cl_input.c:1278–1283` retains only the hardcoded 2.5/2.0
factors in MG state. A repository-wide source/test search found no
`sensitivity_MG`. Raw GameController delivery is not an equivalent for this
gameplay sensitivity rule. Defaults happen to agree when acceleration is zero;
nondefault MG sensitivity and acceleration do not.

**Owner:** `src/PC/client_mp/cl_input.c`, registration in `cl_main_mp.c`, and
the existing input fixture. Register and consume the real dvar; characterize
CoD2x's unconditional client hook before choosing its activation rule. Preserve
stock controls and the native finite-value protections. Test MG and ordinary
movement with independent sensitivity/acceleration values, cgame zoom factor,
zero frame time, smoothing, axis speed caps and mouse-to-strafe paths through
production `CL_MouseMove`/command creation. Do not label this implemented from
the raw-input or callback-rate tests.

### CR03 — P1 prerequisite: give hosted CoD2x gameplay its own lifecycle

**Integration missing.** CoD2x initializes its shared/server modules from
`src/mss32/hook.cpp:219–232` and Linux `src/linux/hook.cpp:42–53`.
`src/shared/game.cpp:14–23` registers the version and forces the implemented
revision on dedicated servers. `src/shared/server.cpp:1211–1215` sets revision
6 and enables stance corrections on map spawn, and `:1090–1096` reliably sends
the version at ClientBegin, including round/map restarts.

Native `Cod2x_Init` is called only by client initialization
(`cl_main_mp.c:1195–1197`); the version defaults to zero
(`cod2x_runtime.c:24–26,52–59`). The dedicated source set excludes client
initialization (`cmake/macos-arm64.cmake:54`). No native server lifecycle call
sets an implemented CoD2x revision or reliably publishes it on ClientBegin.
Native `SV_ClientEnterWorld` calls ordinary `ClientBegin`
(`sv_client_mp.c:646–661`). A listen client shares the same version dvar and
its disconnect unconditionally clears it (`cod2x_runtime.c:137–149`). Thus
merely compiling shared hooks into a dedicated/listen binary does not activate
their correction paths or establish stable server ownership.

**Owner:** `src/PC/qcommon/cod2x_runtime.c` and its interface; narrow server
registration/spawn/begin call sites in `sv_init_mp.c` and `sv_client_mp.c`.
Separate remote-client negotiated state from local-server authoritative state,
without widening shared layouts or pretending the omitted server fixes are
already revision 6 complete. Host enablement, stock control behavior and the
revision advertised to clients must agree. Verify clientless dedicated startup,
listen startup, begin, soft/full restart, local-client disconnect, remote
stock→CoD2x→stock transitions, and shutdown with real production lifecycle
functions. Preserve demo and remote competitive exemptions. This unit gates
CR04–CR07; it should not be solved by changing the client default to 6.

### CR04 — P1: restore hosted grenade throw stance cancellation

**Missing.** CoD2x `src/shared/animation.cpp:479–491` clears authoritative
`legsTimer` when a standing grenade throw persists into crouch/prone, or a
crouch throw persists after leaving crouch. It explicitly checks server-side
animation ownership. Native controller goals
(`src/PC/bgame/cod2x_animation.h:113–214`) have no counterpart. The actual
server caller is `src/PC/game_mp/g_active_mp.c:289–299`; separate animation
users are set at `g_main_mp.c:1055` (server) and `cg_main_mp.c:1181` (client).

**Owner:** `bg_animation_mp.c`/its helper and a narrow server playerstate
access boundary, coordinated after CR03. Add the name/stance checks on the
authoritative controller path only. Test both named throw animations and
unrelated animations across standing/crouched/prone flags, both animation users,
and extension zero/3/6. Prove the client controller cannot clear the server
timer accidentally. WS53's mantle weapon fix does not cover this throw/stance
timer rule.

### CR05 — P1: add hosted hidden-player sound broadcast policy

**Missing.** CoD2x `src/shared/server.cpp:1248–1275` counts in-use player
entities, then refreshes broadcast time for living players while the count is
at most the configured threshold. `:1311` registers
`sv_playerBroadcastLimit`, default 15, range 0–64; zero disables it. No such
dvar or frame policy occurs in native source. Native snapshot visibility already
consumes broadcast time (`sv_snapshot_mp.c:1446–1451`), and ordinary script
entity functions set it, but this does not implement the player policy.

**Owner:** `src/PC/game_mp/g_main_mp.c` after its real frame update, with
registration in the CR03 server owner. Check the actual snapshot consumer and
clock units, rather than copying the Windows layout or assuming all nonzero
values have the required lifetime. Fixture count semantics must include dead
in-use players in the threshold count while only living players receive the
refresh; cover 0, 15, 16, spectators/nonplayers, linked visibility and expiry.
Then prove ordinary fire/footstep events reach a second client across a PVS
portal. The remote client cannot reconstruct entities/events omitted by a stock
server. WS50 improves environmental processing and is independent of this
server-owned missing-entity problem.

### CR06 — P2: add hosted eight-pellet shotgun spread policy

**Missing.** CoD2x `src/shared/weapons.cpp:163–217` selects one pattern and
rotation per eight-pellet shot when its switch is enabled; all other cases use
the original random spread. `:249` registers `g_shotgun_spread_fix`, default
true, CHEAT/CHANGEABLE_RESET; `:259` installs the real spread hook. Native
`src/PC/game_mp/g_weapon_mp.c:587–599` independently calls the original random
endpoint generator for every spread pellet and has no dvar/pattern branch.

**Owner:** `g_weapon_mp.c` and CR03's server registration. Independently
implement from the behavior contract and preserve the established native lag
compensation/bullet recursion around this narrow branch. Characterize RNG
consumption and rotation, not only a screenshot of the pattern. Test eight
pellets with switch on/off, both variants/rotations, non-eight pellet counts,
weapon ranges and authoritative endpoint/damage dispatch. Original random
spread remains the stock/off-switch control. Existing grenade/mantle fixtures
do not prove pellet distribution or server damage.

### CR07 — P1/P2: complete hosted snapshot and connection contracts

**40-snapshot handling is missing (P1).** Native
`src/PC/server_mp/sv_client_mp.c:526–537` converts requests above 30 to 33 ms.
Pinned CoD2x `src/shared/server.cpp:323–338` clamps 1–40 and derives 25 ms at
40. The native client correctly permits 40, but a native host still limits it.
Native `sv_fps` already permits 10–1000 with default 20
(`sv_init_mp.c:450`); neither the reference source nor native client registration
justifies inventing a binary codec or asserting a new unconditional 40 tick
default. Configure/measure 40 server ticks in acceptance.

**Full host wire/admission identity is absent (P2).** Native server protocol
registration and info response remain 118 (`sv_init_mp.c:424`,
`sv_main_mp.c:599`) and DirectConnect checks only that ordinary protocol
(`sv_client_mp.c:1255` onward). Reference admission additionally checks extension
revision/HWID2 and derives the nonzero legacy GUID hash
(`src/shared/server.cpp:405–444`); reference discovery advertises 120 while
connect remains 118 (`src/shared/common.cpp:366–375`). Native absence is not a
failure of the remote client's implementation. It is a gap in claiming the
native host is a complete CoD2x server.

**Owner:** `sv_client_mp.c`, `sv_init_mp.c`, `sv_main_mp.c`, coordinated with
CR03. First add source-tested 1/30/40/41 snapshot handling under the actual host
policy and measure snapshots/usercmd cadence on listen and dedicated. Complete
host advertisement/revision/admission/identity as a separate reviewed unit if
full CoD2x hosting is required; do not advertise revision 6 before its gameplay
contract is implemented, change connect protocol to 120, or introduce any
authentication bypass. Master heartbeat/public-IP integration and UDP limiter
statistics remain additional host operations, not hidden client obligations.

### CR08 — P1: integrate WS47 corrections without changing stored config ownership

This base has not integrated WS47 (`0dab850`). It still registers
`cl_maxpackets` with minimum 15 (`cl_main_mp.c:1164–1166`) where original Mac
and competitive-disable CoD2x require 10; its CoD2x tokenizer remains the older
approximation (`cod2x_policy.c:137–184`). WS47's report and regression suite
document concrete dvar/parser, language, Quick Chat and registration corrections.
Bring in the independently supported current-readiness fixes and their fixtures;
preserve CoD2x's LOD exception, current native defaults 40/125, and competitive
125–250 policy. Reference `cgame.cpp:241–245` raises upper bounds, rather than
unconditionally selecting those defaults. Native 40/125 and this base's
frame-cap default **250** (`common.c:2291`) are deliberate release choices,
not new reference-parity evidence; WS10's historical 333 default is stale.

**Stored-data risk: park the profile mapping portion with Config Studio.**
Current `Cod2x_ConfigGame` maps direct and profile config requests to main
(`cod2x_policy.c:124–132`), and `FS_BuildOSPath_Internal` flattens the qpath to
`config_mp.cfg` (`com_files.c:854–858`). WS10 deliberately delivered and
verified that single shared `<fs_homepath>/main/config_mp.cfg` release path.
Pinned CoD2x actually preserves the active profile's qpath while changing only
the game folder (`src/shared/iwd.cpp:710–724`), so WS47 is correct about the
reference difference. Nevertheless, `0dab850` removes the flattening and bare
config mapping without migration from the already-shipped shared file. That can
make a release appear to lose settings or start a fresh config. A source-correct
path assertion does not resolve existing stored-data ownership.

**Owner/split:** keep WS47's `com_playerprofile.c` create/delete use of writable
`fs_homepath`; park only `Cod2x_ConfigGame` mapping changes and removal of the
two flattening lines in `com_files.c`. Retain the tokenizer changes in the same
`cod2x_policy.c` file. Adjust WS47 profile assertions/report to the maintained
release contract, so an untouched assertion expecting profile qpaths does not
misreport this intentional adaptation as a failure. No private profiles or disk
migration were examined here. Future profile migration, cross-mod active files
and Config Studio serialization are expressly outside this audit/current fix.

## Implemented behavior versus evidence still required

The following traces establish implementations, not a blanket compatibility
certification. Fixtures and reports were inspected; their earlier PASS receipts
were not rerun in this unit.

| Behavior | Active native evidence | Remaining proof / classification |
| --- | --- | --- |
| Browser 118/120; connect 118; revision/HWID2; legitimate CD-key digest | `cl_main_pc_mp.c:226–235,630–634`; `cl_main_mp.c:1918–1954`; `cod2x_protocol.c:6–23`; `cod2x_runtime.c:52–76` | Implemented remote-client contract. Existing identity/connect/resend fixtures model boundaries. Authenticated admission, same identity across reconnects, real master browser and error handling need an owned reference server/CD key. |
| New server policy/systeminfo/reliable cvar handling | `cl_parse_mp.c:143–151,197`; `cg_servercmds_mp.c:476–486,1096–1104`; frame/disconnect at `cl_main_mp.c:2550,2760` | Implemented client hooks. Test actual packet/systeminfo and reliable-command transitions including same-map restarts, demo and missing keys; helper setters alone cannot prove dispatch. Hosted ownership remains CR03. |
| Competitive FPS, rate/snaps/packets, shadows/FX/Miles-file policy; wait | `cod2x_runtime.c:87–135`; `common.c:1779–1781`; `cmd.c:56–64` | Implemented: preserved domains/types, retained applied values and demo exemption. `test_runtime.c` supplies synthetic dvars; characterize through real dvar setters/registration and actual frame/wait timing. Legacy `com_maxfps_limit` remains README-inferred compatibility. |
| Stance classification/timing, posture, alignment, ladders, version-3-only prone yaw | `cod2x_animation.h:59–214`; `bg_animation_mp.c:733–734,984–987,1022–1086` | Formula/timing branches reviewed against `animation.cpp`. Keep current CoD2x overrides. `test_animation.c:7–8,54` uses simplified types/helper calls; it does not execute full production `BG_PlayerAngles`, `BG_RunLerpFrameRate`, controllers, tag output or hit geometry. |
| Displacement-based backward movement and stance-event suppression | `bg_pmove.c:1159–1209,1313–1342` | Implemented shared prediction branches. `tests/movement/movement.c:70` fixes CoD2x version at 3; add 0/3/4/6 end-to-end coverage before claiming every negotiated version. |
| Unavailable smoke/frag event suppression | `bg_weapons.c:1838–1845` | Implemented when game version is nonzero; source keeps the legitimate final-grenade path. Current `tests/movement/weapons.c:35` returns version zero even for the CoD2x compile, so WS53 passing in both modes does **not** exercise this suppression. Test event-ring competition with actual offhand/fire transitions and a receiving client. |
| Pool 4096, large info strings, IWD selection, write switch | `universal/dvar.c:12,39`, `q_shared.c:585–622`; `cod2x_policy.c:27–121`; `com_files.c:1702,1887–1898`; `common.c:743–755,2292` | Implemented source helpers/dispatch; WS10 production fixtures cover bounded cases. Real pure-server lists, restart/same-checksum mod selection and disable/toggle persistence remain acceptance; profile mapping is the explicit CR08 adaptation. |
| Demo recording/upload/quit deferral; match login | `cl_main_mp.c:1145,2553,2764,3324–3420`; `cod2x_demo.c`; `cod2x_features.c:76–79` | Implemented client workflows; local HTTPS/queue fixtures are not actual match/upload service acceptance. Backend lifecycle/stats are reference server/GSC responsibilities. |
| Radar, orbit, colors, spectated name, compass, bullet/pose diagnostics | `cod2x_features.c:65–133`; `cg_view_mp.c:258`; `cg_newDraw_mp.c:1710–1735`; `cg_draw_mp.c:1679`; `cg_weapons.c:1063`; `cod2x_pose.c` | Native source equivalents and production-helper fixtures exist. Rendered positions/materials, permission transitions, calibration, objective/spectator behavior and visible pose/hitbox agreement still need runtime evidence. No pixels were inspected here. |
| Raw mouse/statistics, URL handling, watchdog/crash reports, window/audio | `src/platform/cod2x_native*`, `macos_rawmouse.m`, native display/audio/voice adapters | Platform equivalents, not Windows injection parity. Physical mouse delivery/routing, focus, audio quality and URL launch remain distinct acceptance. MG sensitivity is CR02. |

`mss_q3fs` is **not currently an unconsumed placeholder**: `snd_driver.c:1427`
selects the callback stream path, `:1702–1704` installs file callbacks, and
`macos_audio.c:852–877,895–901` stores and consumes them. The old WS4/WS10
uncertainty about this policy should not be repeated as a new missing fix.
WS50's wet-buffer/environment corrections strengthen the native path, but
audible footsteps, voice interoperability and Miles/EAX acoustic parity still
need their own evidence.

## Coverage closure before release acceptance

1. Compose the current fidelity fixes, narrowly selected WS47 corrections and
   prioritized client/host units. Freeze the exact source commit under test;
   previous reports do not certify a later composed binary.
2. Run current CONTRIBUTING gates for stock and CoD2x, plus the dedicated
   source/link/runtime controls. The WS10 historical dedicated-link failure
   is not asserted to persist: current CMake has dedicated typed-data handling
   (`macos-arm64.cmake:70–85`). This audit ran no link and cannot establish
   dedicated readiness either way.
3. Strengthen the CoD2x regressions at real callers: version 0/3/4/6 controllers
   and tag output, reliable/systeminfo policy changes, smoke/fire event ring,
   competitive dvar domains through real registration/setters, and dedicated/
   listen lifecycle. Keep independent reference expectations rather than simply
   testing a helper against its own current formulas.
4. On the orchestrator-owned live session, perform stock→CoD2x→stock join/rejoin,
   authenticated admission, restart/round transition, demo exemption, 40-tick/
   40-snapshot/125-packet cadence and observer/server hit-model comparison for
   stand/crouch/prone/lean/ladders/mantles/grenade throws. Use CR05/CR06 controls
   to attribute hidden-player sound and shotgun behavior to the local server.
   Re-run the fog/objective/configstring startup cases through real maps.
5. Retain original i386 preprocessing and Linux byte-equality obligations.
   CoD2x corrections are intended native feature behavior; original-fidelity
   repairs must not silently revert those active corrections.

The blanket matrix wording is historical inventory. New rows are needed for
CR01/CR02, and existing server rows should explicitly say **missing native host
implementation** rather than implying that remote-server ownership satisfies
native hosting. Only mark a row accepted when its source hooks, relevant
regressions and required runtime evidence agree.

## Remaining host breadth and explicit exclusions

Beyond CR03–CR07, pinned CoD2x registers custom GSC player/match methods and
HTTP/WebSocket functions (`src/shared/gsc.cpp:32–77`) plus the pre-stop gametype
callback (`:96–97`). Native source has no matching extension registration.
Some mods can require these on a listen/dedicated server; remote clients merely
consume server-driven menus/configstrings. Treat script API compatibility as a
separate host audit/unit, not evidence that the client must add a private match
backend now. Server heartbeat/public-IP/third-master and UDP rate-limiter
operations likewise remain missing host capabilities; this bounded audit did
not validate every server management/GSC implementation.

Windows binary updater/replacement, DLL injection/hot reload, automatic mod
download-and-run/update paths, registry/VirtualStore/elevation, and Windows
CPU-mask mechanics remain the existing exclusions or native platform adaptations.
They are not gameplay fidelity fixes. External match/vMIX/backend integration
remains unimplemented or unaccepted optional service work; do not call it complete
from `match_login`. Destructive cleanup of the user's IWDs remains excluded.
None of those exclusions excuses the concrete shared/client/server gameplay
gaps identified above or authorizes starting 0.3.

## Audit receipt

Read CONTRIBUTING, `cod2x-compat.md`, WS4/WS10, WS43, WS48/WS50,
WS53–WS55 and WS47 from commit `0dab850`. Traced pinned Windows/Linux/shared
module initialization and installed animation, weapons, cgame, competitive,
mouse, IWD, network and GSC hooks against active native source and fixture
boundaries. Source-wide name searches support the missing-dvar/hook findings;
they are not exhaustive proof of every function's correctness.

Only this report is added and it is left **uncommitted**. Whitespace/report-path
verification is the only execution validation for this audit; no production
PASS claim or release approval is made.
