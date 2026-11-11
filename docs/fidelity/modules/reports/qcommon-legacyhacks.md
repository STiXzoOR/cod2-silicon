# qcommon/legacyhacks module reconciliation

Updated 2026-11-04.

**0/2 reconciled: 2 unchecked.** No code was changed.

`UnusedStubA` and `UnusedStubB` are Linux-only ledger entries. Neither has a native definition or reference, and neither name appears in the Mac symbol table. The Linux reconstruction explicitly says the two functions and its accompanying table are absent from the Mac build and that their names and meaning are unknown.

That is insufficient to identify a Mac implementation or to prove that omitting them is equivalent behavior. Both entries remain unchecked; no placeholder functions or unlicensed table data were added. They require an independently identified original function/address before a behavioral verdict can be made.

The arm64 dedicated build has no legacyhacks translation unit. No tests, full CI or replays were run for these unidentified entries.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-utils.noindex/`.

## Integrated batch verification

Integrated source `24bd887` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
