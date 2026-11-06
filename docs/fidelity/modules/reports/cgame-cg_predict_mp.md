# cgame/cg_predict_mp module reconciliation

Updated 2026-11-04.

**9/9 reconciled: 5 equal, 3 fixed, 1 intentional.** Every Mac body was compared
by hand, including the complete prediction command loop and pickup handling.
No Windows override was needed for these ordinary arithmetic/control paths.

## Corrections

**Temporary player collision boxes used the wrong contents.** Mac
0x1de734..0x1de745 passes 0x2000000 for players and 1 for other packed solids
to CM_TempBoxModel. Native passed the capsule selector, always 1 for the
normal wrapper. Brush-model rejection, packed extents and trace merging match.

**Prediction decay and ADS spread rounded in the wrong order.** Mac
0x1de312..0x1de319 computes `(decay-elapsed)/decay`; native used
`1-elapsed/decay`. Mac 0x1ddd69..0x1ddd79 doubles the random float, multiplies
by double-precision pi, then converts to float. Native used float pi.
The normal-movement trigger gate now retains the unsigned comparison from
0x1ddfbd. Three developer diagnostics now use the Mac strings. Existing native
dvar safety, initialized moved flag and reticle reset are preserved.

**Trace bounds chose a different endpoint on ties.** The subtraction masks in
Mac `Trace_CalcBounds` at 0x1de480 choose the start value on equal or unordered
comparisons. Native direct min/max comparisons chose the interpolated end.
Native now preserves the six subtraction/less-than-zero predicates as well
as the interpolation operation order. The exact-symbol entry was checked.

## Matching behavior and naming

`CG_ClearSolidList`, `CG_BuildSolidList` and `CG_PointContents` preserve the
count resets, snapshot/brush-model filters and final contents mask.
`CG_InterpolatePlayerState` preserves bob wrap, optional current-command
angles, and every interpolated field. `CG_TraceCapsule` at 0x1de958 preserves
the world trace, 1022/1023 classification and exact-zero early return.

The Linux-only `CG_Trace` row intentionally maps to Mac/native
`CG_TraceCapsule`: the complete wrapper operations and selector-one clipping
match, and native pmove/view/draw call that Mac-named implementation. There
is no native CG_Trace caller or independent Mac export to reconstruct. A
second wrapper was not added merely to satisfy the Linux spelling.

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
