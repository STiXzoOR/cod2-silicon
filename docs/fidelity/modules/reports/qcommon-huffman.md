# qcommon/huffman module reconciliation

Updated 2026-11-04.

**13/19 reconciled: 12 equal, 1 fixed; 6 unchecked.** Mac 1.3 is the authority. All 19 entries have verdicts. The legacy preprocessed source is unchanged.

## Correction

**A leaf head slot retained a different node.** In `Huff_addRef`, Mac 0x191a6e stores the newly allocated internal node in the leaf's allocated head slot when the next list node exists with weight other than one. Native stored the leaf. The native path now copies Mac's store under `COD2_X64`. The preceding insertion normally makes that next node the weight-one internal node, so the corrected branch is not expected during normal tree training; it has not been runtime exercised.

## Comparison

`Huff_Init`, `Huff_addRef`, `increment`, both offset codecs and the recursive sender were read against Mac. The comparison included both node/list insertion paths, tree replacement, all equal-weight head and freelist transitions, adjacent-parent list correction, root-to-leaf output order, byte clearing and null-node receive behavior.

The Linux helper `send` corresponds to native/Mac `huffman_send`. The other static helper rows were checked in the Mac callers that inline them: bit reads at 0x191765, bit writes throughout 0x191b44–0x191fe0, head-slot allocation at 0x19180a/0x191943, freeing at 0x1917da and tree/list swaps at 0x1918a7/0x191843. Native pointer-width adaptations preserve these operations.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. Both changed source files were preprocessed with `COD2_X64` undefined and compared with the base commit; the outputs are identical. The correction is compiled and reviewed, not runtime exercised. No new tests, CI suite or replays were run.

Six Linux-only interfaces remain unchecked: `Huff_Compress`, `Huff_Decompress`, `Huff_Receive`, `Huff_getBit`, `Huff_putBit` and `Huff_transmit`. There is no native body or call and no matching Mac text symbol. The fixed-tree offset paths were compared, but an exact Mac identity for these unused standalone interfaces was not established.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-msg.noindex/`.

## Integrated batch verification

Integrated source through `12d554b` passed both dedicated builds and all local macOS CI steps 02–13, including both Release client builds, ABI audits and existing source fixtures. The first source-fixture run exposed an outdated locale-14 decimal-separator expectation; the existing fixture was corrected from period to comma against Mac and Windows evidence, then the focused fixture and complete source-fixture step passed. Scoped legacy preprocessing differences are confined to `linux-x86_64-port-on`; the download file also passed an explicit off-configuration check. Two independent reviews covered the six units' changed source and nearby callers against binary evidence, with no actionable findings. These reviews were not second whole-module audits.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and maximum velocity drift 2.0. Unchanged traces do not establish coverage of each correction. Branches not reached by the recording remain compiled and reviewed.

The hosted ILP32 byte-comparison job cannot compare binaries: its unchanged `410342a` reference fails to compile GCC `__stdcall`/`__cdecl` declarations. The workflow already records that limitation and marks the job non-blocking; preprocessing equality is not binary-byte parity.
