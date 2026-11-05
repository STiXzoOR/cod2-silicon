# unix/unix_dir reconciliation

Updated 2026-11-05.

**0/1 reconciled; 1 unchecked.** `Sys_DirectoryHasEntries` has only a Linux ledger identity. No executable Mac symbol, selected native definition or caller was found.

The active native filesystem uses `mac_common.c:Sys_DirectoryHasContents`, whose Mac identity is 0xc4e62. That function accepts only visible regular dirents and has been corrected and recorded in `unix-unix_shared.md`. Similar names do not prove these are the same function or contract. The Linux-only row remains unchecked pending an exact Mac inline identity or a demonstrated provider mapping.

No source changes were made for this module. Both native executables compile; no directory probe was added or run.

## Remaining-identity audit (batch 22)

The remaining `Sys_DirectoryHasEntries` row stays unchecked. Exact-symbol disassembly of Mac `Sys_DirectoryHasContents` at 0xc4e62 confirms its complete loop: skip every dot-prefixed name, accept only type 8 (`DT_REG`), close the directory on completion and return false on open failure. The selected `mac_common.c` provider implements that contract. The Linux-only row describes a broader query that includes hidden files and subdirectories. No original address/caller mapping establishes that row as the same function or a deliberately replaced API. The similar name is not sufficient.

This follow-up changes reports only and finds no proven source defect. The full 101-row unit ledger preserves all 78 previously resolved status/evidence pairs exactly and refines the reasons for the 23 unchecked rows. No new row is certified. The existing `macos_system_network_test` and `macos_threads_ring_test` targets compiled in a private Debug build with two low-priority jobs; they were not run and do not prove any missing identity. No production build, new test/case/assertion, full CI, replay, push or external side effect was performed. Source and inactive builds are unchanged. Private inspection material is under `~/Library/Application Support/CoD2-native-decomp/recon-unix-remaining.noindex/`.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
