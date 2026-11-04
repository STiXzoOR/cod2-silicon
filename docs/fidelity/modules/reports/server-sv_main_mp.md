# server/sv_main_mp module reconciliation

Updated 2026-11-04.

**24/24 reconciled: 8 equal, 10 fixed, 6 intentional.** Mac 1.3 is the authority. The legacy branches are unchanged.

## Corrections

The reliable-command queue differed in several connected rules. Mac 0x159a61 compacts ignorable commands for inactive clients, and 0x159aeb drops a new ignorable command after compaction. Identical command suffixes replace before opcode-specific rules. The decoded table at 0x33b080 replaces `C/D/a/b/o/p/q/r/t` directly and compares the first token for `d/v`; native used different classes. Mac 0x159b89 increments the sequence before the overflow check. The native branch now follows those rules and preserves replacement sequence/count behavior.

`SVC_Status` discarded the score returned at Mac 0x15a178 and inverted the pure/base-game mod check at 0x15a1d2. Both are corrected. Status and game-complete player lists now use Mac's 0x1ffff bound at 0x15a0df/0x15a530. Server-command formatting now uses the full 1.3 message capacity, as Mac 0x15bde4 does, instead of stopping at 0x4000.

`SV_Frame` now supplies `EXE_SERVERKILLED` for an empty kill reason, matching Mac 0x15bf55/0x15c11a. Missing ping diagnostics were restored from 0x15c578, 0x15c49e and 0x15c2dc. Packet acknowledge diagnostics now match 0x15bae6 and 0x15bb15, including Mac's use of the existing reliable-acknowledge field in the negative-message-acknowledge message.

Connectionless status, info, challenge and authorization requests now update master liveness before dispatch, matching Mac 0x15b00e/0x15b267/0x15b197/0x15b42d. These calls depend on `SV_UpdateLastTimeMasterServerCommunicated`, reconstructed in `sv_client_mp` by the server-client worker from Mac 0x17c464. Its ledger identity belongs to that worker.

## Comparison and boundaries

The pause, timeout, ping, bot-update, reliable-copy, reliable-selection, token-match and newline helpers were read inside their Mac callers. Frame residuals, all wrap limits, dvar update masks, archive order, voice dispatch, bot random-call order and packet decode gates were compared by hand. The nonlazy pointer used after packet execution resolves to `_bgs`, matching the existing native clear.

Native replay/input/hash hooks, optional CoD2x query policy and protocol changes, and dedicated client exclusions remain intentional. PunkBuster query/send callbacks and its packet/event backend remain unsupported, as documented by `pb_null_adapter.c` and the WS44 audit. The missing PB query advertisement is retained at that boundary. The visible `hw` values were also checked against Windows: 0x45b215/0x45b229 uses dedicated=2 and listen=5, matching Mac.

## Verification and limits

The arm64 Release dedicated target compiled and linked at low priority with two jobs, including the server-client worker's helper commit 6576638 (locally cherry-picked as 4c099a2). The client translation unit also compiled. Legacy preprocessed output matches the base after normalizing line-derived assertion typedef names. No new tests, full CI or replays were run. The corrections are compiled and reviewed, not exercised.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-server-main.noindex/`.
