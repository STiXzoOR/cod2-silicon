# Mac/Main/mac_splash reconciliation

Updated 2026-11-05.

**4/4 wrapper rows reconciled: 2 equal, 2 intentional.** Mac create/destroy/hide at 0xc4830/0xc4836/0xc4850 are empty returns. Native create remains equal; native hide/destroy now intentionally own the Windows-style AppKit splash lifetime. Show at0xc483c retains the forwarding wrapper; the native drawing provider intentionally consumes cod.bmp as recorded in Mac-Tools-MacGameEngine-startup-splash.md.

Historical provider gap, resolved by the batch19 follow-up below: The client call path remains `WinMain` -> `Sys_ShowSplashWindow` -> the empty `linux_common.c` drawing provider. No equivalent appears in the launcher/bootstrap documentation or code. That separate `CMacGameEngine_DrawSplashScreen` row at 0x25cbc remains unchecked with the missing implementation described in the MacGameEngine report. Dedicated does not call the startup splash path.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.

## Batch 19 follow-up

The missing drawing provider is now reconciled by Mac-Tools-MacGameEngine-startup-splash.md. Native Apple startup shows the Windows-style BMP after canonical startup-variable parsing. Hide/destroy intentionally own the AppKit window; create/show retain their wrapper verdicts. Actual startup visibility remains unexercised.
