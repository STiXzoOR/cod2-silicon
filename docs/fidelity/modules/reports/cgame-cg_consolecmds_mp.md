# cgame/cg_consolecmds_mp module reconciliation

Updated 2026-11-04.

**13/13 reconciled: 11 equal, 1 fixed, 1 intentional.** All assigned Mac bodies were read, including the exact entry of `CG_SizeUp_f`, which the full-binary disassembly misaligned. The legacy source is unchanged.

Mac command data at 0x36e740 contains 18 entries. Native had extra `tcmd` and `tell_target` dispatch entries and a different order. The native table now matches all Mac names and handlers, including `mp_QuickMessage` and the null-handler `mr`. `CG_InitConsoleCommands` at 0x1d3180 adds 23 further completions; native's extra `tell` completion was removed. The server/game `tell` command remains registered by its own owners.

The two extra client dispatch entries came from initial commit 63de35a. No native consumer or CoD2x implementation uses either name; both names are absent from the Mac and Windows binaries. WS47 explicitly left these research-only differences unresolved. This reconciliation closes that gap. The existing native current-snapshot allowance in `CG_ConsoleCommand` is retained: a local server can temporarily have no future snapshot. Pointer-size table strides and the optional rumble initialization are retained.

The score request timeout/reset, popup flag, voice-chat eligibility, shellshock arguments and float conversion order agree with Mac. Scripted table comparison checks every command name/handler, and the call/string triage and constant checks were inspected.

All four assigned client translation units compile with actual arm64 Release commands in stock and CoD2x configurations. The dedicated Release target builds. Ten legacy configurations report zero source mismatches; `git diff --check` passes. No new tests, replays or full CI were run. The corrections are compiled and reviewed, not exercised at runtime.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-interface.noindex/`.
