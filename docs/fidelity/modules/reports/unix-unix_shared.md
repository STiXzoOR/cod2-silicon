# unix/unix_shared reconciliation

Updated 2026-11-05.

**19/26 reconciled: 5 equal, 4 fixed, 10 intentional; 7 unchecked.** The previously reconciled `Sys_SnapVector` Windows-rounding verdict and evidence are preserved exactly.

`Sys_ListFilteredFiles` stopped one result early. Mac 0xc4977 and 0xc4a30 stop only above 0xFFE, allowing entry 4094; native used `>=`. Recursion, path formatting, dot entries, filters, string allocation and directory closure were compared by hand.

Both listing functions used `S_ISDIR`, while Mac 0xc4a1d, 0xc4bec and 0xc4c58 test only mode bit 0x4000. Native now preserves that quirk, including socket classification. `Sys_ListFiles` otherwise retains the suffix comparison, hidden-name exclusion, slash sentinel, 4095-entry bound, filter delegation and NULL termination. Native pointer-list allocation deliberately uses `sizeof(char *)`.

`Sys_DirectoryHasContents` counted every visible entry. Mac 0xc4e87–0xc4e97 accepts only visible `DT_REG` entries, rejecting directories, symlinks and unknown types. The native helper now does the same. `FS_AddGameDirectory` uses this result to decide whether a localized directory qualifies.

`Sys_Mkdir` had reduced Mac 0xc48e2's access mode 7 to existence-only for Windows compatibility. Native POSIX restores `R_OK|W_OK|X_OK`; Windows and the inactive source retain their existing probe.

The removal wrapper at 0xc490c matches: `MacFiles_RemoveDirectoryA(path) == 0`. Its formerly empty live provider is now reconstructed separately in the `Mac/Tools/MacFiles` report. It is not exempted as platform code.

`Sys_DefaultCDPath` and `Sys_DefaultInstallPath` match the Mac empty-string and bounded-static-`getcwd` implementations. Native `Sys_DefaultHomePath` intentionally calls `MacSystem_HomePath`: HOME, then passwd fallback, then `Library/Application Support/CoD2 Silicon`. `FS_Startup` registers its path dvars from those explicit providers instead of the Linux getter/setter layer.

The two clock wrappers preserve low-32-bit millisecond arithmetic. Their selected native provider is `MacWin32.c:timeGetTime` -> `MacSystem_Nanoseconds`, backed by `mach_absolute_time`; initialization uses `pthread_once`. Mac 0xb35c instead rounds `Microseconds` to the nearest millisecond. The monotonic native clock and its floor conversion are an explicit platform replacement, also used by native deadline pacing.

`strlwr` at 0x25916 was compared with the selected `i5stdlib.c` byte loop, signed promotion, `tolower` and original-pointer return. Sound aliases, model material names and gametype scripts still call it. `Sys_FreeFileList` matches the null guard, sentinel walk and final array free.

Both native executables compile and link. Existing clock/network fixtures pass, including HOME fallback. The listing and permission corrections were compiled and reviewed but not exercised. Inactive source checks pass; the changed files contain no `__LINE__` expansions to drift. No tests were added and no deletion was executed.

Seven Linux-only identities remain unchecked: the three critical-section functions, `Sys_GetCurrentUser`, `Sys_GetProcessorId`, `Sys_NullQuery3` and `fastftol`. Their exact Mac identity or native replacement contract is unresolved.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-unix-runtime.noindex/`.

## Remaining-identity audit (batch 22)

All seven remaining rows stay unchecked after reviewing actual candidate owners:

- The three critical-section aliases describe an indexed global mutex layer. Mac `CMutex` construction at 0x1173fe and selected `macos_threads.c` own individual recursive mutexes with construction-error propagation, and `StMutexLock` owns per-object lock lifetimes. No index-to-object or original caller mapping was established. Exact Mac `Dvar_ForEach` at 0x4f76c directly traverses and invokes callbacks without the Linux critical-section layer. That prevents inferring a universal inline lock; it does not establish the missing functions.
- `Sys_GetCurrentUser`: exact Mac `Sys_Init` at 0xc39b2 initializes input and prints hardware information rather than registering the Linux username dvar. Native `CL_GetUsernameForLocalClient` uses the player-name dvar; `MacSystem_HomePath` uses passwd only to obtain a directory. Neither proves an OS-login-name replacement.
- `Sys_GetProcessorId`: the Mac symbol table has only zero-valued absolute `__Z18Sys_GetProcessorIdv.eh` metadata. The executable CPU-frequency, memory-size and SSE queries at 0xc4858, 0xc4876 and 0xc48b4 have concrete selected owners in `mac_configure.c`, but their quantities/Boolean do not identify a processor-enum API.
- `Sys_NullQuery3` is explicitly a reconstruction-assigned unknown name. No original Linux executable address/caller or Mac executable identity was established.
- `fastftol` has no established original scalar-conversion identity. The separately approved `Sys_SnapVector` rounding decision cannot certify it: width, exceptional-input behavior and original call sites remain unproved.

Existing reconciled statuses and evidence, including `Sys_SnapVector`, are preserved exactly in the unit ledger.

This follow-up changes reports only and finds no proven source defect. The full 101-row unit ledger preserves all 78 previously resolved status/evidence pairs exactly and refines the reasons for the 23 unchecked rows. No new row is certified. The existing `macos_system_network_test` and `macos_threads_ring_test` targets compiled in a private Debug build with two low-priority jobs; they were not run and do not prove any missing identity. No production build, new test/case/assertion, full CI, replay, push or external side effect was performed. Source and inactive builds are unchanged. Private inspection material is under `~/Library/Application Support/CoD2-native-decomp/recon-unix-remaining.noindex/`.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
