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
