# botlib/l_memory module reconciliation

Updated 2026-11-05.

**3/3 reconciled: 3 intentional.** Mac 1.3 is the authority. All three bodies were compared by hand; no allocator source change was needed.

Mac `GetMemory` at 0xc346c allocates the requested size plus four, writes magic 0x12345678, and returns the pointer plus four. `FreeMemory` at 0xc3490 silently rejects a different magic and otherwise frees the original allocation. `GetClearedMemory` at 0xc34ae inlines that allocation and clears exactly the requested payload. Its failure branch still calls memset with null; the native implementation retains that behavior rather than adding a guard.

Native already uses a 16-byte header, with the magic in the last four bytes. This preserves malloc alignment for the wider pointers stored in scripts, token copies and punctuation tables. Freeing subtracts the same 16-byte header. This is a deliberate LP64 layout adaptation; the header is not externally serialized. The underlying production `Z_MallocInternal` clears allocated storage and invokes the OOM handler on failure. No indeterminate storage or new null behavior was introduced.

The arm64 Release `cod2_macos_ded` target built at low priority with two jobs. Both complete production botlib translation units were also compiled directly with the dedicated target's unchanged flags. The allocator TU retains its existing unsigned-long-to-int size warning. Triage and constant checks ran; the inlined allocation versus the native GetMemory call was checked by hand. No runtime allocator fixture, new tests, full CI or replays were run. Allocation failure remains compiled and reviewed, not exercised.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-botlib-script.noindex/`.

## Integrated batch verification

Integrated source a3c4c7c passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
