# system/allocator.h attribution reconciliation

Updated 2026-11-05.

2 equal, 2 fixed, 1 intentional (5 entries).

The real owners are CVAOPacket.c and COpenGLVAO.c. VAO stream lookup now copies its generic stream pointers, and the deque map allocator is functional. The CCacheInfoBlock vector belongs to the old VAR cache; active vertex buffers and draw submission use typed CPU storage without that vector.

Changed or unresolved examples: `CVAOPacket_IsCached` at 0x001136ec, `ZNSt5dequeItSaItEE17_M_reallocate_mapEmb` at 0x002f0c36.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

Allocation-exception limit: selected `macos_cpp_abi.cpp:517` implements `__Znwm` with throwing `::operator new`. The new C list/deque construction has no per-allocation unwind cleanup. No active native catch/recovery caller was identified; this does not retire or verify the original exception-cleanup tails. The unmatched worker retains 0x113119/0x113166 as unchecked.
