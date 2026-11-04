# server/sv_main_pc_mp module reconciliation

Updated 2026-11-04.

**7/7 reconciled: 4 equal, 2 fixed, 1 intentional.** Mac 1.3 is the authority. The legacy branches are unchanged.

## Corrections

Remote-command redirects used a 0x3ff0 stack buffer. Mac 0x193dc6/0x193eec uses a 0x1fff0 `LargeLocal` buffer. The native branch now matches that capacity and frees the allocation on every normal return. The 499ms throttle, password branches, argument quoting/join limit, execute and redirect cleanup were read against Mac.

The retail heartbeat fallback tested the deadline backwards. Mac 0x194218 sends when server time reaches or passes the next heartbeat time. The native fallback now uses that comparison and the live `com_dedicated` variable. Its retail heartbeat/game-complete diagnostics were restored. The 180000ms heartbeat and 600000ms status deadlines and address-failure paths were compared by hand.

## Comparison and boundaries

Redirect chunk splitting, cached retail master resolution, network-order port 20710, shutdown's INT_MIN deadline/flatline call and the empty match-end function match Mac. Exact-symbol disassembly confirmed the function entries.

The native macOS dedicated build intentionally routes heartbeat and game-complete status through `MacServer_MasterHeartbeat` and `MacServer_MasterGameCompleteStatus`, which support five configurable master slots. That existing backend remains. Optional CoD2x RCON limiting remains, and the Mac `pb_sv_` force-processing hook stays omitted at the unsupported PunkBuster null-backend boundary documented by `pb_null_adapter.c` and the WS44 audit.

## Verification and limits

The arm64 Release dedicated target compiled at low priority with two jobs. The client translation unit also compiled, covering the retail heartbeat fallback that the native macOS dedicated target replaces. Legacy preprocessed output matches the base. No new tests, full CI or replays were run. The changes are compiled and reviewed, not exercised.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-server-main.noindex/`.
