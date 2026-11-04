# unix/unix_threads reconciliation

Updated 2026-11-05.

**4/5 reconciled: 2 equal, 2 intentional; 1 unchecked.** Exact-symbol Mac bodies at 0x222510, 0x22252e, 0x222548 and 0x22255a were compared with the selected `PC/win32/win_threads.c` provider, which both native targets compile.

Initialization stores the current thread ID before `Com_InitThreadData(0)`. Main-thread checks compare against that saved ID. Mac 0xb00c supplies `pthread_self`; native `MacWin32.c` supplies `pthread_mach_thread_np(pthread_self())`. This is an intentional native ID representation. WinMain initializes it; client console/render checks consume it.

Get/set preserve the original global five-pointer array. It is not per-thread TLS, despite the names. `Com_InitThreadData` writes slots 1, 2 and 3 for `va`, error jump frames and collision trace state. Native pointer width is the only storage adaptation; no guard or thread-local redesign was introduced.

Both native executables compile and link. The existing Debug `threads_ring` fixture passes and distinguishes worker and main IDs; it does not exercise the global engine slot accessors. No source changes or new tests were needed for these four wrappers.

`Sys_NullQuery4` remains unchecked: the Linux-only identity has no established executable Mac body or selected native replacement contract.
