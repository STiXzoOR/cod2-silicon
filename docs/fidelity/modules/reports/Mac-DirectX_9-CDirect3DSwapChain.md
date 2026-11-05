# Mac/DirectX 9/CDirect3DSwapChain reconciliation

Updated 2026-11-05.

**14/14 reconciled: 12 equal, 2 fixed.** The Mac 1.3 i386 binary is the authority. Ordinary bodies were read from exact-symbol disassembly; the six no-write success stubs were checked in `__textcoal_nt`. The OFF source path is unchanged.

QueryInterface at 0x1d89a now invokes AddRef through vtable slot 1. Release at 0x1d8b4 now invokes the deleting destructor through slot 11 when the reference count reaches zero. Native had called the class implementations directly.

Constructor 0x1d916 initializes vtable, null back buffer and reference count 1. Destructors 0x1d84e/0x1d86e conditionally call the surface's virtual Release, with D0 also deleting the owner. GetBackBuffer 0x1d8dc returns the stored surface and unconditionally calls its virtual AddRef. SetBackBuffer 0x1d8fc assigns and AddRefs the new surface without releasing the old one, matching the original behavior. Present, front-buffer data, raster status, display mode, device and present-parameters methods at 0x2ee288–0x2ee2b0 all return success without touching outputs.

The arm64 Release dedicated target and the actual Apple client object compile. All applicable OFF guard comparisons pass; no changed file uses `__LINE__`. No new tests, full CI or replays were run. The virtual-dispatch corrections are compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-buffers.noindex/`.
