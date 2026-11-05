# Mac/Main/mac_splash reconciliation

Updated 2026-11-05.

**4/4 wrapper rows reconciled: 4 equal.** Mac create/destroy/hide at 0xc4830/0xc4836/0xc4850 are empty returns, as are native. Show at 0xc483c forwards `cod.png` (decoded at 0x24e260) to `CMacGameEngine_DrawSplashScreen`, as does native.

This does not approve the drawing provider. The client call path remains `WinMain` -> `Sys_ShowSplashWindow` -> the empty `linux_common.c` drawing provider. No equivalent appears in the launcher/bootstrap documentation or code. That separate `CMacGameEngine_DrawSplashScreen` row at 0x25cbc remains unchecked with the missing implementation described in the MacGameEngine report. Dedicated does not call the startup splash path.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
