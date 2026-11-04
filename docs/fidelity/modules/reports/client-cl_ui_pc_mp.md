# client/cl_ui_pc_mp module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 3 equal, 2 fixed.** Complete Mac 1.3 add/remove/address and cache load/save bodies were compared, including reverse pruning, adjacent duplicate handling, record copies, short-read failures and sorting.

## Corrections

Mac 0x1c7b57–0x1c7ba8 and 0x1c7cfe–0x1c7d54 validate/write the combined size and both complete server arrays. Native retained lengths for 136-byte records and wrote a global-only size marker. The actual native patch-1.3 record is 148 bytes, confirmed by compilation of a type-layout probe. The global array occupies 2,960,000 bytes and favorites occupy 18,944 bytes.

All native read/write extents and the combined 2,978,944-byte marker now derive from `sizeof` the actual arrays. Version 1 remains. The size marker rejects the old truncated native files and other raw layouts; no cache conversion is introduced. Native retains its existing IPX/address layout, which differs from Mac's 140-byte records. The OFF source retains its original constants.

## Verification and limits

No new tests, full CI or replays were run. The arm64 Release dedicated target built, and the client translation unit compiled separately with the configured client flags. All five OFF preprocessing configurations match the base; this file has no `__LINE__` expansions. The cache correction is compiled and reviewed, not exercised with filesystem round trips. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.
