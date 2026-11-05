# Mac/DirectX 9/D3DXShader reconciliation

Updated 2026-11-05.

39 entries: 30 equal, 7 fixed, 2 intentional. Original Mac i386 and coalesced bodies were read directly. No entries remain unchecked.

Buffer/table QueryInterface at 0x118886/0x11893a now invokes virtual AddRef. Release at 0x1188a0/0x118954 now dispatches virtual deletion at slots 6/28 instead of bypassing destructors. GetDesc 0x2f1a5e and GetConstantDesc 0x2f1a66 are genuine no-ops: native no longer overwrites their output storage. The remaining zero-return constant-table methods were confirmed as genuine Mac no-ops.

GetHighAndLowIndices at 0x11897c was an empty link stub. It now returns minimum in the first output and maximum in the second, reads index zero even for count zero, and preserves initial/final output store order. The obsolete link stub remains only in inactive legacy builds. The existing cache sanitizer fixture exercises both virtual Release paths; the index helper was compiled/reviewed, not exercised.

Global wrappers 0x13d5e4/0x13d5f4 call initializer/destructor 0x13d578 for the embedded sShaderPrograms string tree. Current native lp64_shader_cache.h replaces that map with a local extracted cache and owned per-result buffers, which the existing fixture exercised. These are explicit cache/lifetime replacements; no embedded shader payload or original source was imported.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.
