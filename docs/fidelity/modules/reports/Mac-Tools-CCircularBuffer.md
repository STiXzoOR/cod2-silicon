# Mac/Tools/CCircularBuffer reconciliation

Updated 2026-11-05.

7/8 reconciled: 2 equal, 3 fixed, 2 intentional; 1 unchecked. Read all eight exact Mac entries. Native Apple selects src/platform/macos_ring.c and typed pointer/UInt32 fields, excluding the old source's pointer-to-int storage and empty Write stub.

Mac ReadPtrSize 0x1f9636 and ReadPtr 0x1f965e add the previous pending-read length and reset to zero when the resulting position reaches capacity. Native used modulo; restored the exact reset rule and request/span bookkeeping. Mac Write 0x1f9784 allows the full contiguous free span, then optionally a second span before readOffset. Native reserved one byte and clamped a modular used count. It now follows the Mac branches and updates, including the original equal-index full/empty ambiguity. Native zero-capacity guards remain defined.

The default constructor 0x1f95ee and Reset 0x1f9618 match initialized field behavior. The destructor and Alloc keep an explicit native allocation adaptation: always reset storage and record zero capacity on malloc failure, rather than retaining a nonzero capacity with a null buffer. This does not introduce indeterminate native storage.

The size-taking constructor at 0x1f9740 was read: it zeroes all fields, allocates, and records the requested capacity. Native exposes a default constructor and separate Alloc, but no callable identity or surviving caller for that overloaded constructor was found. Its row stays unchecked; absence is not equality. Old Carbon recorder/Miles playback consumers are excluded; native audio/voice providers do not use those raw layouts.

The arm64 Release dedicated target compiled and linked the changed provider. The corrections were reviewed against the disassembly but not exercised; no new tests, full CI or replay ran. OFF preprocessing matches the base, and the file contains no __LINE__ expansions.

Parent ran the unchanged existing Debug ASan/UBSan threads_ring fixture after correcting its SDL target dependency: recursive mutexes, native IDs, virtual destructors,20 dual-stop restarts and10000 wrapped bytes passed. Full/empty ambiguity and allocation failures were not directly exercised. No new tests were added.
