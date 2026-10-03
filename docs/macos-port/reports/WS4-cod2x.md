# WS4 — CoD2x client compatibility

Branch: `port/cod2x`. Worktree: `/Users/stix/Projects/cod2-native-wt/cod2x`.
Reference: CoD2x 1.4.6.8, commit `d8c54695a5239ac99d1dfe96212b809b1a54bfee`.
Completed source implementation and native logic verification on 2026-10-03.
The game cannot run on this Mac yet, so **live admission, authentication,
rendered animation/hitbox agreement and packet cadence are unverified**.

## Delivered

The full behavior inventory is in `../cod2x-compat.md`: source locations, side,
connection/play/optional classification, effort and discrepancies between the
README and current source. No reference implementation, binary, game archive or
decompiler dump was imported. The behavior was reimplemented in typed C; short
comments identify reference locations. No updater or download-and-run path was
added. Optional match/demo upload/vMIX/radar/URL/IWD features are not implemented.

Enable the feature using the existing convention:

```sh
cmake -S . -B build/cod2x -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1
```

This command configures the current upstream target set; it does not establish a
working macOS executable. After WS1 is merged, use its native macOS target.
`COD2_CODX` is absent by default. `COD2_CODX=0` also removes every integration.
CMake omits the three new translation units entirely when disabled, including
empty object/debug inputs, and adds IOKit/CoreFoundation only when enabled.

### Connection

- The browser accepts advertised protocols **118 and 120**, queries both
  Activision and CoD2x masters for both versions, and preserves explicit
  `debug_protocol` behavior. Both connect packets still send **protocol 118**:
  CoD2x `src/shared/server.cpp:388` rejects 120 in the connect userinfo.
- ROM USERINFO `protocol_cod2x=6` and `cl_hwid2` accompany normal stock fields.
  Identity/revision are refreshed before each connect so an internal cvar command
  from an earlier server cannot cause another machine's identity to be sent.
  `cl_cod2x_version=opencod2-cod2x-1.4.6` identifies the independent implementation;
  admission checks numeric keys, not display branding.
- HWID2 is the first 128 bits of SHA-256 over
  `opencod2:cod2x:hwid2:v1:<lowercase IOPlatformUUID>`, encoded as 32 lowercase hex
  characters. IOKit queries this machine's IOPlatformExpertDevice; no random ID,
  registry transplant, spoofing or raw UUID transmission. Missing/invalid UUID
  leaves an empty ID and prints a diagnostic; CoD2x admission will fail while the
  existing stock connection path remains available. The identity backend is
  macOS-specific; no Linux/Windows identity backend was invented.
- `getchallenge 0 <PBHASH>` provides the reference server's optional token-2
  CD-key hash. The existing 1.3 `getKeyAuthorize ... PB <PBHASH>` path also gets
  the completed hash. Stock challenge/auth response handling remains in place.
  Inspection found the baseline hash builder returned an empty string.
- **PBHASH is seeded MD5**, not standard MD5. The stock seed is 0xB684A3,
  modifying the four standard IV words with multipliers 11/71/37/97. Verified
  against the Mac MD5Init and Windows IV constants in the read-only decompiles;
  implemented through the SDK's public CommonCrypto context fields. HWID2 uses
  a separate SHA-256 algorithm. Auth success still needs a live server and a
  valid owned CD key; no auth bypass was added.
- The pure connect encoder produces the original `connect "<userinfo>"` payload,
  excludes its terminal NUL from the transmitted length, and preserves the OOB
  framing supplied by NET_OutOfBandData. Tests decode the added text keys,
  negative challenge and qport=65535. No new binary netfield/codec was found or
  introduced. Movement yaw remains within the existing signed 8-bit playerstate
  field and ordinary entity angles2[1] representation.

### Correct play

- Gated registration defaults/limits allow `snaps 40` and `cl_maxpackets 125`.
  `sv_fps 40` is remote-server configuration: the existing opencod2 registration
  already permits 10..1000. This client does not alter a remote server's tickrate.
- `g_cod2x` from SYSTEMINFO and the existing reliable server cvar command selects
  runtime corrections. Disconnect resets the version, policy, and controller
  state. Stock servers select version 0 and use the existing gameplay branches.
- Current `g_competitive` policy restricts FPS to 125..250 (out-of-range becomes
  250), forces rate=25000/snaps=40/maxpackets=125/sc_enable=0/fx_sort=1/mss_q3fs=1,
  and blocks `wait` once primed. The original domains/types are restored when
  disabled, disconnected or playing demos. Applied values remain, matching the
  reference. Native audio may not consume mss_q3fs; WS3 owns that backend.
- README's older `com_maxfps_limit` has no occurrence in the current reference
  client/server sources. Its described 125..250 policy is supported separately
  as a server-controlled, cheat-protected boolean. It restricts the actual frame
  cap and dvar domain. This is legacy compatibility inferred from the README,
  not a verified current-version wire requirement.
- Runtime extension >=3 uses expanded crouch classification, 200 ms crouch
  transitions, 232 ms crouch→prone blend with 400 ms controller transition,
  prone/lean/reload/fire posture offsets, torso/weapon alignment, immediate swing,
  ladder head correction, timed controller interpolation, and stance bounce.
  Extension 3's older prone-leg yaw rule is confined to version 3. Movement yaw
  follows actual displacement and only reverses BACK input when truly moving
  backward. Four scripted stance events are suppressed while corrections apply.
  The server/client animation users have separate typed state, reset on tree
  change, time rewind and disconnect. No pointer-size assumption was added.
- Holding an unavailable smoke/frag no longer floods the four-event ring with
  EV_EMPTY_OFFHAND on CoD2x; the legitimate final-ammo event remains. Version 0
  preserves the stock branch.
- The **PVS sound correction is server-owned**: a CoD2x server broadcasts living
  player entities below its playerBroadcastLimit. This client consumes those
  ordinary snapshots/events; it cannot synthesize omitted stock-server entities.
  Grenade stance-lock cancellation and consistent shotgun spread are also
  authoritative server fixes. This workstream does not turn an opencod2 listen
  server into a CoD2x server; those server-only implementations were not added.

## Verification evidence

```sh
./tests/cod2x/run.sh
```

Passed all three native binaries with `-Wall -Wextra -Werror`: identity/protocol/
seeded CD-key hash/connect encoding; runtime policy/reconnect/demo handling; and
stance/controller calculations. No engine or game data is needed. Tests build
with plain clang and SDK frameworks and remove their temporary binaries.
All three also passed separate clang builds/runs with
`-fsanitize=address,undefined`; animation builds use `-ffp-contract=off`.

Evidence strategy: no tests directory existed at baseline. Initial new-helper
checks failed because implementations were absent; the connect encoder check
failed with an undefined symbol before implementation. Correct seeded digest
vectors failed against the initial standard-MD5 implementation and passed after
correction. Runtime/controller fixtures were added after source implementation,
with syntax checks and reference inspection as the initial characterization;
no red-before-implementation observation is claimed for them. These tests cover
pure behavior, not the engine ABI, real visual poses or live authorization.

CMake configurations all succeeded:

```sh
cmake -S . -B .ws4-check/cmake-off -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS= -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake -S . -B .ws4-check/cmake-on -DCOD2_X64=ON \
  -DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake -S . -B .ws4-check/cmake-32 -DCOD2_X64=OFF \
  -DCOD2_FEATURE_CFLAGS= -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Compile-command/link inspection found zero new CoD2x objects/frameworks when
OFF, all three new objects with COD2_CODX=1 plus IOKit/CoreFoundation when ON.
The 32-bit configuration retains its original architecture flags. No complete
32-bit binary byte comparison was possible here: even a tiny `clang -m32` link
fails against this SDK/libSystem. The orchestrator/WS5 must run the actual 32-bit
reference build comparison. Source-line/debug metadata can differ after edits;
this report does not claim whole debug binaries were compared byte-for-byte.

For all eight modified pre-existing C units, clang preprocessing with gate absent
and with `-DCOD2_CODX=0` matched `git show port/main:<path>`, after minimizing
whitespace and normalizing only `cod2_off_assert_<LINE>` / `cod2_size_assert_<LINE>`
typedef identifiers generated from source line numbers. No runtime token difference
was found. No shared engine header or data blob changed.

Raw engine syntax check remains blocked by existing Darwin typedef/struct
redefinitions in cod2_defs.h:

```sh
clang -arch arm64 -std=gnu11 -fsyntax-only -DCOD2_X64=1 -DCOD2_CODX=1 \
  -Isrc -Isrc/headers src/PC/qcommon/cod2x_runtime.c -ferror-limit=5
```

A temporary header copy removed only conflicting reconstructed Darwin types
(`__darwin_suseconds_t`, pthread mutex typedef/opaque structs/handler record,
`__darwin_ino_t`, `rlim_t`, `rlimit`) and neutralized x86 regparm attributes.
With those local guards and `-D_FORTIFY_SOURCE=0`, all nine engine units passed:
bg_animation_mp, bg_pmove, bg_weapons, cl_main_mp, cl_main_pc_mp, cl_parse_mp,
cmd, common, and cod2x_runtime. The command shape was:

```sh
clang -arch arm64 -std=gnu11 -fsyntax-only -DCOD2_X64=1 -DCOD2_CODX=1 \
  -D_FORTIFY_SOURCE=0 -I.ws4-animation-check -I.ws4-animation-check/headers \
  -Isrc -Isrc/headers -Wno-duplicate-decl-specifier \
  -Wno-incompatible-pointer-types -Wno-int-conversion <translation-unit>
```

Those temporary headers/binaries/build directories were removed and never
committed. WS1 owns the real header fixes. `git diff --check` passed. No system
packages were installed; no required tool was missing. No push, issue, PR, remote
change or access to sibling worktrees occurred.

## Live verification once the integrated client runs

1. Build gate OFF and ON. Launch the ON client with an owned stock 1.3 install
   and valid CD key, for example `./cod2_macos +set com_maxfps 250 +set rate 25000
   +set snaps 40 +set cl_maxpackets 125`. First connect by IP to a stock 1.3
   server, then CoD2x 1.4.6.8, then stock again; repeat via the browser. Capture
   OOB requests: both master protocols, connect protocol=118, revision=6, stable
   32-character cl_hwid2, normal challenge/qport/password keys. Confirm the server
   derives its GUID from HWID2 and a reconnect keeps the same identity.
2. Use a non-cracked CoD2x server and valid owned CD key for internet auth. Inspect
   getchallenge token 2, the getKeyAuthorize PB field, challengeResponse,
   connectResponse and needcdkey/error handling. Also test LAN challenge flow.
   Do not infer authenticated success from LAN or a cracked server.
3. Configure the server `sv_fps 40`; inspect actual 25 ms snapshots. At client
   250 FPS/snaps40/maxpackets125, measure packet cadence and play a full round
   without snapshot/usercmd decode errors. Compare ordinary snapshot fields with
   stock; no new binary layout should appear. Confirm movement yaw -90..90,
   including BACK input while sliding forward, prone turns and diagonal movement.
4. Toggle server `g_competitive`. Try com_maxfps=0,85,125,200,333; verify 125..250
   and forced network/render settings, rejected wait, restoration of domains
   after disable/disconnect, and demo exemption. Have a server script send the
   legacy com_maxfps_limit cvar command separately; confirm only FPS policy is
   forced. The reliable g_cod2x update at ClientBegin/map restart must work too.
5. With a reference CoD2x observer, compare stand↔crouch↔prone transitions,
   crouch/prone peeking, left lean and diagonals, moving-prone reload/fire, ladder
   pitch/yaw, grenade throw followed by crouch/prone, and server hit model. Repeat
   stock→CoD2x→stock and demo/time rewind; no correction state may leak.
6. Hold +smoke and +frag with zero offhand ammo while firing. The other client
   must keep hearing/seeing fire events and empty-offhand events must not flood
   the ring. Keep the legitimate final-grenade event functional.
7. Put two clients across a PVS portal. With server sv_playerBroadcastLimit=15
   and a small player count, verify hidden-player shots/footsteps. Repeat with
   broadcast disabled and above the threshold to attribute behavior to the
   server. Grenade stance cancellation and shotgun spread must match server
   behavior, not fabricated client events.

## Merge handoff

Implementation commits: `cb20d84` (identity/protocol tests), `fad8a5c`
(connection/policy), `83a6fdf` (animation/smoke). Documentation follows in the
final commit. Merge the focused commits in order. Existing changes are confined to three bgame
units, three client units, common/cmd, and a 12-line CMake stanza near COD2_SRC_DIR.
New helper headers live under PC/qcommon and PC/bgame; **src/headers is untouched**.
Keep the gated source omission and frameworks when resolving WS1's CMake changes.
Retain `-ffp-contract=off` from the port build. Enable the gate explicitly on the
native target; keep it absent for the 32-bit reference comparison.

The implementation workstream is complete locally. Remaining integration gates
are the native engine/header/data migration, actual 32-bit binary comparison,
and every live procedure above. Unit fixtures and temporary syntax guards are
not evidence that the full client links, boots, joins either server, or reaches
250 FPS. The orchestrator owns those integrated checks and merges; this branch
has not been pushed.
