# gfx_d3d/rb_backend module reconciliation

Updated 2026-11-05.

**61/63 reconciled: 31 equal, 23 fixed, 7 intentional, 2 unchecked.** Mac 1.3 i386 is the authority.

The command handlers, scene/matrix setup, surface traversal, text, debug primitives, sun callers and benchmark/fence paths were compared with the original Mac entry points. Some ledger rows are implemented in rb_tess.c, rb_debug.c and rb_sky.c; the verdict follows the actual body.

Corrections include the reversed Set3D gates in triangle/sprite commands (0xd64d6 and 0xd6a43), reversed point dispatch/material/batching (0xdd123), integer conversion of float Z storage in lines/points (0xd6bda, 0xdd331), generic normal/color/binormal stores, and world-text index writes after advancing the count (0xd7355). The world-text old index retains Mac's unsigned-short truncation at 0xd7357. Clear colors now follow the clamp and defined conversion behavior at 0xd4a14 onward. Rotated pictures use the double radians conversion at 0xdb24b. Raw texture locking uses native D3DLOCKED_RECT storage for the pointer written at 0xd4832 onward.

Other corrected owned bodies include enum-width tess overflow saves, quad arithmetic grouping and line overflow order, polygon binormal stores, text cursor unsigned-byte normalization, and unordered sun visibility/fade comparisons. ExecuteRenderCommands now clears black only for r_skipBackEnd, before pending tess flushing and developer debug output (0xd8cde/0xd8d28 -> 0xd8d39/0xd93f5), and benchmark materials use native named fields instead of i386 offsets.

The existing native 260-byte matrix records, expanded entity/backend storage, current GL bindings, null-storage guards, monotonic benchmark clock and Apple identity-gamma restoration are intentional adaptations. The latter was checked against the current R_SetColorMappings provider; no provider file was changed.

Two rows remain unchecked. RB_GpuSyncDelay at 0xd44d8 returns an integer budget in Mac API60; native API59 exposes separate void callbacks and uses a zero rdtsc helper. RB_ExecuteRenderCommands lacks the mode-3 adaptive decay/wait at 0xd93b3/0xd94a2 and has zero timing values in its mode-2 tail. Completing those requires the shared callback, clock-unit and elapsed-budget contract. No return cast or uncalled substitute was added; pending sibling work is not parity evidence.

The review follow-up decoded every entry of the raw 34-slot command table at 0x36ace0 and compared it with the native preprocessed initializer. Slot0 remains the terminator. Native null slots6,7,8,9,28,30,31,32 now route to stencil reference, stencil planes, clip planes, shadow cookie, shadow-cookie blur, points, lines and triangles respectively. Existing mappings for all other slots match the original. The full decoded mapping is in private render-command-table.json.

The actual arm64 compiler layouts were checked before connecting these handlers. Stencil reference has refValue at4; stencil planes have nearDist4/count8/data12; clip planes have count4/data8; shadow cookie has matrix4/fade68; point/line headers have counts4, size/width6 and 16-byte vertices beginning at8. The triangle record is 24 bytes with material8, technique16 and counts20/22. Its inline planar payload now begins after that native record, rather than at the original i386 offset16, and normal.x/y storage is copied as bits as 0xd65de/0xd66e0 do. The r_rendercmds.c allocators use eight-byte command alignment and native records for existing pointer-bearing producers. No current native emitters were found for the eight restored IDs; this establishes declared consumer layout and routing, not runtime emission coverage. No producer or shared-header change was made.

Projected 3D point indices now follow 0xddc21-0xddca7: [3,0,2,2,0,1]. The 2D path retains [1,0,2,2,0,3]. The review changes were compiled in rb_backend.c, and the dedicated target and legacy guard check were repeated. They were not runtime exercised.

Verification: all ten assigned renderer translation units compiled with the local arm64 Release compile commands, at low priority with at most two compiler jobs. The dedicated target built successfully, but does not compile these renderer files; the separate translation-unit compilation supplies that coverage. The legacy guard checker reported no mismatches across five COD2_X64-off configurations, and the edited sources contain no __LINE__ macros. No new tests, full CI or replay runs were performed. Corrections are compiled and reviewed, not runtime exercised. Current GL and the render backend remain active; future Metal documentation is not a parity exemption.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-backend.noindex/`.

## Integrated batch 15 verification

Engine bcafce5 and fixture-only follow-ups ae7b674, 5642801 and 4e73a89 passed both dedicated builds, complete local CI steps 02–13, source guards, actual legacy line-map comparisons, parent/per-unit independent reviews and an additional combined-source review. Steps 07, 08 and 13 passed complete reruns after correcting six existing fixtures. Initial failures exposed stale GL/portal plumbing, an outdoor matrix expectation, a missing display provider, the DX7 skinning result and a missing sprite extraction dependency. Exact Mac 0xd2324–0xd232b confirms paired skinning addition; the fixture now expects that result. The sprite fixture retains its type-4 case and aborts if its out-of-scope cylinder helper is reached. No cases or assertions were added.

The standard full-file guard command cannot preprocess the dispatch fixture's deliberate native-only error. A final source-only pass using the same checker confirms all five inactive configurations are identical. Engine source was unchanged by the fixture corrections. Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree; maximum origin/velocity drift is 0.11792/2.0. No i386 byte-parity claim is made.

This batch closes 655 device, world/model, backend, texture/surface, GL and buffer entries. Fixes cover layouts, resource/context/fence lifetime, command and matrix order, lighting/culling constants and functional DISCARD flags. Constructor/reset/draw/VAO/sampler/timing gaps remain explicit. Existing local accelerated-GL raster/saved-screen, mip, volume and all 18 depth cases passed ASan/UBSan. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.

## Batch 17 follow-up

RB_ExecuteRenderCommands now follows the original mode-dependent fence ordering and adaptive budget arithmetic through renderer-gpu-sync.md. The shared native cycle clock is no longer the zero stub on Apple LP64. RB_GpuSyncDelay remains separately tracked for the typed API59 ownership verdict; no API60 query ABI or Metal fence submission parity is asserted.

## Batch 19 follow-up

RB_GpuSyncDelay ownership is now reconciled by renderer-gpu-budget.md: API59 folds the original budget snapshot into SyncGpu. GL fences and the synthetic clock are explicit; Metal submission tracking and wall-clock equivalence are not claimed.
