# Mac/DirectX 9/CDirect3DVertexBuffer reconciliation

Updated 2026-11-05.

**24/26 reconciled: 12 equal, 12 fixed; 2 unchecked.** The Mac 1.3 i386 binary is the authority. Exact-symbol bodies and the coalesced COM/cache-block bodies were read by hand. The OFF source path is unchanged.

Constructors 0x1f21a and 0x1f354 now create aligned memory, initialize lock/fence state and build a dirty-block cache when usage 0x400000 is set. Cache blocks cover 1024 bytes. The binary scalar at 0x3261c4 is exactly 0.0009765625; the native helper preserves the Mac SSE unsigned conversion, multiply and ceil order, including the high-bit shift/or/double path. Initial native buffer bytes remain deterministically zero-filled instead of copying Mac's unspecified allocation bytes.

Lock at 0x1eb66 had been a pointer-only stub. It now records zero-size/full-length locks, retires dynamic discard storage for two updates, waits on nondynamic buffers and marks the touched cache blocks dirty for flag 1. Flush at 0x1e9de now flushes only dirty 1024-byte ranges and clears each dirty bit after its GL call.

WaitFence at 0x1eab8 now checks strict range overlap for fences aged zero or one frames, finishes overlapping fences, and recycles old/completed IDs. SetFence at 0x1f0a2 prunes old fences, replaces the first identical recent range and inserts the new fence at the list front. Frame subtraction remains unsigned. The native CFence layout was independently checked against 0x1f9c0: ID, pointer, byte count and frame count. The provider is owned by the GL reconciliation packet. Native fence dispatch tags are pinned to GL and removed before direct deletion.

Destructors 0x1eca0, 0x1edfe and 0x1ef50 now destroy the optional cache, release nondynamic VAO packet references, recycle every fence, clear list nodes and destroy aligned backing memory. Cache-block destructors at 0x2ee528/0x2ee53c reset their vtables. Native pointer-width list/vector/cache helpers replace the empty helpers needed by these bodies. QueryInterface at 0x1e8ee now calls virtual AddRef. Descriptor writes, Release, resource type 6 and the no-write COM stubs match.

The existing draw-facing prefix is unchanged. Apple compiler record layouts confirm 32-byte backing state, 24-byte native fence/list nodes and a 112-byte vertex owner. Device constructors allocate `sizeof` the shared type. The compiled Metal CPU fallback retains direct allocation ownership and bypasses the added GL backing/cache/fence behavior.

Two device functions attributed to this module remain unchecked: programmable VAO creation at 0xde60 and fixed-function VAO creation at 0xe4b0. Both Mac bodies were read. No corresponding selected native function exists in the baseline or pending device commit 9f05cde. Their packet selection, VAR/cache flush and stream guards need the pending VAO/vertex-array provider work. Current direct-array/program drawing does not establish parity, and it does not yet exercise the restored SetFence path. The GL packet's initialization must initialize generic VAO/tree state before nondynamic buffer destruction calls `CVAOPacket_ReleaseBuffer`.

The arm64 Release dedicated target, this actual Apple client object and the device consumer compile. All applicable OFF guard comparisons pass. No new tests, full CI or replays were run. Restored behavior is compiled and reviewed, not exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-buffers.noindex/`.
