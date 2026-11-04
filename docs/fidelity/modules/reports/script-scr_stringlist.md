# script/scr_stringlist module reconciliation

Updated 2026-11-04.

**39/42 reconciled: 25 equal, 14 fixed; 3 unchecked.** Mac 1.3 i386 is the authority. Linux names were used to identify inline helpers; no reference source was copied. The legacy preprocessed source is unchanged.

All 25 exported Mac bodies were compared, including hashing, circular collision chains, move-to-head lookup, three insertion placements, string reclamation, restart traversal, user/refcount transitions and formatted string wrappers. Fourteen Linux-only rows were identified in those concrete callers. Format/error string bytes were checked directly in the Mach-O file. The hash uses signed bytes, 32-bit wrap, modulo 16383 plus one for lengths below 256, and `(len >> 2) % 16383 + 1` otherwise.

## Corrections

**Filename overflow reported the wrong cursor.** Mac 0x43f3d passes the advanced source pointer to `Com_Error`; native passed the original filename. Mac also sign-extends each input byte before lowercase conversion (0x43e97 and 0x43f25), while native converted an unsigned byte. Both details now match.

**Speculative corruption workarounds changed the Mac paths.** Native bounded searches in lookup, interning, unlinking and restart could return, allocate or recycle an unrelated hash bucket after their limit. Mac 0x42ec4–0x42f20, 0x432af–0x43311, 0x42c6a–0x42c83 and 0x43a93–0x43aac follow links until their normal stop. Native also suppressed reference additions when its side cache considered a matched string free; Mac 0x4336a–0x43388 and 0x43488–0x4349a applies the user/refcount transition directly. These workarounds are now excluded from `COD2_X64`; their legacy branches remain. An extra free-list-head check in the empty target-slot insertion path was also excluded, matching 0x43217–0x43264. These differences concern inconsistent/corrupt tables; they were not runtime exercises.

**Lowercasing a zero id skipped the reference reads.** Mac 0x43873–0x4390e enters its lowercase loop using the converted pointer; native skipped that loop for a null pointer and passed an uninitialized local buffer onward. The reconciled path follows the Mac reads. This does not make zero a valid string id.

## Unchecked identities

`SL_GetLowercaseString` (the two-argument Linux wrapper), `SL_TransferSystem` and `Scr_SetStringFromCharString` have no Mac symbols or native definitions/calls under those names. The underscore lowercase implementation is verified, but no concrete Mac inline caller was established for these three distinct functions. Their Linux bodies do not establish equality or prove a missing native implementation.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled in this worker's worktree with low priority and two jobs. All corrected paths were compiled and reviewed, but none was exercised in a runtime scenario. No new tests, full CI, replays or Windows comparison were run. No platform-specific behavior was identified that called for a Windows decision. `COD2_X64=OFF` preprocessing is byte-identical to the assigned base for all three unit source files.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-memory.noindex/`.

## Integrated batch verification

Integrated source `dd6a421` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
