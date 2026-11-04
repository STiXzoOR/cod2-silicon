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
