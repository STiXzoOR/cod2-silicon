# Mac/DirectX 9/CDirect3DVertexDeclaration reconciliation

Updated 2026-11-05.

9 entries: 4 equal, 5 fixed. Original Mac i386 bodies were read directly. No entries remain unchecked.

GetDeclaration at 0x1f5f4 copies caller elements into stored elements, then reports count. Native copied in the opposite direction. The Mac bug is preserved, including sentinel copying. Constructor 0x1f820 recognizes all four end-marker words; native previously omitted Usage/UsageIndex. QueryInterface and Release now use the virtual AddRef/deleting-destructor slots shown at 0x1f5b2/0x1f5cc.

The type and usage jump tables at 0x328060/0x328080 were decoded from the original binary. Sizes, tokens, stream reset and byte-based skip chunks before the stream token match. At 0x1f7d0 a repeated Usage=2 checks used register 2 and selects register 6 without marking register 6; native now preserves that branch. Existing defined native handling of texture usage registers outside the 16-byte used array remains, rather than introducing the original out-of-bounds stack write. The native malloc/free contract also retains its existing null-allocation behavior instead of importing C++ allocation exceptions. Successful construction always includes at least one sentinel element.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.

## Integrated batch 16 verification

Engine 28e3856 and fixture-only follow-up ea23228 passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
