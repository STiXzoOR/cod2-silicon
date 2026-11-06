# qcommon/statmonitor module reconciliation

Updated 2026-11-04.

**3/3 reconciled: 2 equal, 1 fixed.** Mac 1.3 i386 is the authority. The legacy source is unchanged.

## Corrections

**The invalid-entry message lacked its control byte.** Mac `StatMon_Warning` at 0xaebfc loads the string at 0x249e30 through 0xaec4c; that string starts with 0x15. The native branch now passes the same bytes to `Com_Error`.

**The renderer callback truncated material pointers.** Mac calls renderer slot four at 0xaeca4 and stores the returned pointer at 0xaecac. The old cast declared an `int` return, losing the upper half of a native 64-bit pointer. The native branch now declares a `MaterialHandle` return. Its compiled arm64 code stores x0 directly after the callback.

The remaining warning logic matches Mac: the enabled gate precedes the unsigned 0..6 check, expiration is current time plus duration, registration occurs only for an empty entry when the renderer is started, arguments are name/0x30/1, and the count grows to type plus one. `StatMon_GetStatsArray` at 0xaecb2 and `StatMon_Reset` at 0xaeccc match the returned storage/count and the reset of all seven entries plus the count, with the native structure's pointer size respected.

## Verification and limits

The arm64 Release `cod2_macos_ded` build completed with exit 0 and compiled this file. Both changed paths were compiled and reviewed, but not exercised. The caller and drawing/reset consumers were read; `tests/fixes13/timing.c` only stubs `StatMon_Warning`, and no direct fixture was found. No tests were added or changed, as requested, and no full CI or replay was run. The `COD2_X64=OFF` source selection was checked byte-for-byte against the starting file.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-utils.noindex/`.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
