# system/deque.tcc attribution reconciliation

Updated 2026-11-05.

1 fixed (1 entries).

The coalesced uint16 deque clear helper was a no-op. It now frees all blocks after the start block and retains that start iterator/block, as native Optimize requires.

Changed or unresolved examples: `ZNSt5dequeItSaItEE5clearEv` at 0x002f0e64.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

Allocation-exception limit: selected `macos_cpp_abi.cpp:517` implements `__Znwm` with throwing `::operator new`. The new C list/deque construction has no per-allocation unwind cleanup. No active native catch/recovery caller was identified; this does not retire or verify the original exception-cleanup tails. The unmatched worker retains 0x113119/0x113166 as unchecked.
