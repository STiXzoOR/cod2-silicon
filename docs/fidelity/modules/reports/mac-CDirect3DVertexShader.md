# Mac/DirectX 9/CDirect3DVertexShader reconciliation

Updated 2026-11-05.

8 entries: 2 equal, 3 fixed, 3 intentional. Original Mac i386 wrappers and coalesced GetFunction were read directly. No entries remain unchecked.

QueryInterface at 0x1f91c now calls virtual AddRef. Release at 0x1f936 now calls the virtual deleting destructor at slot 6 when its saved count reaches zero. The native GetFunction declaration/dispatch now includes the destination argument from the original symbol at 0x2ee780; its true no-op behavior leaves both outputs untouched. GetDevice at 0x1f964 is also genuinely a no-op.

Mac constructor 0x1f96c and destructors 0x1f8cc/0x1f8ec operate on a C++ OpenGL base at this+4 and two vtables. Current native storage keeps a program GLuint in baseState; the cache/declaration/draw providers supply strings and stream state separately. Pending System f8a3c70, read directly at 5f88bd5, restores the wrapper D1-to-base-D2 chain and binding-preserving base upload/deletion. This unit intentionally preserves that pending implementation instead of duplicating it. These three wrapper lifetime/layout verdicts require that dependency to be integrated and do not assert an i386 C++ object overlay or exception ABI.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.
