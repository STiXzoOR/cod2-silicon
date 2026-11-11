# qcommon/com_bsp_load_obj module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 5 equal, 1 fixed.** Mac 1.3 i386 is the authority. The legacy source is unchanged.

`Com_LoadBsp` at 0x11154e omitted byte 0x15 between the localized error key and filename in both load-failure paths and the wrong-version path. The native branch now uses the strings at Mac 0x24745c and 0x2542c8. The open/read/close sequence, short-read and 0x140-byte minimum checks, checksum timing, IBSP/version-4 checks, and no-op little-endian lump loop otherwise match.

The getter's optional size/checksum writes and return value match Mac 0x111508; this is the semantic counterpart of the Linux-named `Com_GetBspHeader`. The Linux-named `Com_IsBspLoaded` check is inlined in Mac `Com_CleanupBsp` at 0x111694-0x11169b. Both unload paths were compared, including the unconditional free in `Com_UnloadBsp`.

The full-binary disassembler misaligned the getter entry; a fresh `otool -p __Z10Com_GetBspPiPj` disassembly supplied its actual prologue and body.

String/call triage is clean after the correction. A scripted branch reconstruction confirms `COD2_X64=OFF` source is byte-for-byte unchanged. The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. The correction is compiled and reviewed; no new tests, full CI or replays were run. The changed error paths have not been exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-map-support.noindex/`.

## Integrated batch verification

Integrated engine source through `aa2f246` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `42d8893` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `4e8b534` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
