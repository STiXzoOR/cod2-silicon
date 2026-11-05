# Mac/DirectX 9/D3DXShader reconciliation

Updated 2026-11-05.

39 entries: 30 equal, 7 fixed, 2 intentional. Original Mac i386 and coalesced bodies were read directly. No entries remain unchecked.

Buffer/table QueryInterface at 0x118886/0x11893a now invokes virtual AddRef. Release at 0x1188a0/0x118954 now dispatches virtual deletion at slots 6/28 instead of bypassing destructors. GetDesc 0x2f1a5e and GetConstantDesc 0x2f1a66 are genuine no-ops: native no longer overwrites their output storage. The remaining zero-return constant-table methods were confirmed as genuine Mac no-ops.

GetHighAndLowIndices at 0x11897c was an empty link stub. It now returns minimum in the first output and maximum in the second, reads index zero even for count zero, and preserves initial/final output store order. The obsolete link stub remains only in inactive legacy builds. The existing cache sanitizer fixture exercises both virtual Release paths; the index helper was compiled/reviewed, not exercised.

Global wrappers 0x13d5e4/0x13d5f4 call initializer/destructor 0x13d578 for the embedded sShaderPrograms string tree. Current native lp64_shader_cache.h replaces that map with a local extracted cache and owned per-result buffers, which the existing fixture exercised. These are explicit cache/lifetime replacements; no embedded shader payload or original source was imported.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.

## Integrated batch 16 verification

Engine 28e3856 and fixture-only follow-up ea23228 passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
