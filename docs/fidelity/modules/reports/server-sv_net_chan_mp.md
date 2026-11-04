# server/sv_net_chan_mp module reconciliation

Updated 2026-11-04.

**7/8 reconciled: 6 equal, 1 fixed, 1 unchecked.** The Mac 1.3 symbol bodies and native source were compared by hand after triage and constant checks. The encoder and profile-update helpers are inlined in their Mac callers. Full-binary `otool` misdecoded the `SV_Netchan_Decode` prologue; exact-symbol disassembly at 0x1907cc resolved it. The legacy preprocessed source is byte-identical to the starting commit.

## Corrections

**Server profile output lost its drop and acknowledgement columns.** Mac 0x19104e–0x1910f9 and 0x191328 derive packet loss and trailing pending packets from the 32 snapshot frames. Native omitted both calculations and columns. The native implementation now follows the original header, fragment percentages, loss/pending counts, OOB formatting and configurable `cl_profileTextHeight`. The existing headless dedicated routing to the console is retained.

**The last profile columns expose an original format bug.** Mac 0x190d59–0x190dd4 places the total-byte integer at stack offset 0x34, then a maximum-size double at 0x38 and minimum-size double at 0x40. The format at 0x2dbab4 consumes three integer words after total bytes, so it reads the maximum double's low/high words and the minimum double's low word. The OOB and client rows do the same. Windows confirms this layout at 0x45cee9–0x45cf1e and 0x45d2c3–0x45d30f with the same integer format. Native explicitly extracts those three words and passes integers; it preserves the original output without depending on undefined native variadic behavior.

## Verification and limits

The rolling byte XOR encoder/decoder, four-byte unencoded header, key sources, string restart, transmit/fragment wrappers, OOB profiling and profile-stat update gates match Mac. Optional native packet traces remain diagnostic instrumentation.

`SV_Netchan_SendOOBPacket` remains unchecked. It has no Mac symbol and no native body. The Mac symbol/string inventory and `NET_SendPacket`/`NET_OutOfBandData` callers were inspected, but no equivalent inline block for the Linux helper's unconditional preparation, non-OOB diagnostic and send/profiling wrapper was identified. No equality or intentional-replacement claim is made from its absence.

An arm64 Release `cod2_macos_ded` build succeeded with two low-priority jobs. The non-dedicated profile branch also compiled to an object; it reads `cl_profileTextHeight` through the existing native dvar lookup rather than adding a shared global. Profiling changes are compiled and reviewed, not exercised. No new tests, full CI or replays were run; parent integration owns runtime validation.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-server-snapshot.noindex/`.
