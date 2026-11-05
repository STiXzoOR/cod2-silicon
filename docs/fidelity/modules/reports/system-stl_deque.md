# system/stl_deque.h attribution reconciliation

Updated 2026-11-05.

1 equal, 4 fixed (5 entries).

Score retains a bounded native FIFO equivalent to Mac hit-count-before-push behavior with ten entries. Optimize was only memmove; it now builds/removes triangle nodes, emits the first triangle, follows the binary’s highest-score/first-tie selection and trims the FIFO after every subsequent index. The three deque storage helpers are restored with native pointer widths and 256 uint16 entries per block. Empty/malformed triangle input keeps defined native handling; Mac reads invalid storage there. C++ allocation-exception cleanup is not reproduced by the C implementation.

Changed or unresolved examples: `Optimize` at 0x00112c44, `ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm` at 0x002f0f1a, `ZNSt11_Deque_baseItSaItEED2Ev` at 0x002f0bda, `ZNSt5dequeItSaItEE16_M_push_back_auxERKt` at 0x002f0de0.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).

Allocation-exception limit: selected `macos_cpp_abi.cpp:517` implements `__Znwm` with throwing `::operator new`. The new C list/deque construction has no per-allocation unwind cleanup. No active native catch/recovery caller was identified; this does not retire or verify the original exception-cleanup tails. The unmatched worker retains 0x113119/0x113166 as unchecked.

Independent review caught the previously unused helper’s reversed comparison. The native helper now selects a strictly greater score at Mac 0x112454 and preserves the first candidate on ties. Compiled and reviewed only; renderer reachability is not established.
