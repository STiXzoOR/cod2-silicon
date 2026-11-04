# qcommon/legacyhacks module reconciliation

Updated 2026-11-04.

**0/2 reconciled: 2 unchecked.** No code was changed.

`UnusedStubA` and `UnusedStubB` are Linux-only ledger entries. Neither has a native definition or reference, and neither name appears in the Mac symbol table. The Linux reconstruction explicitly says the two functions and its accompanying table are absent from the Mac build and that their names and meaning are unknown.

That is insufficient to identify a Mac implementation or to prove that omitting them is equivalent behavior. Both entries remain unchecked; no placeholder functions or unlicensed table data were added. They require an independently identified original function/address before a behavioral verdict can be made.

The arm64 dedicated build has no legacyhacks translation unit. No tests, full CI or replays were run for these unidentified entries.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-utils.noindex/`.
