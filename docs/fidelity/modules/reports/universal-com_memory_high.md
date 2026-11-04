# universal/com_memory_high module reconciliation

Updated 2026-11-04.

**1/1 reconciled: 1 equal.** Mac `Hunk_ClearTempMemoryHigh` at 0x37a60 unconditionally copies high permanent usage into high temporary usage. The native body in `src/PC/universal/com_memory.c` matches. The separate Linux module label does not imply a separate native file.

This operation changes the high allocation position, without clearing payload storage. The Mac export establishes identity directly; no new wrapper or implementation was needed.

The arm64 Release dedicated target builds. The unchanged macOS allocator diagnostics and patch-1.3 storage fixtures pass, but neither specifically exercises this high-temp reset. No new tests, full CI or replay were run.
