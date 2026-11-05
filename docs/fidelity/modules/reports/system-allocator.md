# system/allocator.h attribution reconciliation

Updated 2026-11-05.

2 equal, 2 fixed, 1 intentional (5 entries).

The real owners are CVAOPacket.c and COpenGLVAO.c. VAO stream lookup now copies its generic stream pointers, and the deque map allocator is functional. The CCacheInfoBlock vector belongs to the old VAR cache; active vertex buffers and draw submission use typed CPU storage without that vector.

Changed or unresolved examples: `CVAOPacket_IsCached` at 0x001136ec, `ZNSt5dequeItSaItEE17_M_reallocate_mapEmb` at 0x002f0c36.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

Allocation-exception limit: selected `macos_cpp_abi.cpp:517` implements `__Znwm` with throwing `::operator new`. The new C list/deque construction has no per-allocation unwind cleanup. No active native catch/recovery caller was identified; this does not retire or verify the original exception-cleanup tails. The unmatched worker retains 0x113119/0x113166 as unchecked.

## Integrated batch 14 verification

Integrated source c1bb3b0 passed both dedicated builds, complete local CI 02–13, native-only source guards, parent and fresh independent reviews, and both 2,000-frame replays. No changed source uses line macros. Replay players, state hashes and entities exactly match both d873794 baselines and each other; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. No new tests or i386 byte-parity claim.

This batch closes 880 entries. Fixes cover image metadata/mips and descriptor storage, container/sorting behavior, shader ownership, and the eight-byte AbsoluteTime caller/provider ABI. Three System constructor/factory boundaries and six import/allocation boundaries remain unchecked. Microphone capture remains unsupported. Dedicated replay does not exercise client rendering, shader/context lifetime, device output, or dormant platform wrappers. Unreached client and error paths remain compiled and reviewed; local fixtures establish only their stated cases.
