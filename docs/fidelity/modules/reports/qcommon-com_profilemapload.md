# qcommon/com_profilemapload module reconciliation

Updated 2026-11-04.

**1/1 reconciled: 1 equal.** Mac 1.3 i386 is the authority. No source change was needed.

`Com_IsMapProfilerActive` at 0x38410 has a prologue, clears eax and returns. Native returns zero. Its exact symbol entry was disassembled and read; call/string triage and the constant check found no behavior difference.

The arm64 Release `cod2_macos_ded` target compiled with two low-priority jobs. No new tests, full CI or replays were run.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-map-support.noindex/`.
