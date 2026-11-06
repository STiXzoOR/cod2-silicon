# script/scr_tempmemory module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 5 equal.** Mac 1.3 i386 is the authority. The source did not change.

All five bodies at 0xba0a4–0xba168 were compared, with exact-symbol-start disassembly and the constant checker. `TempMemoryReset` clears the position. `TempMalloc`, `TempMallocAlign` and `TempMallocAlignStrict` all use the same unaligned sequence: add the requested length, reallocate to that size, return the base plus the old position, then store the new position. The two alignment names introduce no extra padding on Mac. `TempMemorySetPos` retains both reallocation calls and the pointer-delta arithmetic.

The arm64 Release `cod2_macos_ded` target compiled in this worker's worktree with low priority and two jobs. No new tests, full CI or replays were run, and these helpers were not exercised separately. `COD2_X64=OFF` preprocessing is byte-identical to the assigned base.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-memory.noindex/`.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
