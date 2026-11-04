# client/cl_net_chan_mp module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 4 equal, 1 fixed, 1 intentional.** Complete Mac 1.3 bodies were compared with the native code and its callers. All six entries were also checked with the call/string triage and constant tools. Exact-symbol disassembly corrected the full-binary tool's misaligned `CL_Netchan_Decode` prologue. The inactive source is unchanged.

## Correction

`CL_Netchan_PrintProfileStats` now reads `cl_profileTextHeight`, as Mac 0x19046b and the other draw sites do. Native previously hardcoded a height of 10. Its existing registration supplies the Mac default of 16 and range 1–100; `Dvar_GetInt` retrieves the registered integer without requiring another global. Mac 0x1903f0–0x19041d draws the receive row once before formatting the total. Native drew it twice, displacing the total. Console output, both absent-profile branches, both streams' statistics, the sums and all overlay coordinates were compared.

## Preserved contracts

The decoder and transmitter use the same reliable-command rings, low-byte key components, unsigned character shifts and string restart index as Mac. A zero challenge remains valid. `CL_PacketEvent` supplies the bytes after the acknowledgement; `CL_WritePacket` supplies the nine-byte clear prefix followed by the compressed bytes and drains fragments with the forwarding wrapper.

The decoder retains opt-in native packet tracing, so its verdict is intentional. The OOB functions match the profiling gate, preparation order, client socket, by-value address and accounting arguments. Typed connection, netchan and pointer-free profile-stream fields preserve the native allocation contracts.

## Verification and limits

The arm64 Release `cod2_macos_ded` target linked at low priority with two jobs. That target excludes these client files, so both changed client translation units were also compiled to arm64 objects with the actual client flags, and again with CoD2x, network hardening and WWW download enabled. All four compiles passed. The parser has its pre-existing `strlen` narrowing warning.

The five inactive preprocessing configurations and actual SDK preprocessing with `COD2_X64` removed have no differences. The changed files' active raw-offset inventory has no new violations, and `git diff --check` passed. No tests, replays or full CI were run. The correction is compiled and reviewed, not exercised. No entries remain unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-client-network.noindex/`.

## Integrated batch verification

Integrated source `3ee60be` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, both function/callback ABI audits and existing source fixtures. The screen/main cinematic callback resolves in both complete client links. All twelve changed source files and two changed fixture files reported by the guard checker differ only under `linux-x86_64-port-on`; the remaining fixture adapters preserve inactive configurations. No changed production source uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews and independent source/binary reviews covered all six units and the existing fixture corrections. Review corrected cinematic initial state and an autocomplete scan that could cross the input terminator. The final bounded implementation was compiled and re-reviewed. No unresolved introduced-code finding remains. Main client GPU synchronization and its complete frame caller remain unchecked pending the renderer/timer contract audit. The existing wide time average remains intentional for the measured CoD2x epoch regression; Windows auto-update remains outside the documented native updater boundary.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed network, cinematic, console or UI edge; unreached corrections remain compiled and reviewed.

The initial catalog coverage step failed because the restored `unskippablecinematic` registration was absent from the catalog. Two deterministic generations and a structural comparison established that the only functional catalog addition was that command; other changes were revision/line metadata. The complete catalog CI step passed on rerun. The existing Swift decoder fixture initially pinned an older revision; its expected revision was updated without removing assertions, and all decoder checks passed. Original failure logs are preserved.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written. Existing clock, command, authorization, map-loading and match-lifecycle fixtures received the adapters or setup corrections needed by the reconciled interfaces; assertions were retained.
