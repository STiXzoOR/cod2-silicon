# universal/com_memory module reconciliation

Updated 2026-11-06.

**63/64 reconciled: 41 equal, 11 fixed, 11 intentional, 1 unchecked.** Mac 1.3 is the authority. Linux was read only for helper names and intent. Behavior changes are native-only. Legacy preprocessed tokens, including all seven actual OOM line-number arguments, are identical in five OFF configurations.

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

## Remaining-identity audit, 2026-11-05

The seven remaining rows stay unchecked. This pass made no source changes and preserved all established verdicts. Exact-symbol reads reconfirmed the actual native platform owners: `Z_FreeInternal` at 0x3740c delegates to free, `Z_VirtualCommitInternal` at 0x374c4 supplies an existing pointer and size to `VirtualAlloc`, and `Z_VirtualDecommitInternal` at 0x3743a supplies flag 0x4000 to `VirtualFree`. Mac `VirtualAlloc` at 0xb120 returns a non-null supplied pointer or allocates zeroed storage; `VirtualFree` at 0xb13a releases only flag 0x8000. Native `src/Mac/Tools/MacWin32.c` implements that boundary. This positively identifies the active allocation path, but does not establish that the anonymous Linux `Z_FreeRaw`, one-argument `Z_VirtualCommit`, or clearing `Z_VirtualDecommit` rows are replaced APIs. Their different signatures and effects remain unresolved.

Direct references to the Mac hunk permanent/temp fields at 0x477f00/04 and 0x477f08/0c were assigned to their existing allocation, diagnostic, mark and reset owners. `Hunk_SetMark` at 0x37828 returns the high permanent count. The hide/show pair at 0x37bf2/0x37c08 saves and restores the low permanent count. `Hunk_ClearTempMemoryHigh` at 0x37a60 copies the high permanent count to the high temporary count; it does not implement saving and replacing high permanent state. None establishes the full high hide/show API. Reading the low count inside a larger allocator or diagnostic does not identify the separate getter, and resetting marks does not establish a shutdown/free contract for `s_origHunkData`. These direct-reference results are search bounds, not proof that no indirect or inlined identity exists.

Stock Release and CoDxRelease production objects for this module and MacWin32 compiled using their configured arm64 commands. The existing native diagnostics fixture passed, and the unchanged storage fixture passed with ASan in both configurations. These exercise guard allocation/free and large-local rollback, not the seven unidentified APIs. Triage and selected constant checks ran as leads only. No new tests, cases, assertions, full CI or replay ran. Source bytes and therefore OFF `__LINE__` expansions remain unchanged. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-storage-remaining.noindex/`.


## Original raw-free identity follow-up

The retained original Linux 1.0 IDB identifies the anonymous third free wrapper as sub_80a927c at0x80a927c–0x80a928f (exclusive end). Its position between named Z_VirtualFreeInternal and the out-of-memory handler agrees with the reconstructed Z_FreeRaw row. The original 1.3 executable retains that wrapper at0x80ab5e0–0x80ab5f3, immediately after the two named free wrappers. All three complete bodies forward the sole pointer unchanged to the same free import; only relative call displacements differ. The original executable evidence establishes a strict operation alias, without inventing the unknown original name.

Exact Mac Z_FreeInternal at0x3740c–0x37415 tail-jumps to free. The native default Z_FreeInternal likewise calls free(ptr). This closes Z_FreeRaw as equal for the complete shared free operation; it does not assert that a distinct raw wrapper was emitted on Mac. Existing optional guardheap ownership handling remains unchanged. Parent independent review is recorded in the private orchestration packet. No production source change, compilation, tests, CI, replay, or runtime exercise occurred.

Original Linux identities also sharpen the remaining hunk targets: Shutdown0x80aba6e, low-permanent getter0x80abf0e, and high hide/show0x80ac500/0x80ac51e. These require complete Mac operations beyond the already documented low hide/show and high temp-clear owners. The virtual commit/decommit helpers retain different signatures/effects from the Mac exported platform shims. All six remaining rows stay unchecked. Private evidence: recon-provenance-storage5.noindex.


## Hunk shutdown in the terminal quit operation

The original executable establishes a complete consumed owner for Hunk_Shutdown. Named original 1.0 Sys_Quit and Sys_Exit bodies map uniquely to original 1.3 Sys_Quit at0x80d3a7a and Sys_Exit at0x80d3a36. The actual call at0x80d3a8a invokes Hunk_Shutdown at0x80aba6e after dvar and command cleanup. That helper frees original hunk backing, zeroes active/original pointers and total size, and clears both eight-byte permanent/temp state records. The caller then restores stdin flags and invokes Sys_Exit(0), whose complete terminal path ends in exit(status). There is no returning engine continuation within this identified quit operation.

Exact Mac Sys_Quit at0xc3be0–0xc3c6d supplies the platform replacement: its shutdown calls and queued-event frees are followed by preference synchronization and exit(0). Native mac_main.c preserves that complete terminal owner, including established optional platform adaptations. Remaining hunk backing is released with process teardown. Hunk_Shutdown is therefore intentional for this consumed terminal lifecycle. This does not establish a standalone reusable shutdown API, equal reset timing before exit, or equal observations from finalizers. The verdict relies on the original owner/call sequence and complete Mac/native terminal operation, not missing symbols or callers.

Independent review accepted this narrow boundary. No production source changed; no compilation, tests, CI, replay or runtime exercise occurred. The five remaining memory helpers stay unchecked. Exact original and Mac owners, unique original body maps and native hashes are preserved in recon-provenance-storage5.noindex.

## Original allocation and clear providers (storage10)

Two anonymous original1.3 helpers now have complete provider mappings. The reconstruction names remain unknown labels. Z_VirtualCommit0x80ab702 forwards its size to calloc(1,size) and directly returns. Complete Mac Z_VirtualReserveInternal0x37498 passes NULL,size,0x2000,4 to VirtualAlloc0xb120; that selected path tail-calls calloc(1,size). Native preserves this complete nullable zeroed-allocation chain. The identical original sibling0x80ab6b2 is actually called by the original allocation owner at0x80ab6da; no caller of the anonymous copy is invented. This row is equal for the complete existing provider operation. The earlier two-argument existing-pointer CommitInternal candidate remains rejected.

Z_VirtualDecommit0x80ab71e forwards ptr, literal0 and size to memset, with no C-level return-value API. Complete Mac Com_Memset0x47f62 and native com_shared.c forward all arguments to memset. This strict original default-value adapter establishes equal zero-fill provider behavior, not the no-op virtual decommit API. No allocation, release, metadata write or error call is added. Both mappings preserve existing platform allocator, pointer and size_t contracts: no identical negative-size ILP32/LP64 outcomes, allocation success/capacity, deterministic zero-size pointer, invalid-storage allowance or incidental EAX-return contract is claimed. Independent reviews accepted both; existing primary providers and ownership adaptations are unchanged.

Three memory rows remain unchecked. The state-reference audit distinguishes complete Mac Hunk_SetMark0x37828 (high getter), Hunk_HideTempMemory0x37bf2/Show0x37c08 (low state), Hunk_ClearTempMemoryHigh0x37a60 (temp from permanent), and Hunk_ClearToMark0x37f10 (both high fields plus cleanup). None establishes the low getter or high save/replace/restore helpers' complete contracts. Original1.0 IDB code-reference follow-up adds no actual consumer; that bounded finding is not a closure.


Private original bodies and new exact Mac/provider/state-owner evidence are retained in recon-provenance-storage5.noindex and recon-provenance-storage10.noindex. No source changes, compilation, tests, CI, replay or runtime exercise occurred. Missing names/callers are not completion evidence.

## Complete low-permanent read (lowgetter25)

Hunk_GetLowPermanent is equal through the complete scalar read consumed by the actual memory diagnostic. Original0x80abf0e–0x80abf18 reads one global and returns it; the original diagnostic and low allocator corroborate that same global as the permanent low offset, distinct from temporary usage. Mac Com_Meminfo_f0x3752d reads _hunk_low and passes the unchanged32-bit result at0x37532 to the actual print call0x3753d. Its raw format at0x2429f8 is "%8i low permanent\n". Native passes hunk_low.permanent to the identical signed format. Full Mac low allocator0x382a2–0x38358 corroborates the real field and separate temporary write; it is state-role evidence only.

This supplies the complete read/result/observer that the earlier general state-reference inventory had not established. There is no conversion, missing getter branch, output arithmetic or positivity restriction. Preceding print calls must have returned before this selected operation; their effects and later output are outside its boundary. No standalone exported getter, literal inline ancestry, concurrent mutation timing, allocator/error/overflow equivalence or whole diagnostic certification is inferred. Existing source repairs outside this read are not inherited.

Independent full original/Mac/native review: units/review-provenance-lowgetter25.md; private evidence recon-provenance-lowgetter25.noindex. One equal closure; high temporary hide/show helpers remain unchecked. Source unchanged; no new defect, compilation, tests, CI, replay or runtime verification.


## Complete saved high-permanent restore (hunkrestore35)

`Hunk_ShowTempMemoryHigh` is equal for the complete actual permanent-cursor restore inside `Hunk_ClearToMark`. Original 1.3 0x80ac51e through RET 0x80ac52a performs one unchanged input-word store to high.permanent and returns, with no temp update or cleanup. The actual Mac operation has a concrete marker input: `Scr_BeginLoadScripts` saves the full `Hunk_SetMark` result at 0xaf399, then allocates canonical-string storage through a provider that updates the same high-permanent word. `Scr_EndLoadScripts` reads that saved mark through the same global and passes it to the restore owner at 0xaf0d9.

The selected complete effect is the full permanent store at 0x37f1b, after the separate high.temp store and before cleanup. Native `hunk_high.permanent = mark` preserves that input and state write. The restored value is immediately consumed by `Hunk_ClearData` to form its high boundary at 0x37dbe and control actual file-data comparisons. This establishes a real saved-cursor restore rather than a generic assignment or presumed no-op.

The previous rejection of whole-function substitution remains correct: `Hunk_ClearToMark` additionally changes temp and cleans data. Neither surrounding effect belongs to the accepted one-state operation. The scope is a valid serialized corresponding begin/end with unchanged saved mark and defined owned storage; arbitrary marks, overflow, malformed/concurrent/reentrant lifecycle, complete allocation/loading/cleanup behavior and the separate high hide/save helper remain outside it. One equal closure, no source change or new defect. Independent review: units/review-provenance-hunkrestore35.md; complete original, fresh Mac producers/restore/consumer and allocator, raw state addresses and native hashes are retained in private recon-provenance-hunkrestore35.noindex. No compilation, tests, CI, full build, replay or runtime verification.
