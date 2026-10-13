# WS76 — CoD2x local GSC identity and player methods

2026-10-07. Base `754faec61bd728bd0c96ddee003f87792eb10ba0`, branch
`codex/cod2x-script-identity`. Read-only reference
`/Users/stix/Projects/cod2-native-refs/CoD2x`, pinned
`d8c54695a5239ac99d1dfe96212b809b1a54bfee` (1.4.6.8).
This implements the bounded local-method contract identified by WS69. StopGameType,
masters, UDP limiting, 0.3, Config Studio and Metal are separate work.

## Source contract and implementation

The seven canonical names come from reference `src/shared/gsc.cpp:32–39`.
The reference resolves original methods first, then compares extension names
without case and replaces the requested name with its canonical spelling
(`gsc.cpp:124–145`). Native `Scr_GetMethod` now performs that final extension
lookup after its original player, entity, HUD and general resolvers. A separate
native table leaves the original builtin tables intact. All additions require
both `COD2_X64` and `COD2_CODX`.

The reference's `gsc_player.cpp:11–168` defines the value contracts:

- `getIp`: dotted IPv4 from the actual client network address.
- `getHWID`: the actual admitted 32-byte HWID2; empty for an internal bot.
  Native captures admission metadata, so a later userinfo edit cannot replace it.
- `getCDKeyHash`: the untrusted token supplied in the actual getchallenge,
  bounded to the reference's 32 characters. A new `suppliedDigest` field retains
  malformed tokens locally, separately from WS69's forwarding field. Only an
  exactly 32-hex digest can enter the authorization request.
- `getAuthorizationStatus`: the actual challenge reply reason from the configured
  authorization server, bounded to the reference's 32 characters (`server.cpp:682–685`).
  Missing replies, LAN/local exemption and timeout admission produce no invented
  `KEY_IS_GOOD`. A supplied hash and an HWID admission do not establish CD-key
  authorization. The accessor exposes challenge reply state; it does not prove
  that the current supplied digest was independently validated.
- `getViewOrigin`: calls the real `G_GetPlayerViewOrigin`, including its
  view-height, bob, lean and turret-tag paths.
- `getStance`: `crouch` before `prone`, otherwise `stand`, using player-state
  flags 2 and 1.
- `isUsingTurret`: integer boolean from authoritative entity `s.eFlags & 0x300`,
  matching `ENTITY_FLAG_MG` in reference `cod2_entity.h:21`.

The accessors enforce the native player method boundary: correct entity class
and a bounded player entity with a real game client. Class, nonplayer and
out-of-range failures use `Scr_ObjectError`. Extra arguments are ignored, matching
all seven reference accessors; they do not introduce an argument-count policy.
No shared `client_t`, `challenge_t`, `scr_data_t` or builtin-table layout changes.

Private challenge metadata remains bound to the actual challenge slot, exact
address/port and challenge value. Private client metadata is captured before
`ClientConnect`, so a connection script can read it. A rejected mod connection,
`SV_FreeClient`, client-slot reuse, `SV_FreeClients` and `SV_Shutdown` clear it.
Free preserves metadata through the original disconnect callback, then clears
it. Script-persistent ID reset and legitimate reconnect preserve the matching
connection's provenance. `SV_ChangeMaxClients` copies active clients to a new
allocation; sidecars therefore follow slot/address/challenge rather than an old
allocation pointer. Late authorization updates require the matching current
client, address, challenge and supplied token. An unrelated or recycled
challenge cannot update the old client.

There is a protocol correlation limit within an unconnected challenge. An exact
address retry reuses the challenge value even when its supplied digest changes.
The sidecar clears the stored reason on that change, but an authorization reply
carries only the challenge value, with no echoed digest or request generation.
A late reply to an earlier request can therefore supply the current challenge's
reason. The fixture characterizes this through the real retry and reply paths:
the new digest remains visible while the actual late packet's
`EARLIER_REQUEST_REPLY` reason is exposed. The existing challenge flow does not
prevent it; this unit preserves that protocol behavior and does not invent a
digest-validation result.

The host readiness constant remains **0**. Fixture revision 6 is explicit
synthetic host state; this unit does not activate production extension admission
or advertise a complete host revision.

## Validation receipt

`tests/lp64/script/identity_methods.py` uses the frozen integration tree's actual
release engine flags and actual headers, remapped to this worker. It extends
`tests/lp64/game/script_api.c` through an opt-in fixture mode. The new fixtures
extract production metadata/admission/lifecycle functions, the original player
table and resolver, the extension implementations, actual compiler builtin
method emission/function-table insertion, and actual VM builtin-method dispatch
and `Scr_AddString`/`Scr_AddBool`/`Scr_AddVector` routines. Existing legacy player
implementations and external services are fixture seams; the new method bodies
and their compiler/VM boundaries are real production code. String interning,
object identity and allocation services use synthetic fixture storage.

The actual lexer/parser separately parses a synthetic GSC function containing
all seven named player calls. This is front-end syntax/node evidence plus
separate real emission/VM boundary evidence, not a claim that the whole game's
script loader or a complete mod ran.

- Initial behavioral baseline: 53 intended missing-binding/value assertions per
  CoD2x target at `754faec`, `/tmp/ws76-red2`. Expanded baseline before the
  argument-compatibility review: **111** assertions per target,
  `/tmp/ws76-release-baseline`. Compilation/setup errors
  before those behavioral receipts are not counted as red evidence.
- Final script/admission/lifecycle fixtures: **zero failures** with ASan/UBSan,
  stock and CoD2x, listen and dedicated. Receipts:
  `/tmp/ws76-argument-stock-final` and `/tmp/ws76-argument-codx-final`.
- Cases include accepted/denied and late/spoofed/recycled authorization,
  missing/malformed and uppercase supplied tokens, two client slots, LAN/local
  and bot identities, userinfo mutation, persistent script-ID reset, reconnect,
  free/reuse, real shutdown, real client-allocation resizing, metadata visible
  inside a synthetic connection script, and mod-rejection cleanup. VM cases
  cover all seven values, canonical lookup and original-resolver precedence,
  crouch/prone overlap, both MG bits, normal and turret-tag view origins,
  invalid class/nonplayer/entity 65535 and ignored extra arguments. For every
  accessor, three extra arguments (integer, float and string) preserve the expected
  value and VM type; real VM routines release all three argument values, leave
  exactly one return value, and clear both parameter counts. A separate case
  records the same-challenge digest-retry correlation limit above.
- Parent review caught an unsupported no-arguments restriction in the first
  implementation. Independent permissive-argument tests failed all **seven**
  accessors per CoD2x target before removing it, `/tmp/ws76-argument-red`, then
  passed in the final fixtures above. Native entity/player checks remain intact.
- An intermediate malformed-token fixture failed one intended assertion per
  target before separating local metadata from forwarding. A real resizing
  fixture failed three metadata assertions per target with allocation-pointer
  binding before switching to stable slot provenance.
- All **16** changed production TU/configuration combinations compile with
  actual release flags, without a temporary readiness definition:
  `/tmp/ws76-source-compile-final`. This is compile evidence, not a full link.
  After the argument correction, the affected `g_client_script_cmd_mp.c` compiles
  again in all **four** stock/CoD2x listen/dedicated release configurations,
  `/tmp/ws76-argument-compile/receipt.json`.
- Selected stock and CoD2x ABI audits: eight TUs each, **zero errors and
  mismatches**, `/tmp/ws76-abi/{stock,codx}-functions-final.json`. Full composed
  ABI/import/raw-offset/renderer-callback validation remains root's gate. A
  callback audit against these selected databases stopped with `StopIteration`
  because that tool requires `r_init.c`; no callback-audit pass is claimed here.
- `check_inactive_gates.py --base 754faec`: five changed C files, 17 inactive
  configurations, zero mismatches. `tools/abi/legacy.py --base 754faec
  --compile-commands /tmp/ws76-abi/build-macos-codx.json`: five files across
  five inactive configurations, plus four exact SDK commands with `COD2_X64`
  removed, zero mismatches/errors. New includes follow original offset
  assertions, preserving their legacy `__LINE__` typedef names. These checks
  do not replace an actual i386 binary comparison.
- Existing pure `tests/cod2x/run.sh` identity/protocol/runtime/animation fixtures
  pass. `git diff --check` passes. Existing SDK deprecation and conversion
  warnings remain in compile logs.

## Retained script limitations and reproduction

The actual GSC parser passes **ASan only**. UBSan is still **red** in the retained
parser at `src/PC/script/yyparse_impl.h:602:26`:
`short *yyssp = yyssa - 1`, reported as index -1 outside `short[200]`.
Both CoD2x targets reproduce it in `/tmp/ws76-parser-ubsan`; their separate
method/admission/VM executables still pass ASan/UBSan. Do not classify complete
script parser sanitizer validation as green.

The fixture uses a bounded deterministic RNG because the inherited
`SV_GetChallenge` signed `rand() << 16` expression triggers UBSan with a large
libc random value. Its compiler buffer begins with a synthetic GetSelf byte,
placing this fixture's emitted short operand at an aligned address. Initial
emission without that byte exposed the inherited short-store alignment issue
at `src/PC/script/scr_compiler.c:2159`; arbitrary packed bytecode alignment is
not certified by this receipt. Neither existing server/compiler/parser issue
was changed in this bounded unit. Original failure logs remain in
`/tmp/ws76-red` and `/tmp/ws76-green-first`.

From this worker tree, reproduce the normal CoD2x fixture with:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/script/identity_methods.py \
  --compile-db /Users/stix/.codex/worktrees/protocol-fidelity/cod2-native/build-macos-codx/compile_commands.engine.json \
  --output /tmp/ws76-reproduce
```

Use the stock `build-macos/compile_commands.engine.json` for stock controls,
add `--baseline 754faec` for baseline characterization, or add
`--parser-ubsan` with a fresh output path for the explicit parser failure.

The WS69 runner is unchanged by this unit. Root must compose its extractor
with the expanded metadata block and `SV_ClientNumForClient` helper before
rerunning it. The existing host-lifecycle fixture's shutdown extraction also
needs the new metadata cleanup dependency during composition.

Parent composition now extracts the complete production metadata block for
WS69. The separate host lifecycle fixture records the shutdown cleanup boundary;
the identity fixture above still executes the real cleanup implementation.
Parent stock/CoD2x protocol and lifecycle reruns pass in
`CoD2-native/ws76-composed`, and parent identity reruns pass in
`CoD2-native/ws76-parent`, under Application Support. Parser limitations remain
unchanged by these fixture adaptations.

Subsequent WS83 resolves the signed challenge-shift issue with explicit native
32-bit arithmetic and full-range fixed random vectors. The earlier WS76 failure
receipt above remains the diagnosis history; its small identity-fixture random
stream is no longer the sole challenge arithmetic coverage.

All identities and service replies in the new fixtures are synthetic. No
game/GUI, live authorization, owned-key join, benchmark or third-party assets
ran or were copied. Work is uncommitted and unstaged for root review and
integration; full current-release host readiness remains unclaimed.
