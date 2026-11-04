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
