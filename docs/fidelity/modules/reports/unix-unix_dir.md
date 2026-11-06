# unix/unix_dir reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 fixed.** The directory43 follow-up below establishes the complete actual directory-query role and inherits the prior Mac DT_REG repair. Earlier identity/search checkpoints remain historical.

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


## Original executable provenance follow-up (platform6)

The original 1.0 named Sys_DirectoryHasContents at 0x80d416a, consumed by FS_AddGameDirectoryInternal at 0x80a04b4, is the allocating, path-normalizing universal/unix_shared wrapper. It is not the separate thin Sys_DirectoryHasEntries reconstruction in unix_dir.c. Treating that named original as the unresolved thin helper would conflate two ledger identities. No positive original executable anchor for the thin helper was recovered; its any-entry contract also remains distinct from the Mac provider's filtering. The one row remains unchecked.

This is a report-only provenance audit. No verdict or production source changes, compilation, tests, CI, replays or runtime exercise were performed. Private original body/address/caller packets are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-platform6.noindex/`. Prior verification records above describe earlier work, not this follow-up.


## Complete directory predicate and admission consumer — directory43

`Sys_DirectoryHasEntries` is fixed through the complete actual directory-input, enumeration lifecycle and Boolean-result operation. A fresh original1.3 import-call trace recovered the distinct thin body at0x80d36ac–RET0x80d373f: direct incoming path, failed open returns0, successful open enumerates until a non-dot/dotdot entry or end, closes exactly once, and returns saved fullint0/1. All148 executable bytes and raw dot strings were independently checked. Its reconstructed name supplies intent; the complete original executable operation is new evidence beyond the earlier name search. No separately named original1.0 thin symbol or missing-caller inference is claimed.

The allocating/path-normalizing original HasContents wrapper remains distinct. Its own inline loop at original1.3 0x80d6886–RET0x80d6974 does not call the thin body. The original FS_AddGameDirectory caller corroborates the directory-eligibility purpose, but wrapper copy/allocation/normalization/free effects are not silently assigned to the thin helper.

Fresh full Mac Sys_DirectoryHasContents0xc4e62 receives the actual OS-directory pointer and returns0 without closing on failed open. After a successful open it skips dot-prefixed entries and every non-DT_REG type, saves1 for the first visible regular entry or0 for null readdir, closes exactly once and returns the saved Boolean, ignoring close errors. Native mac_common.c274 preserves the complete operation with native DIR/dirent fields. Original Linux accepts any name except dot/dotdot, so its hidden/nonregular results differ explicitly; the mapping follows Mac behavior authority rather than claiming Linux filter equality.

Actual FS_Startup's nonempty basepath localized loop feeds FS_AddGameDirectory. Its selected new-directory phase builds game/language and a normalized OS path, removes the generated trailing slash, calls the predicate at0x35140, and consumes the entire Boolean at0x35145/47. False exits before allocation; true admits the directory searchpath and its IWD processing. The ordinary defined producer has valid backed strings, a fitting game/language and path length+2<=255 with nonempty normalized output. This is an actual complete result consumer, not a hypothetical path adapter or unused flag. Oversized paths, returned fatal-handler continuations and downstream searchpath allocation behavior are outside the predicate classification.

The whole predicate includes the DT_REG check already repaired in f689a60c and recorded as fixed for primary mac:0x000c4e62. This row inherits that repair; it adds no defect, source change or platform policy. No standalone thin export, wrapper allocation equivalence, arbitrary errno parity, directory snapshot/race guarantee, stat fallback or whole filesystem lifecycle is asserted.

Independent review: units/review-provenance-directory43.md. Private exact bodies, original bytes/import/dot-string receipts, actual producer/consumer evidence and source hashes: recon-provenance-directory43.noindex. No compilation, tests, CI, replays, directory probe or runtime verification in this follow-up.
