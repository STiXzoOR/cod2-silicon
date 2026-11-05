# system/new_allocator.h attribution reconciliation

Updated 2026-11-05.

3 intentional, 2 unchecked (5 entries).

The old surface/volume reserve helpers are replaced by typed calloc mip arrays in the active texture providers; CResInfo assignment belongs to excluded MacDisplay storage. Both Device constructors remain unchecked. The selected CreateDevice initializes 4096-byte static native storage through CDirect3DDevice_Init and bypasses the original constructor. The existing exported constructor sets only vtable/refcount, and complete Mac default/state/Reset equivalence is not established. The parent retains that device-initialization audit.

Changed or unresolved examples: `CDirect3DDevice_CDirect3DDevice` at 0x0001a7ba, `CDirect3DDevice_CDirect3DDevice` at 0x0001ab62.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
