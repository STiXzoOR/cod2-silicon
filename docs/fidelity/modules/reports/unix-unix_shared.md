# unix/unix_shared reconciliation

Updated 2026-11-05.

**20/26 reconciled: 5 equal, 5 fixed, 10 intentional; 6 unchecked.** The previously reconciled `Sys_SnapVector` Windows-rounding verdict and evidence are preserved exactly.

`Sys_ListFilteredFiles` stopped one result early. Mac 0xc4977 and 0xc4a30 stop only above 0xFFE, allowing entry 4094; native used `>=`. Recursion, path formatting, dot entries, filters, string allocation and directory closure were compared by hand.

Both listing functions used `S_ISDIR`, while Mac 0xc4a1d, 0xc4bec and 0xc4c58 test only mode bit 0x4000. Native now preserves that quirk, including socket classification. `Sys_ListFiles` otherwise retains the suffix comparison, hidden-name exclusion, slash sentinel, 4095-entry bound, filter delegation and NULL termination. Native pointer-list allocation deliberately uses `sizeof(char *)`.

`Sys_DirectoryHasContents` counted every visible entry. Mac 0xc4e87–0xc4e97 accepts only visible `DT_REG` entries, rejecting directories, symlinks and unknown types. The native helper now does the same. `FS_AddGameDirectory` uses this result to decide whether a localized directory qualifies.

`Sys_Mkdir` had reduced Mac 0xc48e2's access mode 7 to existence-only for Windows compatibility. Native POSIX restores `R_OK|W_OK|X_OK`; Windows and the inactive source retain their existing probe.

The removal wrapper at 0xc490c matches: `MacFiles_RemoveDirectoryA(path) == 0`. Its formerly empty live provider is now reconstructed separately in the `Mac/Tools/MacFiles` report. It is not exempted as platform code.

`Sys_DefaultCDPath` and `Sys_DefaultInstallPath` match the Mac empty-string and bounded-static-`getcwd` implementations. Native `Sys_DefaultHomePath` intentionally calls `MacSystem_HomePath`: HOME, then passwd fallback, then `Library/Application Support/CoD2 Silicon`. `FS_Startup` registers its path dvars from those explicit providers instead of the Linux getter/setter layer.

The two clock wrappers preserve low-32-bit millisecond arithmetic. Their selected native provider is `MacWin32.c:timeGetTime` -> `MacSystem_Nanoseconds`, backed by `mach_absolute_time`; initialization uses `pthread_once`. Mac 0xb35c instead rounds `Microseconds` to the nearest millisecond. The monotonic native clock and its floor conversion are an explicit platform replacement, also used by native deadline pacing.

`strlwr` at 0x25916 was compared with the selected `i5stdlib.c` byte loop, signed promotion, `tolower` and original-pointer return. Sound aliases, model material names and gametype scripts still call it. `Sys_FreeFileList` matches the null guard, sentinel walk and final array free.

Both native executables compile and link. Existing clock/network fixtures pass, including HOME fallback. The listing and permission corrections were compiled and reviewed but not exercised. Inactive source checks pass; the changed files contain no `__LINE__` expansions to drift. No tests were added and no deletion was executed.

Six Linux-only identities remain unchecked: the three critical-section functions, `Sys_GetCurrentUser`, `Sys_GetProcessorId` and `Sys_NullQuery3`. Their exact Mac identity or native replacement contract is unresolved. The consumed scalar conversion for `fastftol` is reconciled below.

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

Integrated source a3c4c7c passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.


## Original executable provenance follow-up (platform6)

Original executable provenance now identifies fastftol at 1.0 0x80d37fe and 1.3 0x80d5e6e: save x87 control word, select truncation, FISTP to 32-bit storage, restore control word and return the integer. This does not identify a particular Mac conversion owner or reopen the established Windows-style SnapVector decision.

The original 1.3 indexed critical-section API is concrete: initialization 0x80d67da creates six recursive mutexes at 0x860b340 with a 24-byte stride; Enter 0x80d6842 and Leave 0x80d6864 compute that indexed address and call pthread lock/unlock. The retained direct-call inventory has 61 calls within 29 bounded owner bodies: 48 calls with index 4 in dvar operations, eight with index 2 in error handling, two each with indices 0 and 5 in logging/output, and one initializer in startup. This refines the missing ownership contract; it does not establish equality to Mac's per-object CMutex API. A complete replacement account across these owners, including exceptional continuations, remains missing. No lock removal or new threading policy is approved here.

Original Sys_Init at 0x80d3ad6 calls Sys_GetCurrentUser 0x80d6760 at 0x80d3b17 and registers the username dvar. This positive original use still does not identify Mac player-name or home-path logic as the same operation. Source-order anchors place GetProcessorId at 0x80d6746 and NullQuery3 at 0x80d6750; constant-return shape supplies no CPU-enum or anonymous-query Mac identity. All seven rows remain unchecked.

This is a report-only provenance audit. No verdict or production source changes, compilation, tests, CI, replays or runtime exercise were performed. Private original body/address/caller packets are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-platform6.noindex/`. Prior verification records above describe earlier work, not this follow-up.
## Complete scalar conversion in CrashLand

`fastftol` is fixed through the already corrected float32-to-signed32 operation consumed by CrashLand. Complete original Linux bodies at 1.0 0x80d37fe and 1.3 0x80d5e6e load a four-byte float and return a truncating four-byte integer result. The reconstructed `long` spelling describes that original ILP32 result; it does not establish a native LP64 long API. No original external caller is invented.

Primary Mac conversions at 0x70974 and 0x70a50 consume scalar single-precision values and return signed32 integers before separate damage/view-dip selection. Native `PM_CrashLandFloatToInt` accepts exactly [-2^31,2^31), truncates valid values and returns INT_MIN for NaN, infinity or out-of-range values without an invalid C cast. This matches the complete returned conversion result, including signed zero, subnormals and -2^31, when the Mac instruction returns with the relevant exception masked. The fixed classification inherits the earlier CrashLand repair; no additional source defect or change is counted.

This bounded consumed-operation mapping claims no standalone export, 64-bit long ABI, x87/MXCSR flags, control-word side effects, trap mode or floating-point environment equivalence. The approved Windows-style `Sys_SnapVector` rounding is unchanged. Current movement source matches the independently reviewed four-configuration manual compilation receipt; no compilation, tests, CI, replay or runtime exercise ran for this follow-up. Compiled/reviewed does not mean runtime verified or playable. Exact evidence and independent review are retained in `recon-provenance-fastftol12.noindex` and `units/review-provenance-fastftol12.md`.


## Platform14 indexed critical-section ownership

The three indexed critical-section rows remain unchecked. This pass expands the
platform6 call inventory into complete original bodies for 28 named owners and a
29-owner map retaining the startup call. There are 61 direct calls: 48 using dvar
index 4, eight using error index 2, two each using logging index 0 and redirect
index 5, and one initializer. Original 1.3 initialization at 0x80d67da constructs
six recursive mutexes, 24 bytes apart at 0x860b340, ignores pthread result codes,
and destroys the attribute. Enter 0x80d6842 and Leave 0x80d6864 compute the indexed
address and pass it to pthread lock/unlock. Generic per-object CMutex construction,
including its checked error propagation, is not that complete ownership contract.

The recovered continuations make several proposed shortcuts invalid:

- Com_PrintMessage holds index 5 through its redirect callback at 0x8060c92 and
  index 0 through the recursive logfile-opening Com_Printf at 0x8060d83. Its
  normal exits release the respective lock. Fresh exact Mac 0x2df82 and native
  common.c preserve the callback, buffer reload, opening guard and recursive
  print directly. Those operation owners do not identify a global mutex array.
- Com_Error releases index 2 at 0x80611d3 before obtaining TLS key 2 and jumping;
  Com_ErrorAbort snapshots the error flag under that lock and releases it at
  0x8061215 before its conditional jump. Com_Frame instead acquires index 2 after
  setjmp and holds it through error cleanup, releasing on either normal branch.
  Com_Quit_f acquires it at 0x8061257 and reaches terminal Sys_Quit without a
  release. Fresh Mac Error 0x2e33a, Frame 0x306c4 and Quit 0x2f940 have concrete
  direct flag/TLS/cleanup/exit owners, with the established returning-error and
  native platform adaptations preserved. They cannot certify ErrorAbort's
  separately unresolved operation or imply a common lock lifetime.
- Dvar_ResetDvars 0x80b53e4 holds index 4 and calls Dvar_Reset 0x80b5092, which
  acquires the same recursive lock. Dvar_LoadDvarsFromBuffer 0x80b52ee likewise
  holds it while calling the locking lookup at 0x80b5350. RegisterNew releases
  before its capacity-error call at 0x80b3caa. Fresh exact Mac ResetDvars 0x547c0
  and Reset 0x53792 directly pass the reset value and source to SetVariant; their
  native implementations do the same. This positive consumed operation accounts
  for reset behavior, not all indexed dvar ownership.

Four named owners in that map remain independently unresolved: Com_ErrorAbort,
Dvar_SetVec2FromSource, Dvar_SaveDvars and Dvar_LoadDvarsFromBuffer. The latter
binary serialization pair is not the settled named text-loader operation. Neither
existing verdicts on other owners nor a single-thread assumption can fill those
specific gaps. No new synchronization policy is proposed and no source defect was
proved. A future complete replacement must account for storage identity, recursion,
callback reentry, error continuations and terminal ownership for the actual owners;
otherwise these three entries remain open.

The settled fastftol12 consumed conversion verdict is preserved unchanged.

Private evidence is retained in recon-provenance-platform14.noindex, including the
full original owner bodies, fresh exact Mac owners and owner-boundaries.json.
Three focused unchecked refinements; zero closures or defects. Static review only;
no source change, compilation, tests, CI, replay or runtime verification.
