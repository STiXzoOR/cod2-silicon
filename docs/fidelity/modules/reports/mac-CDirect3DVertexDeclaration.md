# Mac/DirectX 9/CDirect3DVertexDeclaration reconciliation

Updated 2026-11-05.

9 entries: 4 equal, 5 fixed. Original Mac i386 bodies were read directly. No entries remain unchecked.

GetDeclaration at 0x1f5f4 copies caller elements into stored elements, then reports count. Native copied in the opposite direction. The Mac bug is preserved, including sentinel copying. Constructor 0x1f820 recognizes all four end-marker words; native previously omitted Usage/UsageIndex. QueryInterface and Release now use the virtual AddRef/deleting-destructor slots shown at 0x1f5b2/0x1f5cc.

The type and usage jump tables at 0x328060/0x328080 were decoded from the original binary. Sizes, tokens, stream reset and byte-based skip chunks before the stream token match. At 0x1f7d0 a repeated Usage=2 checks used register 2 and selects register 6 without marking register 6; native now preserves that branch. Existing defined native handling of texture usage registers outside the 16-byte used array remains, rather than introducing the original out-of-bounds stack write. The native malloc/free contract also retains its existing null-allocation behavior instead of importing C++ allocation exceptions. Successful construction always includes at least one sentinel element.

The arm64 Release dedicated target built at low priority with two jobs. All four assigned Apple client objects and the narrow device helper compiled through the actual CMake renderer rules. The unchanged existing `shader_cache` fixture passed ASan/UBSan. Six-file guard comparison against 28b9601 differs only under native port-on; changed files contain no `__LINE__`. No new tests, full CI or replays were run. GL calls were compiled and reviewed, not exercised. Private evidence is under `~/Library/Application Support/CoD2-native-decomp/recon-mac-shaders.noindex/`.
