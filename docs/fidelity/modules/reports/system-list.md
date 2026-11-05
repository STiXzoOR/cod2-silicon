# system/list.tcc attribution reconciliation

Updated 2026-11-05.

1 equal, 2 fixed (3 entries).

CMemoryBuffer owns the delayed-free request list; its node-only clear matches the binary. The CFence* and GLuint* erase helpers were stubs and now save the successor, unlink/free only the node and return that successor. These are pointer-sized native linked lists, with no pointee destruction.

Changed or unresolved examples: `ZNSt4listIP6CFenceSaIS1_EE5eraseESt14_List_iteratorIS1_E` at 0x002ee596, `ZNSt4listIPmSaIS0_EE5eraseESt14_List_iteratorIS0_E` at 0x002eeab2.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
