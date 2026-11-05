# Mac/Main/mac_main reconciliation

Updated 2026-11-05.

**9/9 reconciled: 7 equal, 1 fixed, 1 intentional.** These are the nine assigned identities; the separate Unix-runtime work owns the other functions in this file.

`Sys_HasInfoChanged` used the wrong precision for its CPU-change thresholds. Mac `mulsd` at 0xc425c/0xc426a loads doubles from 0x340ca8/0x340cb0 containing `1.100000023841858` and `0.8999999761581421`. They are widened `1.1f`/`0.9f`, whereas native used double literals. The native Apple branch now widens float literals before double multiplication. The signed +/-32-MB tests, case-sensitive GPU comparison and prompt branches match. The constant comparison now reports no float difference; this is a deterministic constant correction, not x87 last-bit chasing.

The 528-byte `Sys_GetInfo` copy at 0xc3510, info dvar names/ranges/flags `0x1011` and archive setter order at 0xc3534 were compared with their actual `Com_SetRecommended` callers. Exact-symbol disassembly corrected a misaligned full-binary entry. Checksum prompting at 0xc4056 and archiving before the computer-change prompt at 0xc3efa match, including re-reading the checksum after the prompt. The native duplicate info registration before `Sys_ArchiveInfo` is idempotent before any stores. Native dialog results come from the existing terminal `MessageBoxA` provider.

`Sys_NormalExit` at 0xc38e8 really is empty, despite live common-engine callers. `Sys_Net_Restart_f` at 0xc39a8 calls `NET_Restart`. `Sys_DirectXFatalError` at 0xc364a retains the localized title/body, flags `0x10` and `exit(-1)` in native Apple; the extra DLL-recovery branch cannot activate through the selected direct-linked renderer. Renderer import callers were checked.

`WinMain` at 0xc3a7a retains the engine initialization/system-info/`Com_Init` responsibilities and frame loop. Native crash setup, dedicated mode/signals, CoD2x hooks and frame observation are explicit additions. Terminal console and engine pacing replace the old Carbon console and 5-ms dedicated GUI sleep; the omitted PB calls are the separately documented null-adapter boundary. Engine info behavior was reviewed separately rather than excluded because this is a platform file.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
