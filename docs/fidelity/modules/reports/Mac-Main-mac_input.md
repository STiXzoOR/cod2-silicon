# Mac/Main/mac_input reconciliation

Updated 2026-11-05.

**9/9 reconciled: 3 equal, 6 intentional.** Read every Mac callback, including the complete key fallback and mouse-transition paths. Native Apple SDL input bypasses all seven old `CCallOfDutyEngine` callbacks; it emits engine events directly from `SDL_PumpInputEvents` and `IN_Frame` in `linux_input.c` (the WS3/WS38 boundary).

The motion wrapper at 0x10b818 preserves signed `Point.h/v` ordering. The wheel wrapper at 0x10b830 preserves positive versus nonpositive `0xce/0xcd` press/release pairs. Text at 0x10ba6e preserves `isprint`/backspace acceptance and the message timestamp. These three wrapper bodies match even though Apple SDL bypasses them; native SDL's fractional/multiple wheel steps and UTF8 scalar delivery belong to the documented adapter.

The two key wrappers differ: Mac translates VK special cases, keypad Enter/equals, `MapVirtualKeyA` and extended character-pair searches. Their simplified native bodies and different old table are not called by native Apple input, which uses `sdl_to_keynum`. The two mouse-button wrappers also differ: Mac compares its object mask against shared `sOldButtonState`, whereas the native dormant wrappers lack that transition filter and keep an unvalidated opaque object offset. Native SDL buttons directly emit transitions for the five engine keys. None of those four Carbon callbacks is approved for reuse.

`IN_Init` retains the `in_mouse` registration (`true`, `0x1021`), inactive diagnostic and modified clear, adding the existing `in_rawmouse` preference/SDK initialization. `IN_Shutdown` adds handler and callback-queue drainage to Mac's empty body. Live initialization/restart/quit/error callers were checked. Dedicated excludes this translation unit and has no SDL provider.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.
