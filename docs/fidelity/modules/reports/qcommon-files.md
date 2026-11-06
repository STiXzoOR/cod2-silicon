# qcommon/files module reconciliation

Updated 2026-11-04.

**4/4 assigned ledger entries reconciled: 2 equal, 2 fixed.** The assigned functions are `FS_Dir_f`, `FS_NewDir_f`, `FS_PureServerSetReferencedIwds` and `FS_TouchFile_f`; other functions in this translation unit were outside this ledger assignment. Each assigned function was read against Mac 1.3 and its strings were decoded from the binary.

## Corrections

**Filtered listings ignored pure-only behavior.** Mac `FS_NewDir_f` at 0x4694c passes zero as the fifth `FS_ListFilteredFiles` argument, `FS_LIST_PURE_ONLY`. The native ABI repair supplied `FS_LIST_ALL`. The existing native branch now passes the reference value, so restricted/pure search paths control loose-file listing. The sort, path conversion, print loop and release already matched.

**Referenced-IWD mismatch errors had different text.** Both Mac error paths at 0x47305 and 0x4736e use `iwd sum/name mismatch`, the literal at 0x24347c. The native branch now uses that exact text. Checksum/name tokenization, the 1024-item cap, old-reference shutdown, NULL/empty names, copy loops and final count matched.

`FS_Dir_f` matches the argument gate, optional extension, listing behavior, output and release. It caches the unchanged command argument count instead of reading it three times. `FS_TouchFile_f` matches both its usage and success branches.

## Verification and limits

The isolated arm64 Release `cod2_macos_ded` build passed with two jobs at low priority. Both changed translation units compiled. The inactive-source guard against 27d1e9b reports zero mismatches in ten legacy configurations; this is a token check, not a byte-identical i386 build. No new tests, full CI or replays were run. These corrections are compiled and reviewed, not exercised. No assigned entry remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-profile.noindex/`.

## Integrated batch verification

Integrated engine source through `868a08f` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `b1f110d` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `b275adb` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
