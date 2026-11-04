# qcommon/com_bsp_load_obj module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 5 equal, 1 fixed.** Mac 1.3 i386 is the authority. The legacy source is unchanged.

`Com_LoadBsp` at 0x11154e omitted byte 0x15 between the localized error key and filename in both load-failure paths and the wrong-version path. The native branch now uses the strings at Mac 0x24745c and 0x2542c8. The open/read/close sequence, short-read and 0x140-byte minimum checks, checksum timing, IBSP/version-4 checks, and no-op little-endian lump loop otherwise match.

The getter's optional size/checksum writes and return value match Mac 0x111508; this is the semantic counterpart of the Linux-named `Com_GetBspHeader`. The Linux-named `Com_IsBspLoaded` check is inlined in Mac `Com_CleanupBsp` at 0x111694-0x11169b. Both unload paths were compared, including the unconditional free in `Com_UnloadBsp`.

The full-binary disassembler misaligned the getter entry; a fresh `otool -p __Z10Com_GetBspPiPj` disassembly supplied its actual prologue and body.

String/call triage is clean after the correction. A scripted branch reconstruction confirms `COD2_X64=OFF` source is byte-for-byte unchanged. The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. The correction is compiled and reviewed; no new tests, full CI or replays were run. The changed error paths have not been exercised.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-map-support.noindex/`.
