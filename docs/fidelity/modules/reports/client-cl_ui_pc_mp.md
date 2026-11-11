# client/cl_ui_pc_mp module reconciliation

Updated 2026-11-04.

**5/5 reconciled: 3 equal, 2 fixed.** Complete Mac 1.3 add/remove/address and cache load/save bodies were compared, including reverse pruning, adjacent duplicate handling, record copies, short-read failures and sorting.

## Corrections

Mac 0x1c7b57–0x1c7ba8 and 0x1c7cfe–0x1c7d54 validate/write the combined size and both complete server arrays. Native retained lengths for 136-byte records and wrote a global-only size marker. The actual native patch-1.3 record is 148 bytes, confirmed by compilation of a type-layout probe. The global array occupies 2,960,000 bytes and favorites occupy 18,944 bytes.

All native read/write extents and the combined 2,978,944-byte marker now derive from `sizeof` the actual arrays. Version 1 remains. The size marker rejects the old truncated native files and other raw layouts; no cache conversion is introduced. Native retains its existing IPX/address layout, which differs from Mac's 140-byte records. The OFF source retains its original constants.

## Verification and limits

No new tests, full CI or replays were run. The arm64 Release dedicated target built, and the client translation unit compiled separately with the configured client flags. All five OFF preprocessing configurations match the base; this file has no `__LINE__` expansions. The cache correction is compiled and reviewed, not exercised with filesystem round trips. No rows remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-screen.noindex/`.

## Integrated batch verification

Integrated source `ca4ed64` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
