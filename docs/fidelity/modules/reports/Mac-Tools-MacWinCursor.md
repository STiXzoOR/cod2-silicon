# Mac/Tools/MacWinCursor reconciliation

Updated 2026-11-05.

8/8 reconciled: 2 equal, 6 intentional. Read the Mac cursor selector at 0xbe94, animation callback at 0xc018, WinIcon loader at 0xbae4, cursor-file loader at 0xc6a8 and static initialization/destruction at 0xc090/0xc160/0xc170. The repeated local static-init symbol was resolved by its address rather than using another module's identically named symbol.

SwitchToMacCursor 0xbff2 and SwitchToWinCursor 0xbfd2 preserve the saved-handle guards and clear-before-restore order. Their visual provider is intentionally different. Native linux_input IN_Frame owns SDL relative capture and SDL_ShowCursor; the game UI draws the in-game cursor while focused. No native caller uses the old WinCursor registry or loads the 32x32 ICO/CUR/ANI QuickDraw objects. Windows uses USER32 cursor APIs (LoadCursorA at 0x4663ee), while native uses SDL. SetWinCursor retains only compatibility bookkeeping, and no Carbon animation timer or C++ vector initializer/destructor is registered.

The WinIcon and WinCursor stubs are documented as retired backend boundaries, not equivalent parsers. Their opaque placeholder storage is not populated with speculative 0x44-byte i386 objects. No source changed in this module. The selected provider/source graph compiled as part of the arm64 Release dedicated build; the native SDL input path and cursor visuals were inspected but not exercised. No new tests or replay ran.
