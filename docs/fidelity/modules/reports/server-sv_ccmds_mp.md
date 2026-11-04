# server/sv_ccmds_mp module reconciliation

Updated 2026-11-04.

**35/35 reconciled: 27 equal, 5 fixed, 3 intentional.** Compared Mac 1.3 i386 command bodies and inlined helpers with native code, including command registrations, kick-all iteration, restart state changes and status formatting. Triage and constant scans supported the hand comparisons. The `COD2_X64`-undefined source projection is byte-identical to the base.

## Corrections

Mac `SV_RemoveOperatorCommands` at 0x172032 is empty. Native now retains the command table and initialization guard, matching that lifetime.

Map rotation lost the parser's end-of-input signal. Mac 0x1725e6, 0x172637 and 0x1727e1 test the parse pointer; native now returns NULL when it is cleared, refills the full rotation only on the initial exhausted read, and distinguishes missing gametype/map values from exhausted rotation. An empty quoted token remains an unknown keyword. The Linux-only `UI_GetMapRotationToken` row maps to these inlined blocks.

`SV_Map_f` omitted `Com_DedicatedModified` at Mac 0x172076. The matching native implementation already existed inline in `Com_Frame_Try_Block_Function`. It is now a shared native helper in `qcommon/common.c`, called by both frame and map paths. Compared its dedicated dvar flags, latched/current check, registrations, modified flag, client shutdown, console/exit and dedicated-command calls with Mac 0x2ee14–0x2eee8. Existing dedicated-platform client-call guards remain; the legacy frame text is unchanged.

The dumpuser usage diagnostic now reads `Usage: info <userid>`, as Mac 0x171f8a does.

## Retained boundaries

`PbMsgToScreen` is a bundled PB callback with no native caller after the WS44 null-adapter replacement. It was compared at Mac 0x15f586; ordinary engine console commands were audited separately. The Linux-only dedicated-command removal helper has no Mac counterpart or caller in the compared mode-switch path; native follows the empty Mac removal entry. Existing CoD2x stop-gametype hooks around map and restart operations remain.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiles at low priority with two jobs. No new tests, full CI or replays were run. The existing stop-gametype fixture gained a stub for the dedicated-mode switch, which is outside that fixture's scope; its stock client and dedicated targets pass under ASan/UBSan. The existing timing fixture now extracts the real dedicated-mode helper when the frame calls it; its current and pre-extraction baseline runs pass with CoD2x compiler flags under ASan. It exercises populated rotation and map paths, but not every parser-exhaustion case or the real dedicated-mode switch. Those corrections, command lifetime and the usage diagnostic are compiled and reviewed, not exercised directly.

Private per-function evidence is in `server-client.ledger.json` in the orchestrator's unit directory. Private disassembly, constants and logs: `~/Library/Application Support/CoD2-native-decomp/recon-server-client.noindex/`.
