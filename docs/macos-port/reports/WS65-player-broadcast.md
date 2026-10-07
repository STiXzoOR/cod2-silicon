# WS65 — Hosted CoD2x player broadcast policy

2026-10-07. Implemented CR05 from WS56 on native base `380bf2b`, branch
`codex/cod2x-player-broadcast`, isolated checkout
`/Users/stix/.codex/worktrees/config-catalog/cod2-native` (historical directory
name only). Independently read the clean pinned CoD2x reference
`d8c54695a5239ac99d1dfe96212b809b1a54bfee`; no third-party implementation was
copied. This bounded current-release unit does not start 0.3, Metal or Config
work. Changes remain unstaged and uncommitted for the parent to integrate.

## Contract and implementation

The reference `src/shared/server.cpp:1239–1279` calls the original frame first,
counts all 1024 entities with a client, in-use state and ET_PLAYER type, then
sets living matching entities' broadcast time to the server clock plus one
while the count is at most the configured positive limit. Dead in-use players
count toward the threshold. There is no team, connection or spectator check:
an ET_PLAYER spectator matches; a spectator represented as a non-player entity
does not. The implementation intentionally follows those entity semantics.
The reference registers `sv_playerBroadcastLimit` at `:1311`, default 15,
integer domain 0–64, flags `DVAR_CHANGEABLE_RESET` (`0x1000`); zero disables
refreshing.

Native production edits are confined to `src/PC/game_mp/g_main_mp.c`:

- Register that dvar in `G_RegisterDvars_impl`, used by real `G_InitGame`
  startup and exposed through `G_RegisterDvars`. It works in both listen and
  dedicated source sets; no client-only initialization is needed.
- Refresh at the end of the complete `G_RunFrame`, after client end-frame,
  registration publication and animation diagnostics. Read `svs.time`, the
  consumer's clock, and use typed entity fields with no shared layout changes.
- Gate declaration, registration, helper and call with positive `COD2_X64`
  and `COD2_CODX`. Stock and every inactive feature configuration retain their
  prior production behavior.

**CR03 lifecycle is not a prerequisite for this particular policy.** Both
reference platform startup hooks initialize `server_init` unconditionally
(`src/linux/hook.cpp:42–53`, `src/mss32/hook.cpp:219–232`), and its frame policy
does not inspect `g_cod2x`, negotiated revision, match state or animation
activation. Native game dvar registration occurs before the hosted game's
frames, so this self-contained registration/caller path supplies the same
activation contract. CR03 remains separate work for version ownership and
other gated fixes. No revision is fabricated or advertised here; admission,
remote-client version state and lifecycle were not changed.

## Clock and visibility evidence

Native `sv_main_mp.c` increments `svs.time` by `frameMsec` before
`G_RunFrame(svs.time)`, then sends client messages after simulation. Ordinary
script broadcast expiry already uses millisecond lifetimes
(`g_scr_main_mp.c:4046`: `level.time + 300`). Thus reference `+1` is one
millisecond, not one tick or second. The server's existing time wrap safeguard
at `sv_main_mp.c:962` precedes the signed integer boundary used here.

The real native predicate `sv_snapshot_mp.c:1433–1452` requires linking and
excludes the viewing client's own entity before checking broadcast time.
Nonzero broadcast time takes priority over hidden flags, client masks, PVS
and fog. It clears the time only when it is strictly less than `svs.time`:
the endpoint is inclusive. It then returns visible even on the clearing
observation; a subsequent observation uses the ordinary tests. This retained
consumer behavior means zero/over-limit transition stops refreshing without
immediately clearing existing script/policy timestamps. No snapshot consumer
change is part of WS65. The fixture records this nuance explicitly.

## Verification receipt

Private outputs are under
`/Users/stix/Library/Application Support/CoD2-native-protocol/validation.noindex/ws65-broadcast/`.
Compilation used `taskpolicy -b nice -n 19`. Engine flags come from the actual
`{stock,codx}/compile_commands.engine.json` under the same validation root;
source/include roots were remapped to this checkout and outputs kept private.

- New dedicated `tests/cod2x/run_player_broadcast.py` and
  `test_player_broadcast.c` exercise production registration, complete frame
  caller, policy, and actual visibility predicate with ASan+UBSan. The
  pre-change CoD2x source fails both client/dedicated flag variants for missing
  registration and refresh (`baseline-codx-both/*-result.log`); this red was
  also observed before the first production write (`baseline-codx-red2/`).
- Stock and CoD2x fixtures pass both actual target flag variants and both
  listen/dedicated dvar contexts. Cases include 0/15/16/64 players,
  zero/default/max limits, dead counting, live-only refresh, ET_PLAYER
  spectator semantics, inactive/clientless/non-player filtering, final table
  slot/65th entity, script times, end-frame health change, clock ownership,
  repeated refresh, PVS/flags/masks, self/link exclusion, inclusive expiry
  and first expired observation. See `tests/cod2x/player_broadcast.md` for
  commands and fixture boundaries.
- Actual complete `g_main_mp.c` translation units compile successfully with
  preserved Release engine flags in stock client, stock dedicated, CoD2x
  client and CoD2x dedicated configurations (`production/*-compile.log`).
- Scoped function ABI audit passes both target TUs per mode: zero errors,
  mismatches and new findings (`abi-stock.json`, `abi-codx.json`). Scoped
  existing raw-offset scanner/manifest checks pass the affected client TU
  in both modes: two retained candidates, zero failures (`raw-*.json`).
  The stock/dedicated layouts are unchanged; this is scoped evidence, not a
  full repository/import ABI approval.
- `check_inactive_gates.py --base 380bf2b`: 17 configurations, zero mismatches.
  `tests/fixes13/legacy.py --base 380bf2b`: 10 legacy configurations, zero.
  `tools/abi/legacy.py` on the scoped actual CoD2x database: five option-off
  configurations plus both SDK preprocess commands with `COD2_X64` removed,
  zero mismatches/errors. These are token gates, not Linux i386 byte equality.
- Existing WS54 `level_state` and WS55 `level_scripts` fixtures pass under
  remapped real stock and CoD2x flags. Their production blocks were unchanged.

## Limits and remaining acceptance

The new fixtures stub game/script/HUD dependencies and dvar allocation, and
use synthetic entities/PVS. They prove registration arguments and the real
frame-to-visibility behavior slice, not actual dvar setters, network packet
delivery, map portaling, client sound rendering or performance. No game, GUI,
benchmark, global installation or live audio acceptance ran. No proprietary
input or generated outputs entered the repository. The parent owns composed
binary builds, full merge gates, and live second-client fire/footstep checks
across a PVS portal with zero/default/over-limit controls. WS65 alone does
not close CR03 or certify complete CoD2x server parity.
