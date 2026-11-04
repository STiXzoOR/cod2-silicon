# universal/com_memory module reconciliation

Updated 2026-11-04.

**57/64 reconciled: 36 equal, 11 fixed, 10 intentional, 7 unchecked.** Mac 1.3 is the authority. Linux was read only for helper names and intent. All changes are native-only; removing `COD2_X64` with `unifdef` produces byte-for-byte identical source before and after this change.

## Storage and lifetime

Mac `VirtualAlloc` at 0xb120 returns an existing non-null address unchanged, or calls `calloc(1,size)`. `VirtualFree` at 0xb13a frees only for flag 0x8000; decommit is a no-op. Native MacWin32 preserves this contract. Fresh hunk backing is defined zeroed storage. Clearing marks or freeing temporary blocks changes positions and registration chains, without clearing reclaimed payload bytes.

Mac file-data records have data/next/type/name at offsets 0/4/8/9. Native uses 0/8/16/17, and the existing `offsetof(name)` allocations correctly account for the wider pointers. `Hunk_AddData` needs only the header; `Hunk_SetDataForFile` adds the terminated name. Cleanup tests the registration record's address against the freed half-open gap, unlinks it before invoking the type-specific destructor, and preserves records outside that gap.

Low temporary allocations at 0x38192 use a 16-byte header. Only magic and rollback size occupy initialized header fields; rollback includes alignment padding. The native header and allocation order already match. No indeterminate backing or extra payload initialization was introduced. The Mac's unconditional size subtraction on free, without checking whether this is the last block, is preserved.

The Mac large-local buffer spans BSS 0x377f00 through 0x477f00: 1 MiB. Native patch 1.3 already has that capacity, four-byte position rounding, saved-position destruction and buffer-plus-marker lookup. The unusual high-memory upper bound in `Com_TouchMemory` also matches Mac and was preserved.

## Corrections

Nine hunk initialization/allocation failure messages omitted the leading 0x15 marker. Restored it for `Com_InitHunkMemory` (0x37f26), high permanent allocators (0x37844, 0x37904, 0x38358), high temporary allocation (0x379a8), low permanent allocators (0x37a70, 0x382a2), low temporary allocation (0x38192), and temporary reallocation (0x37b3e). Their alignment, counter updates, capacity comparisons, payload clearing and signed MB conversion already matched.

The optional guard allocator returned directly from `Z_MallocInternal` and `Z_MallocGarbageInternal`, bypassing the required OOM check. Those branches now use the shared failure path, matching the failure contract at 0x37d82 and 0x3747c. `Z_TryMallocInternal` continues to return NULL on failure. Native guard pages, hunk canaries, wider file-data headers, local OOM file/line diagnostics and the existing COD2_CODX 512 MB policy remain intentional adaptations.

## Verification and gaps

The arm64 Release `cod2_macos_ded` target builds with two low-priority jobs. The unchanged macOS diagnostics fixture passes for zeroed guarded allocation, guarded/ordinary freeing, stack capture and hunk-canary repair. The unchanged patch-1.3 storage fixture passes for the complete large-local buffer and position rollback. Triage, constants checks and `git diff --check` ran. No new tests, full CI or replays were run.

Allocation-failure handling and the message-marker corrections are compiled and reviewed, not exercised. Diagnostics exercises successful guarded allocation, not guard-allocation failure.

Seven Linux-only rows remain unchecked: `Hunk_Shutdown`, `Hunk_GetLowPermanent`, `Hunk_HideTempMemoryHigh`, `Hunk_ShowTempMemoryHigh`, `Z_FreeRaw`, `Z_VirtualCommit` and `Z_VirtualDecommit`. No code body or specific inline Mac caller was established for them. The high mark names have only absolute-zero `.eh` symbols. The three raw/virtual helpers are explicitly anonymous, unreferenced claims in Linux; in particular, Linux's one-argument calloc commit and memset decommit cannot be equated with the Mac existing-pointer commit and no-op decommit exports. No native bodies were invented from those claims.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-memory.noindex/`.
