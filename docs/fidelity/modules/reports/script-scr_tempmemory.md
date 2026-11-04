# script/scr_tempmemory module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 5 equal.** Mac 1.3 i386 is the authority. The source did not change.

All five bodies at 0xba0a4–0xba168 were compared, with exact-symbol-start disassembly and the constant checker. `TempMemoryReset` clears the position. `TempMalloc`, `TempMallocAlign` and `TempMallocAlignStrict` all use the same unaligned sequence: add the requested length, reallocate to that size, return the base plus the old position, then store the new position. The two alignment names introduce no extra padding on Mac. `TempMemorySetPos` retains both reallocation calls and the pointer-delta arithmetic.

The arm64 Release `cod2_macos_ded` target compiled in this worker's worktree with low priority and two jobs. No new tests, full CI or replays were run, and these helpers were not exercised separately. `COD2_X64=OFF` preprocessing is byte-identical to the assigned base.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-memory.noindex/`.
