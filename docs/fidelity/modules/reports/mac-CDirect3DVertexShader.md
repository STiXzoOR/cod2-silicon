# Mac/DirectX 9/CDirect3DVertexShader reconciliation

Updated 2026-11-05.

8 entries: 2 equal, 3 fixed, 3 intentional. Original Mac i386 wrappers and coalesced GetFunction were read directly. No entries remain unchecked.

QueryInterface at 0x1f91c now calls virtual AddRef. Release at 0x1f936 now calls the virtual deleting destructor at slot 6 when its saved count reaches zero. The native GetFunction declaration/dispatch now includes the destination argument from the original symbol at 0x2ee780; its true no-op behavior leaves both outputs untouched. GetDevice at 0x1f964 is also genuinely a no-op.

Mac constructor 0x1f96c and destructors 0x1f8cc/0x1f8ec operate on a C++ OpenGL base at this+4 and two vtables. Current native storage keeps a program GLuint in baseState; the cache/declaration/draw providers supply strings and stream state separately. Pending System f53c6a7, read directly at c1be798, restores the wrapper D1-to-base-D2 chain and binding-preserving base upload/deletion. This unit intentionally preserves that pending implementation instead of duplicating it. These three wrapper lifetime/layout verdicts require that dependency to be integrated and do not assert an i386 C++ object overlay or exception ABI.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against f0d941a differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.

## Integrated batch 16 verification

Engine d7e6ca9 and fixture-only follow-up 935519b passed both dedicated builds, complete local CI steps 02–13, native-only source guards, per-unit parent/independent reviews and a fresh combined-source review. All five inactive configurations are unchanged; no changed production file uses line macros. Initial step 07 exposed an omitted CTAB register-count input in the existing shader-argument fixture. Supplying the restored field made the complete step 07 rerun pass with unchanged cases and assertions. Packet-container integration also removed a duplicated preprocessor guard, restoring the exact independently reviewed initializer.

Both 2,000-frame replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six Linux logic fields agree, with maximum origin/velocity drift 0.11792/2.0. Existing accelerated-GL raster/saved-screen checks and all 18 depth cases passed ASan/UBSan. No new tests, cases or assertions were added; no i386 byte-parity claim is made.

This batch closes 334 math, shader, array, material, command and frame/setup entries. Fixes cover arithmetic/aliasing, shader ownership/declarations, array and cache lifetime, constant-table routing, command/buffer order, light partitioning and screenshot handling. Full device registration/state/draw and timing gaps remain explicit for separate follow-ups. Dedicated replays do not exercise client rendering; unreached client/error paths remain compiled and reviewed. These checks do not establish a complete rendered frame.
