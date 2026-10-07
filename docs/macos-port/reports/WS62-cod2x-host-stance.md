# WS62 — CoD2x authoritative grenade stance timers

2026-10-07. Bounded implementation of WS56 CR04 on native base `380bf2b`.
Behavioral reference: read-only CoD2x HEAD
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`, version 1.4.6.8.
The implementation was independently authored from the source-backed rules;
no third-party implementation was copied.

## Result and source evidence

The native CoD2x controller now clears the authoritative player's `legsTimer`
when a standing grenade throw persists into crouch/prone, or a crouch throw
persists after leaving crouch. This is staged source coverage for CR04;
**CR03 host revision activation and publication remain pending**.

The reference correction is in `src/shared/animation.cpp:479–491`, inside the
revision-at-least-3 block at `:294`. It checks server animation ownership via
`animation_isServer` (`:83–85`) and a non-null legs animation. Its exact names
are `pb_stand_grenade_throw` and `pb_crouch_grenade_throw`. The first is canceled
when entity crouch or prone is set; the second when crouch is clear, including
prone. `src/shared/cod2_entity.h:17–18` defines crouch as `0x4` and prone as
`0x8`. The reference returns for mounted flags before reaching this block
(`animation.cpp:148–149`).

The native change is confined to `bg_animation_mp.c` and its
`cod2x_animation.h` helper. The timer rule runs through the existing
revision-at-least-3 controller branch, checks `bgs->anim_user == 1`, and uses
the existing `SV_GameClientNum(es->clientNum)` playerstate boundary. Native
server initialization sets animation user 1 (`g_main_mp.c:1055`); client
initialization sets user 0 (`cg_main_mp.c:1181`). The actual authoritative
caller is `G_PlayerController` (`g_active_mp.c:289–299`), which installs
`level_bgs` and selects its client information using `self->s.clientNum`.
Playerstate-to-entity conversion copies the player client number
(`bg_misc.c:479`), and server client information is assigned that client number
(`g_client_mp.c:854–856`). Server startup registers the client storage and
`sizeof(gclient_t)` stride (`g_main_mp.c:1094`); `SV_GameClientNum`
(`sv_game_mp.c:137–140`) uses that registered storage and stride.

The new source disappears when either native or CoD2x is inactive. There are
no signature or layout changes, new revision defaults, or server revision
advertisements. Existing controller formulas, revision 3 behavior, ladder
goals, and blend/state tracking are preserved.

## Verification

`tests/cod2x/animation_stance.py` inherits the actual engine compiler flags,
enables assertions and ASan/UBSan, and runs `animation_stance.c`. The fixture
includes the full production animation translation unit and helper, the
actual `G_PlayerController` body, and the actual `SV_GameClientNum` stride
calculation. The server lookup wrapper asserts that the client controller
never enters the server boundary. DObj services and revision/animation-world
inputs are synthetic; the fixture never supplies the timer result.

- Proof first: stock baseline `380bf2b` passed. CoD2x baseline failed at the
  actual `legsTimer` assertion before either production file was changed.
- Current stock and CoD2x: each passed 144 cases plus two mounted controls
  under ASan/UBSan. Cases cover both exact throw names, unrelated and
  near-matching names, null legs with a torso throw, standing/crouch/prone
  and combined crouch/prone flags, client/server animation users, and
  revisions 0/3/6. Client 5 uses real `gclient_t` storage; neighboring legs
  timers and every torso timer retain sentinels.
- Actual Release compilation: `bg_animation_mp.c` compiled for client and
  dedicated targets in both stock and CoD2x modes with unchanged engine
  flags and private output objects.
- Scoped ABI: `bg_animation_mp.c`, `g_active_mp.c`, and `sv_game_mp.c`, across
  client/dedicated commands, yielded six TUs per mode, zero errors and zero
  mismatches. Raw-offset checks for the affected client TU in each mode
  reported zero sites and zero unreviewed/unsafe entries.
- Legacy guards: two changed production files, five native-off
  configurations, zero mismatches; two actual SDK commands with native
  removed, zero mismatches/errors. CoD2x inactive-gate checks: two files,
  17 configurations, zero mismatches. Explicit unifdef comparison: two
  production files, four inactive-gate configurations, zero differences.
- Existing `test_animation.c` helper assertions passed under ASan/UBSan.
  `git diff --check` passed.

All fixture and compiler outputs are private under
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/ws62/`.
Compilation ran serially with `taskpolicy -b nice -n 19`; the engine compile
databases were remapped into this worktree and private output directories.
No shared build outputs, game processes, benchmarks, global settings, or
licensed assets were changed.

## Limits and integration dependency

The fixture supplies revisions 0/3/6 and animation ownership at a synthetic
authoritative boundary. It does not execute host startup, ClientBegin,
restart, disconnect, or shutdown negotiation. The existing shared controller
still reads `Cod2x_GameVersion`; WS56 CR03 must separate remote-client state
from local-server authoritative revision state and activate/publish only the
implemented host policy. This work does not make native hosting revision 6
complete. The staged CR04 unit can compose with that lifecycle after its
contracts are ready.

No live listen/dedicated gameplay, throw animation rendering, hitbox behavior,
or original Linux i386 artifact comparison was run. The preprocessing checks
prove inactive source preservation, not byte-identical legacy objects or live
hosting acceptance. This work starts no 0.3, Metal, or Config Studio changes.
