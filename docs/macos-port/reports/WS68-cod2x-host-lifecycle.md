# WS68 — Bounded native CoD2x host lifecycle ownership

2026-10-07. Clean starting HEAD
`8a1cb60910d295b6ab0934b3c2478af70c5666d0`, branch
`codex/cod2x-host-lifecycle`, isolated tree
`/Users/stix/.codex/worktrees/cod2x-client-fixes/cod2-native`.
Read-only reference: `/Users/stix/Projects/cod2-native-refs/CoD2x`, verified
pin `d8c54695a5239ac99d1dfe96212b809b1a54bfee`, version 1.4.6.8.
Repository CONTRIBUTING instructions and WS56 were read. No third-party
implementation was copied. No game, benchmark, proprietary input, installation,
config migration, Metal/0.3 work, authentication service, network gameplay,
staging, commit or push was performed.

## Result and causal chain

This corrects the **bounded CR03 ownership/lifecycle defect**. It does not
complete CR03 hosted extension enablement or certify complete CoD2x hosting.
`COD2X_HOST_REVISION` deliberately remains **0**, independently of the remote
client's implemented `COD2X_REVISION` 6. Native host gameplay, its public version
dvar and its reliable client publication therefore agree on stock revision.

The initial fixture reproduced the causal gap: actual native `SV_Init` does
not register `g_cod2x` on base `8a1cb60`, because only client initialization calls
`Cod2x_Init`. The native public dvar then serves two roles: remote systeminfo
and reliable client commands change it, while shared prediction/server gameplay
reads it. `Cod2x_Disconnect` clears that public policy and both animation users.
A listen host therefore lacks an authoritative lifetime independent of its
client. Dedicated hosting lacks the registration altogether. Merely defaulting
the client dvar to 6 would activate and advertise unverified hosted contracts.

Pinned reference evidence establishes the required lifecycle shape:

- `src/shared/game.cpp:14–23`: shared registration, with dedicated version
  ownership enforced independently of a client.
- `src/mss32/hook.cpp:219–232` and `src/linux/hook.cpp:42–53`: shared/server
  initialization runs from server startup.
- `src/shared/server.cpp:1211–1215`: reassert version and stance fix activation
  before the original map-spawn body.
- `src/shared/server.cpp:1090–1116`: publish the version reliably after gameplay
  `ClientBegin`, including round/soft restart.
- `src/shared/animation.cpp:1374–1420`: activation changes blend and stance-event
  code paths; it is not a mutation of animation-table durations. Native blend,
  controller and stance-event branches already use the negotiated revision.
- `src/shared/server.cpp:1313–1315`: dedicated competitive policy remains
  writable by the host, while a listen client's public policy is protected.

## Implementation boundary

Shared policy registration now has a server initialization entry and remains
idempotent when client initialization follows. Dedicated initialization does
not read a machine identity or register client identity/features. The native
dedicated competitive dvar has the reference `0x1008` flags; the listen/client
policy keeps `0x18`.

Host lifetime is tracked separately from client active/demo state. Full spawn
establishes ownership after scriptinfo reset and before gameplay initialization;
soft restart does so through production `SV_InitDvar`. The version dvar domain
is locked to the selected hosted revision while owned. This prevents an internal
setter from publishing a revision above the implemented host contract. Shutdown
releases ownership and restores the remote negotiation domain 0–6.

Explicit systeminfo defaults, the generic systeminfo loop and the reliable
client dvar setter all share `Cod2x_SetClientDvar`. A hosted listen client's
version/competitive/FPS-policy updates cannot change host-owned policy, including
empty keys and case variations. Remote sessions keep the original typed dvar
setter, missing-key reset and demo behavior. Moving from remote play into a host
clears the previous remote competitive/FPS policy. Client disconnect restores
client limits and clears client animation user 0 while preserving host ownership,
host policy and server animation user 1.

All four native begin sites in `sv_client_mp.c` now use one publishing boundary:
enter-world, first usermove, same-map server-id recovery and test-client creation.
The original gameplay `ClientBegin` runs first; the reliable `v g_cod2x "0"`
command follows. This addresses the complete native source inventory, rather
than adding publication only to the restart/enter-world path.

No engine layout, disk/wire record, connect protocol, server admission decision,
identity derivation or stored config ownership changes in this unit. All new
production behavior requires positive native and CoD2x gates. Protocol remains
118. WS69 owns the separate protocol/admission/discovery implementation.

## Verification receipt

Private outputs are under
`/Users/stix/Library/Application Support/CoD2-native/ws68-*`.
The actual engine databases were read from
`/Users/stix/Projects/cod2-native/build-macos/compile_commands.json` and
`build-macos-codx/compile_commands.json`; their paths were remapped to the owned
tree. Compilation and fixtures ran with `taskpolicy -b nice -n 19`.

- **Red/green regression:** `host_lifecycle.py --dedicated --baseline 8a1cb60`
  fails at the explicit assertion that production `SV_Init` registers
  `g_cod2x`. The same production-body fixture passes with current source.
- **ASan/UBSan host fixtures:** CoD2x listen/client and clientless dedicated
  variants each report zero failures. They execute actual `SV_Init`, full
  `SV_SpawnServer`, soft/full `SV_MapRestart`, `SV_InitDvar`, `SV_Shutdown`,
  `SV_ClientEnterWorld`, `CL_SystemInfoChanged` and the reliable client setter
  with real production dvars and info parsing. They verify protocol 118 and its
  fixed domain, host domain locking/unlocking, registration identity, host0
  versus remote6, generic-loop protection, empty and case-varied policy,
  preservation of host competitive policy on disconnect, begin order/reliable
  text, and remote stock→6→3→stock/demo/disconnect transitions. Dedicated uses
  the actual dedicated compile entry.
- **Stock native control:** the production lifecycle fixture reports zero
  failures, creates no CoD2x dvar/identity and sends no revision command.
- **Production animation fixture:** all 144 grenade-stance cases and mounted
  controls pass under ASan/UBSan. Added checks prove a real client-only reset
  preserves authoritative user 1, and all-user reset clears it.
- **Mouse regression:** existing production MG registration/mouse/command and
  revision fixture reports zero failures after adapting its extraction seam
  to the new host state variable.
- **Existing small CoD2x runner:** identity/protocol/CD-key/FPS, runtime policy,
  demo/reconnect/immutable identity and animation helper fixtures pass.
- **Engine compilation:** 18 changed production objects compile with actual
  stock/client, stock/dedicated, CoD2x/client and CoD2x/dedicated target flags.
  This covers runtime, server init/client, client parse, reliable server-command
  dispatch and animation units wherever the target includes each source.
- **Inactive gates:** 8 C/header files × 17 configurations, zero mismatches
  against `8a1cb60`. `git diff --check` passes. This is a preprocessor control,
  not the Linux byte-for-byte i386 artifact gate.

Fixture map/script/frame/network services and HWID lookup are synthetic.
Systeminfo serialization is a fixture service; typed dvar and info parsing
are production. Every begin source site is checked to use the same production
publishing helper, but only enter-world is dynamically exercised as a begin
caller. Fixtures do not run the packet decoder/usermove/test-client pipelines,
an engine process, real GSC scripts, or authenticated admission. No full engine
link, live game, rendering, sound/PVS, latency/cadence or hit-geometry acceptance
is claimed here. Parent owns independent review, integration and the full gate.

## Hosted revision activation remains a separate decision

The **exact enablement point** is `COD2X_HOST_REVISION` in
`src/PC/qcommon/cod2x.h`. It is not a runtime/config switch, and changing the
remote client default is not an enablement mechanism. A reviewed change there
must be accompanied by candidate-revision integration tests and matching host
advertisement/admission policy. No evidence in this unit justifies selecting
3 as a fallback revision; the audited pin defines a current revision-6 contract,
and version 3 also selects a distinct prone-yaw path.

WS62/WS65 are present in the starting tree. The parent reports WS66 and WS69 as
separate implementation units. Once all are composed, the remaining activation
requirements beyond those implementations are concrete verification gaps:

1. **Shared movement at 0/3/4/6 with real negotiated/owned state.**
   `tests/movement/movement.c:70` hardcodes revision 3. It does not establish
   authoritative versus predicted backward displacement or stance-event
   suppression at the hosted revision and stock fallback across transitions.
2. **Unavailable-grenade event behavior at nonzero revision.**
   `tests/movement/weapons.c:35` returns 0 even in a CoD2x build, so production
   `bg_weapons.c`'s nonzero-revision suppression remains unexercised by that
   suite. Cover smoke/frag availability, the legitimate last-grenade path,
   offhand/fire transitions and event-ring competition/receiving-client delivery.
3. **Full animation revision agreement.** The grenade fixture covers timer
   ownership at 0/3/6 and the existing helper suite covers blend formulas.
   Neither proves complete production `BG_PlayerAngles`, `BG_RunLerpFrameRate`,
   controller/tag output and server/prediction agreement at 0/3/4/6, including
   revision-3-only prone yaw. These are required before treating stance
   activation as fully verified.
4. **Actual composed hosted lifecycle and admission.** This unit intentionally
   tests hosted zero. It does not exercise a hosted nonzero revision combined
   with WS69's actual discovery, extension/HWID admission, legitimate CD-key
   authorization, ban/GUID identity, reconnect and restart paths. A successful
   helper fixture or an integration compile alone cannot establish that contract.
5. **Acceptance that the reports already reserve for runtime.** Preserve WS65's
   second-client sound/event visibility and expiry checks, WS66's actual
   authoritative pellet/damage dispatch checks, and CR07's listen/dedicated
   snapshots/usercmd cadence at configured 40 server ticks. The reference does
   not support changing the default server tick rate unconditionally. No live
   evidence was collected here under the current no-game-runtime instruction.

These are verification/acceptance gaps, not newly observed bugs or a reason to
discard the implemented server fixes. Keep the release's remote CoD2x client
enabled; keep the native host's public revision zero until the composed contract
has the required evidence.

## Owned files and handoff

Production: `src/PC/qcommon/cod2x.h`, `cod2x_runtime.c`,
`src/PC/bgame/cod2x_animation.h`, `src/PC/client_mp/cl_parse_mp.c`,
`src/PC/cgame_mp/cg_servercmds_mp.c`, `src/PC/server_mp/sv_init_mp.c` and
`src/PC/server_mp/sv_client_mp.c` (the publishing helper and four begin sites).
Tests/docs: `tests/cod2x/host_lifecycle.c`, `host_lifecycle.py`,
`animation_stance.c`, `run_mouse_sensitivity.py`, `README.md`, and this report.
WS69's DirectConnect/GetChallenge/Authorize and discovery hunks are separately
owned; its `sv_init_mp.c` protocol include may need a mechanical duplicate-include
normalization when composed with this unit.

The pre-fix tree was clean and all changes remain unstaged/uncommitted for parent
review. Manual scope review preserved the explicit ownership boundaries and
inactive gates; parent performs independent review before integration/commit.
