# system/list.tcc attribution reconciliation

Updated 2026-11-05.

1 equal, 2 fixed (3 entries).

CMemoryBuffer owns the delayed-free request list; its node-only clear matches the binary. The CFence* and GLuint* erase helpers were stubs and now save the successor, unlink/free only the node and return that successor. These are pointer-sized native linked lists, with no pointee destruction.

Changed or unresolved examples: `ZNSt4listIP6CFenceSaIS1_EE5eraseESt14_List_iteratorIS1_E` at 0x002ee596, `ZNSt4listIPmSaIS0_EE5eraseESt14_List_iteratorIS0_E` at 0x002eeab2.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
