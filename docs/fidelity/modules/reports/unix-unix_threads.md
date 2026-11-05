# unix/unix_threads reconciliation

Updated 2026-11-05.

**4/5 reconciled: 2 equal, 2 intentional; 1 unchecked.** Exact-symbol Mac bodies at 0x222510, 0x22252e, 0x222548 and 0x22255a were compared with the selected `PC/win32/win_threads.c` provider, which both native targets compile.

Initialization stores the current thread ID before `Com_InitThreadData(0)`. Main-thread checks compare against that saved ID. Mac 0xb00c supplies `pthread_self`; native `MacWin32.c` supplies `pthread_mach_thread_np(pthread_self())`. This is an intentional native ID representation. WinMain initializes it; client console/render checks consume it.

Get/set preserve the original global five-pointer array. It is not per-thread TLS, despite the names. `Com_InitThreadData` writes slots 1, 2 and 3 for `va`, error jump frames and collision trace state. Native pointer width is the only storage adaptation; no guard or thread-local redesign was introduced.

Both native executables compile and link. The existing Debug `threads_ring` fixture passes and distinguishes worker and main IDs; it does not exercise the global engine slot accessors. No source changes or new tests were needed for these four wrappers.

`Sys_NullQuery4` remains unchecked: the Linux-only identity has no established executable Mac body or selected native replacement contract.

## Remaining-identity audit (batch 22)

The remaining `Sys_NullQuery4` row stays unchecked. The reconstruction explicitly says the original name is unknown and the function is unreferenced. The ledger supplies only source/line provenance; no original Linux executable address/caller was established, in addition to the missing Mac executable/inline identity. A constant-return helper or another no-op native provider cannot be assigned by body shape alone. The four established thread wrappers retain their exact prior ledger statuses and evidence.

This follow-up changes reports only and finds no proven source defect. The full 101-row unit ledger preserves all 78 previously resolved status/evidence pairs exactly and refines the reasons for the 23 unchecked rows. No new row is certified. The existing `macos_system_network_test` and `macos_threads_ring_test` targets compiled in a private Debug build with two low-priority jobs; they were not run and do not prove any missing identity. No production build, new test/case/assertion, full CI, replay, push or external side effect was performed. Source and inactive builds are unchanged. Private inspection material is under `~/Library/Application Support/CoD2-native-decomp/recon-unix-remaining.noindex/`.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.
