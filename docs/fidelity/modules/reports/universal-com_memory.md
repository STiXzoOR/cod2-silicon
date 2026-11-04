# universal/com_memory module reconciliation

Updated 2026-11-04.

**57/64 reconciled: 36 equal, 11 fixed, 10 intentional, 7 unchecked.** Mac 1.3 is the authority. Linux was read only for helper names and intent. Behavior changes are native-only. Legacy preprocessed tokens, including all seven actual OOM line-number arguments, are identical in five OFF configurations.

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

Independent review found that adding conditional source lines shifted the legacy `__LINE__` diagnostics even though the selected legacy statements were unchanged. Three OFF-only line-map resets preserve the seven original arguments (213, 225, 236, 249, 263, 294 and 368). The follow-up preprocessing comparison leaves `__LINE__` active; the ordinary guard checker substitutes that macro and cannot establish this detail. Native dedicated rebuilding also passed after the correction.

Seven Linux-only rows remain unchecked: `Hunk_Shutdown`, `Hunk_GetLowPermanent`, `Hunk_HideTempMemoryHigh`, `Hunk_ShowTempMemoryHigh`, `Z_FreeRaw`, `Z_VirtualCommit` and `Z_VirtualDecommit`. No code body or specific inline Mac caller was established for them. The high mark names have only absolute-zero `.eh` symbols. The three raw/virtual helpers are explicitly anonymous, unreferenced claims in Linux; in particular, Linux's one-argument calloc commit and memset decommit cannot be equated with the Mac existing-pointer commit and no-op decommit exports. No native bodies were invented from those claims.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-universal-memory.noindex/`.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
