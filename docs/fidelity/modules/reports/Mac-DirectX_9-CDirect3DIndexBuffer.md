# Mac/DirectX 9/CDirect3DIndexBuffer reconciliation

Updated 2026-11-05.

**18/18 reconciled: 10 equal, 8 fixed.** The Mac 1.3 i386 binary is the authority. Ordinary bodies were read from exact-symbol disassembly; the COM stubs were read from `__textcoal_nt`. The OFF source path is unchanged.

The constructor bodies at 0x1b580 and 0x1b5fa embed a 32-byte-aligned `CMemoryBuffer` and derive index width with `GetFormatSizeInBits(format) >> 3`. Native had used a direct allocation and assumed every non-INDEX16 format meant four-byte indices. The LP64 owner now preserves its existing draw-facing prefix and appends the backing-memory state. Its Apple size is 80 bytes; both device allocation sites use `sizeof`.

Lock at 0x1b508 sets the lock metadata, substitutes the full buffer length for size zero, and handles usage 0x200 plus flag 0x2000 by retiring the backing allocation for two updates and recreating it. The draw-facing pointer follows the new aligned data. Both destructors now destroy the backing allocation rather than freeing an interior data pointer.

GetDesc at 0x2ee208 is a no-write success stub. Native had invented descriptor writes. QueryInterface and Release now use the virtual AddRef and deleting-destructor slots at 0x1b4be and 0x1b4d8. The remaining no-write COM stubs, resource type 7 and empty Unlock match.

Initial buffer bytes remain zero-filled as an intentional native allocator adaptation; the Mac allocation leaves them unspecified. The compiled Metal CPU-constructor fallback retains direct `calloc` storage and a null backing-memory vptr, matching its existing sidecar destructor's `free(data)` ownership. Metal object allocation is 4096 bytes and covers the extended owner; GL and Metal lock paths discriminate by the actual backing state.

The arm64 Release dedicated target and actual Apple client objects for this module and the device consumer compile. All 25 applicable OFF/platform guard comparisons across this packet pass; no changed file uses `__LINE__`. Constants/triage scripts were used only to find candidates; C++ names required explicit ranges and coalesced bodies required direct disassembly. No new tests or replays were run. Changes are compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-buffers.noindex/`.
