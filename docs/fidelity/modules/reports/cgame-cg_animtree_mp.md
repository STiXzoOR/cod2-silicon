# cgame/cg_animtree_mp module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 6 equal.** Compared every exported Mac body with native by
hand, supported by call/string triage and the constant checker. No source
changes were needed.

`CGScr_LoadAnimTrees` at 0x1e4150 passes zero to the script loader. The set/check
pair at 0x1e4164 and 0x1e4188 stores and compares the entity type and model.
`CG_SafeDObjFree` at 0x1e41b8 frees the DObj before clearing the type/model cache,
then conditionally frees and nulls the animation tree with the MT_Free callback.
The two bulk cleanup functions preserve that ordering over 0..63 and 64..1023.
Native typed arrays preserve widened model/tree pointers and entity stride.

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
