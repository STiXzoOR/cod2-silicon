# botlib/l_memory module reconciliation

Updated 2026-11-05.

**3/3 reconciled: 3 intentional.** Mac 1.3 is the authority. All three bodies were compared by hand; no allocator source change was needed.

Mac `GetMemory` at 0xc346c allocates the requested size plus four, writes magic 0x12345678, and returns the pointer plus four. `FreeMemory` at 0xc3490 silently rejects a different magic and otherwise frees the original allocation. `GetClearedMemory` at 0xc34ae inlines that allocation and clears exactly the requested payload. Its failure branch still calls memset with null; the native implementation retains that behavior rather than adding a guard.

Native already uses a 16-byte header, with the magic in the last four bytes. This preserves malloc alignment for the wider pointers stored in scripts, token copies and punctuation tables. Freeing subtracts the same 16-byte header. This is a deliberate LP64 layout adaptation; the header is not externally serialized. The underlying production `Z_MallocInternal` clears allocated storage and invokes the OOM handler on failure. No indeterminate storage or new null behavior was introduced.

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. Both complete production botlib translation units were also compiled directly with the dedicated target's unchanged flags. The allocator TU retains its existing unsigned-long-to-int size warning. Triage and constant checks ran; the inlined allocation versus the native GetMemory call was checked by hand. No runtime allocator fixture, new tests, full CI or replays were run. Allocation failure remains compiled and reviewed, not exercised.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-botlib-script.noindex/`.
