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
