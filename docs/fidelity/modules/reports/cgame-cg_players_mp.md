# cgame/cg_players_mp module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 4 equal, 2 fixed.** Every Mac body was compared by hand,
including the complete turret positioning/blending body inlined into
`CG_Player` at 0x1e6834. No platform-specific player behavior required a
Windows override. Existing native storage and layout contracts remain.

## Corrections

**The weapon gate tested the wrong flag.** Mac 0x1e6fec tests 0x20000 after
scene submission. Native tested 0x2.

**Turret blend times rounded in the wrong order.** Seven Mac paths divide
1000 by the current frame time before multiplying by the absolute weight
difference; for example 0x1e71b2..0x1e71be. Native multiplied before dividing.
The missing `tag_weapon` warning and the childless-animation error's 0x15
localization prefix now also match the Mac strings.

**Reset debug output missed live players.** Mac 0x1e64c1 returns to the common
debug print at 0x1e6333 after clearing live animation state. Native printed
only on the dead-player return. Mac also supplies a promoted double to its
`%i` yaw format. Native now explicitly passes that double's low word, copying
the deterministic i386 output without relying on native varargs undefined
behavior. The existing null-tree protection is retained; typed native frame
clears replace the i386 fixed 48-byte clears.

## Matching behavior

`CG_UpdatePlayerDObj` preserves the validity gate and clientinfo/DObj arguments.
`CG_PlayerFloatSprite` preserves local first-person suppression, head-tag and
fallback heights, fixed-screen radius and draw flags. `CG_PlayerSprites`
preserves script/team filtering and the killcam, connection, voice and balloon
priorities. `CG_Corpse` preserves the corpse-info index, DObj update, animation,
stance lighting offsets and scene flags. Both body renderers keep their
existing widened, zero-initialized GfxEntity scratch and omit the extra legacy
animation-time update, which the Mac bodies do not contain. Exact-symbol
`otool` disassembly corrected the misleading full-listing prologue for
`CG_UpdatePlayerDObj`.

## Verification and limits

The arm64 Release dedicated target built successfully with `COD2_X64=ON` at
background priority and two jobs. The four assigned client translation units
were separately compiled using the actual `cod2_macos` compiler commands,
since the dedicated target excludes cgame. They also compiled with the same
commands plus `-DCOD2_CODX=1`. The player source retains its existing warning
for passing the byte-backed render scratch to the typed weapon renderer.

The COD2_X64-off comparison passed all five inactive configurations. Exact
SDK preprocessing with COD2_X64 removed also matched, including real line
expansions. These sources contain no direct `__LINE__` uses after the edits.
No new tests, full CI, replays or runtime checks were run. All corrections are
compiled and reviewed, not exercised in a client session. The orchestrator
handles integrated runtime validation.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-cgame-player.noindex/`.

## Integrated batch verification

Integrated source a90c5b7 passed both dedicated builds and every local macOS CI step 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All sixteen changed production source files differ only under linux-x86_64-port-on; the five inactive configurations are preserved. The two existing audio/HUD fixture setup corrections intentionally apply across configurations. Prefixes through the two source layout assertions, including the existing line reset, are unchanged. Source checks do not establish i386 binary-byte parity.

Parent inspection and independent source/binary reviews covered the six units. Review corrected the Windows fallback-label length and scoreboard arithmetic overflow. No unresolved introduced-code finding remains. The sound module retains 31 unresolved identities rather than claiming equality from missing symbols.

Both 2,000-frame stock and CoD2x replays exactly match both fresh d873794 baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client rendering, camera overlays, UI or every corrected sound/effect path; unreached corrections remain compiled and reviewed.

The command/settings catalog was regenerated twice from the clean engine revision, with identical bytes. Its semantic changes are limited to removing tcmd, tell_target and cg_shock_viewKickFadeTime, and removing the cgame tell registration while retaining both server registrations. Other changes are source revision/line metadata. Generation initially rejected a leftover lowercase authored setting key; that description was removed and the original failure log preserved. Coverage checks and the existing Swift decoder fixture passed with the updated revision expectation. No new tests were written.

Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.

The original step 13 failure is preserved. Its existing player-animation fixture incorrectly expected eFlags bit 2 to suppress a weapon, while Mac 0x1e6fec tests bit 0x20000. The existing case now checks the weapon-call arguments and call order; no test case was added. Independent focused review and the complete step 13 rerun passed. This fixture-only correction does not change the engine source used for builds, ABI audits or replays.
