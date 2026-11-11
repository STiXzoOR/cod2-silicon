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

## Integrated batch verification

Integrated engine source through `aa2f246` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `42d8893` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `4e8b534` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.

## Original OOB owner refinement (storage10)

SV_Netchan_SendOOBPacket remains unchecked. Complete original1.3 body0x80976aa checks the -1 packet header and reports a non-OOB diagnostic, always prepares server OOB profiling, sends, then updates send profiling. Existing NET_SendPacket/NET_OutOfBandData operations do not include that whole composition. Original1.0 code-reference follow-up adds no actual consumer, and missing callers are not evidence of replacement.


Private original bodies and new exact Mac/provider/state-owner evidence are retained in recon-provenance-storage5.noindex and recon-provenance-storage10.noindex. No source changes, compilation, tests, CI, replay or runtime exercise occurred. Missing names/callers are not completion evidence.
