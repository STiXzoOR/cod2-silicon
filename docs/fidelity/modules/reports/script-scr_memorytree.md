# script/scr_memorytree module reconciliation

Updated 2026-11-04.

**20/20 reconciled: 9 equal, 11 fixed.** Mac 1.3 i386 is the authority. Linux names were used to identify inline helpers; no reference source was copied. The legacy preprocessed source is unchanged.

The comparison covers all 13 exported functions and seven Linux-named helpers in their Mac callers. The generated popcount, trailing-zero and bit-length tables match for all 768 bytes. All allocator diagnostic literals were read directly from the binary and compared. Exact-symbol-start disassembly was obtained for every exported function; it corrects the full-binary decoder's misaligned `Scr_GetStringUsage` entry.

## Corrections

**Tree promotion copied only half a bucket.** Mac `MT_AddMemoryNode` at 0x440fe–0x44116 and 0x44141–0x44159 copies complete eight-byte records, including the remaining four bytes of reused payload. Its promotion cascade also copies node 0 at its end. Native copied only the two 16-bit links. Root removal in `MT_AllocIndex` (0x446dc–0x4470e), `MT_FreeIndex` (0x44ff4–0x45026), `MT_Free` and force-allocation rebuilding has the same full-record propagation. The shared helpers now preserve those writes and the left-child choice on equal scores.

**Safe freeing returned too early.** Mac 0x45298–0x4549a removes and reinserts a free block already covering the requested node. Native returned immediately. Restoring the remove/reinsert sequence also preserves the payload movement caused by tree promotion.

**A zero-byte request used the wrong size class.** Mac 0x445d1–0x445e9 computes `(numBytes + 7) / 8 - 1` and indexes the contiguous tables without a lower clamp. For zero, `logBits[-1]` is `numBits[255]`, which is 8. Native clamped that index to zero and also wrapped negative byte indices. The size helper now follows Mac's lookup; allocation, freeing, force-allocation and realloc use that helper.

The force-allocation bitmap has a separate zero-filled contract: Mac `Z_VirtualAllocInternal` at 0x37cf8 calls `VirtualAlloc`, whose NULL-address path at 0xb12c calls `calloc(1, size)`. The native Mac shim matches.

The arena is static storage and starts zeroed, but `MT_Init` clears only tree metadata. Allocation does not clear reused payload. Full-record propagation moves prior contents; it does not establish a zero-initialization contract for callers. Allocation-history-dependent fields remain the caller's responsibility to reconcile.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled in this worker's worktree with low priority and two jobs. All corrected paths were compiled and reviewed, but none was exercised in a runtime scenario. No new tests, full CI, replays or Windows comparison were run. No platform-specific behavior was identified that called for a Windows decision. `COD2_X64=OFF` preprocessing is byte-identical to the assigned base for all three unit source files.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-script-memory.noindex/`.

## Integrated batch verification

Integrated source `24bd887` passed both dedicated builds and every local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eight changed source files, all differing only under `linux-x86_64-port-on`. Independent source/binary reviews covered the six units and relevant callers. Review caught an MSVC-incompatible variable-length temporary in the string-concatenation correction; it was replaced with the existing compiler-compatible stack allocation before integration. No unresolved changed-code findings remain. MSVC itself was not built; its configured compatibility mapping was inspected.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Those unchanged traces do not establish that every corrected edge was exercised. Allocation failures, debugger/error edges and other unreached paths remain compiled and reviewed.

Generic VM recovery, allocation-dependent notify state and missing helper identities remain explicitly unchecked where evidence was insufficient. No undefined native state was introduced to claim parity. Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile; preprocessing checks are not binary-byte parity.
