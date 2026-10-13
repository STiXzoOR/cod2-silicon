# WS60: camera attachment fidelity

Date: 2026-10-07. Branch: `codex/camera-fidelity`; base:
`425ee1b74fb3c8b38c74818fc2335fd288ecb37a`. This implements WS44 FA03's
confirmed camera omissions in `src/PC/cgame_mp/cg_view_mp.c`. The change is
native-only, applies in stock and CoD2x modes, and preserves the intentional
CoD2x orbit and WS45 native trace guard. No 0.3, Metal, configuration feature,
shared-header or build-system work is included.

## Reference authority and implementation

Authority is the owned, read-only Mac 1.3 STABS executable at
`~/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386`, SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
The original branches and their STABS field identities were inspected directly.
The implementation was independently written from those contracts. No external
implementation source was copied, and no binary or disassembly is committed.

`CG_CalcViewValues` at `0x1d3b28` has two distinct attachment contracts:

| Path | Original evidence | Restored contract |
| --- | --- | --- |
| Mounted origin | `0x1d3d64`–`0x1d3d6e`, `0x1d4678`–`0x1d46d8` | Predicted `eFlags & 0x300` selects `cg_entities[viewlocked_entNum]`; use its `nextState.number` and `localClientNum` to find the DObj. A present object supplies the world `tag_player` position before third-person positioning and camera shake. An absent object leaves the current origin intact. |
| Required mounted tag | `Com_Error` at `0x1d48eb`, message at `0x2e3164` | A present DObj without `tag_player` raises `ERR_DROP` with the original message and its leading byte `0x15`. |
| Mounted firing jitter | `0x1d46de`–`0x1d46e5`, `0x1d48f5`–`0x1d499d` | Only `viewlocked == 2` with first-person rendering adds two independent `crandom()` samples: pitch times the mounted entity weapon's `vertViewJitter`, then yaw times `horizViewJitter`. |
| Spectator transition | `0x1d3d84`–`0x1d3d8b` | When the next snapshot's `pm_type` is 5, skip normal view-height/bob/lean offsets. The earlier predicted spectator return remains intact. |
| Optional weapon tag | `0x1d4172`–`0x1d418b`, `0x1d42eb`–`0x1d4369` | After shake and the viewmodel basis copy, exclude predicted `pm_type` 4/5, third person and `eFlags & 0x300`. A positive viewmodel weapon index must have a DObj, a nonnegative `tag_camera` bone index and a rotation/translation array. A missing optional component preserves the camera. |
| Weapon transform | `0x1d436f`–`0x1d45cf` | Convert the selected bone quaternion using its `transWeight`, append its translation, compose local camera matrix times current view matrix with `MatrixMultiply43`, then update the rendered camera origin/axis and recover `refdefViewAngles` with `AxisToAngles`. Preserve the copied viewmodel basis. Perturbation and FOV follow the attachment. |

STABS resolves the original player-state offsets `0xa0`, `0x590` and `0x594`
to `eFlags`, `viewlocked` and `viewlocked_entNum`; the tag offsets `0x9a` and
`0x9c` to `tag_player` and `tag_camera`; and weapon offsets `0x57c` and `0x580`
to `horizViewJitter` and `vertViewJitter`. `DObjAnimMat` is the original
32-byte quaternion/translation/weight record. Production uses named typed
fields and array indexing, with no new raw offsets or layout changes.

The existing `ConvertQuatToMat` in `com_math.c` already implements the weighted
quaternion arithmetic in the original camera's inline conversion. Reusing it
retains the record's weight contract. The matrix composition uses a separate
output buffer, and the viewmodel state is copied before attachment as in the
reference. Both `CG_InitView` and `CG_DrawActiveFrame` prepare view-weapon
animation before calculating the camera; the weapon tag reads that prepared
skeleton and preserves the downstream view-weapon rendering basis.

## Verification

Private evidence and all compilation outputs are under
`~/Library/Application Support/CoD2-native-camera/validation.noindex/`.
The caller's actual engine databases under
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/`
were read only. The fixture remaps source/include paths into the camera worktree
and writes its outputs into the private evidence directory. Compilation ran
serially at background priority with `taskpolicy -b nice -n 19`.

`tests/camera/run.py` includes the complete production camera translation unit
and links the real quaternion, matrix and angle helpers from `com_math.c`.
The fixture supplies controlled DObj/tag results and deterministic shake,
perturbation and random inputs. Expected positions and axes are independent
analytic constants, not results calculated by the tested implementation.
Existing movement view/bob, weapon/mantle and fixes13 fixtures were inspected;
none exercised these attachment paths. They were left unchanged.

Before production edits, both modes' new weapon fixture failed with X position
10 instead of the independently expected 12. The unchanged baseline also failed
the mounted origin (10 instead of 100), required-tag error, camera pipeline
ordering and mounted third-person cases. After adding final fixture cases,
a historical `425ee1b` run confirms six changed groups fail in each mode while
the ordinary viewport/FOV group passes; `baseline-results.json` records these
results. The final fixture also covers zero and nonzero bone selection,
quarter-weight and non-unit quaternions, rotations around all three axes,
rotated parent axes, view-lock modes 0/1/2, both mounted flags, absent objects,
optional tag/skeleton fallback, spectator/cubemap exclusions and CoD2x orbit.

| Check | Stock | CoD2x |
| --- | --- | --- |
| Actual Release camera translation-unit compile | Pass | Pass |
| Camera fixture, ASan + UBSan, assertions enabled | Seven groups pass | Seven groups pass |
| Scoped function ABI audit, seven production TUs including new callees' definitions | 0 errors, 0 mismatches, 0 new findings | 0 errors, 0 mismatches, 0 new findings |
| Scoped camera raw-offset audit | 21 existing sites, 0 unreviewed/unsafe | 21 existing sites, 0 unreviewed/unsafe |

The scoped ABI set is camera, math, DObj, entity tag helpers, DObj management,
common error handling and weapon-state definitions. It does not substitute for
the coordinating agent's full build and ABI gates. The legacy fixture reports
one changed production file, ten configurations and zero mismatches.
`tools/abi/legacy.py` reports five native-off configurations and one exact SDK
preprocessing command with zero mismatches/errors. `unifdef -UCOD2_X64
-U__aarch64__ -U__arm64__` followed by `cmp` yields byte-identical baseline and
current camera source. The trace guard reports zero unguarded diagnostics in
721 files, and `git diff --check` passes.

## Limits and handoff

This closes the bounded source omissions confirmed in FA03. Synthetic services
do not establish real model-tag availability, animated skeleton readiness,
live mounted aiming, hit registration, renderer output or full camera parity
against the original runtime. No game, benchmark, packaging, network gameplay,
publication or installation ran. The Linux i386 byte-for-byte object/binary gate
and original-versus-native runtime camera replay remain external acceptance.

The production change, isolated fixture/runner/README and this report are handed
back uncommitted. The coordinating agent owns integration and authoritative
full-system verification.
