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
