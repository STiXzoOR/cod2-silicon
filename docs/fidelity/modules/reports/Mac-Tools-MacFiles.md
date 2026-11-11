# Mac/Tools/MacFiles reconciliation

Updated 2026-11-05.

**5/5 reconciled: 1 equal, 3 fixed, 1 intentional.** Exact mangled-symbol Mac disassembly was used; Linux supplied no implementation evidence.

`MacFiles_RemoveDirectoryA` at 0x8b18 and `RemoveDirectoryContents` at 0x89ea were empty native stubs. Apple native now uses the real SDK FSRef filesystem APIs. Lookup errors propagate and a non-directory returns `paramErr` (-50). The iterator requests one child at a time with `kFSIterateDelete` and `kFSCatInfoNodeFlags`, recurses into directories, clears the original locked flag when necessary, deletes children and closes the iterator. It stops at stored errors and clears `errFSNoMoreItems` (-1417). The original ignores `FSSetCatalogInfo` and iterator-close statuses.

The outer function has a deterministic Mac bug: 0x8b68–0x8b7a returns the saved initial lookup success, even if child or root deletion later fails. Native copies that result rather than reporting a newly invented failure. The root is deleted only if recursion reports success. `Sys_RemoveDirTree` and the player-profile deletion caller use this provider.

SDK `FSRef` storage is 80 bytes, matching the engine's opaque declaration. The native branch uses the SDK's full `FSCatalogInfo`, `ItemCount`, iterator and status types. Every field read is written by the corresponding successful SDK call; no indeterminate fields are read to simulate the old layout. Keeping the same FSRef APIs preserves their resolution and symlink policy instead of replacing it with guessed POSIX traversal.

Six matching no-op exports in `link_stubs.c` are excluded only on Apple with `COD2_X64`, so the client imports `FSPathMakeRef`, `FSOpenIterator`, `FSGetCatalogInfoBulk`, `FSSetCatalogInfo`, `FSDeleteObject` and `FSCloseIterator` from CoreServices. CMake now links that framework. The source/callback audit found no active native caller of the old `MacFolders` wrappers that also use `FSPathMakeRef`; their unfinished path implementation remains separately queued.

`CleanPath` at 0x8940 takes an unsigned-byte Boolean. Native's int parameter tested high bits too; the native branch now retains the original low-byte choice. The copy loop and terminator match. The C++ in-place overload at 0x8998 is intentionally consolidated into the alias-safe three-argument C helper with identical input/output pointers. No native callback targets the old overload. `MacFiles_access` at 0x8b7c matches the 1024-byte normalization buffer, empty-input handling and final `access` argument order.

Both arm64 Release targets compile and link; client symbols verify SDK ownership. Inactive source comparisons pass and these files contain no `__LINE__` uses. Removal, path-error and lock/symlink cases were compiled and reviewed only. No deletion, URL, app launch or user-activity assertion was executed; no relevant existing deletion fixture was found, and no new tests were written. The existing clock/network/thread fixtures do not exercise these corrections.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-unix-runtime.noindex/`.

Integrated ABI follow-up: the SDK-backed MacFiles_RemoveDirectoryA returns 32-bit OSStatus, while mac_common inherited the project's 64-bit OSStatus declaration. Its Apple x64 declaration now uses 32-bit int to match the actual provider; legacy declarations are preserved. Actual caller objects compiled and inactive guards passed. Filesystem deletion was not executed.

## Integrated batch verification

Integrated source a3c4c7c passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
