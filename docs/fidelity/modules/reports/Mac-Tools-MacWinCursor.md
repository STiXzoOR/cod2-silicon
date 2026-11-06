# Mac/Tools/MacWinCursor reconciliation

Updated 2026-11-05.

8/8 reconciled: 2 equal, 6 intentional. Read the Mac cursor selector at 0xbe94, animation callback at 0xc018, WinIcon loader at 0xbae4, cursor-file loader at 0xc6a8 and static initialization/destruction at 0xc090/0xc160/0xc170. The repeated local static-init symbol was resolved by its address rather than using another module's identically named symbol.

SwitchToMacCursor 0xbff2 and SwitchToWinCursor 0xbfd2 preserve the saved-handle guards and clear-before-restore order. Their visual provider is intentionally different. Native linux_input IN_Frame owns SDL relative capture and SDL_ShowCursor; the game UI draws the in-game cursor while focused. No native caller uses the old WinCursor registry or loads the 32x32 ICO/CUR/ANI QuickDraw objects. Windows uses USER32 cursor APIs (LoadCursorA at 0x4663ee), while native uses SDL. SetWinCursor retains only compatibility bookkeeping, and no Carbon animation timer or C++ vector initializer/destructor is registered.

The WinIcon and WinCursor stubs are documented as retired backend boundaries, not equivalent parsers. Their opaque placeholder storage is not populated with speculative 0x44-byte i386 objects. No source changed in this module. The selected provider/source graph compiled as part of the arm64 Release dedicated build; the native SDL input path and cursor visuals were inspected but not exercised. No new tests or replay ran.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
