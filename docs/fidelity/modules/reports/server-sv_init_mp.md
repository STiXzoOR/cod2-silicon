# server/sv_init_mp module reconciliation

Updated 2026-11-04.

**25/26 reconciled: 15 equal, 8 fixed, 2 intentional; 1 unchecked.** Mac 1.3 is the authority. Native changes are guarded by `COD2_X64`; the legacy branches are unchanged.

## Corrections

`SV_SetConfigstring` accepted negative indices, unlike Mac's unsigned check at 0x1424e2. Its chunk capacity was also one byte too large: Mac 0x14259f subtracts the decimal index length including its terminator from 0x3fd. Both now match. The missing 0x15 error prefix was restored here and in `SV_SetUserinfo`, `SV_SetConfigValueForKey`, startup and client allocation errors.

`SV_Startup` now initializes the master communication timestamp from server time, as Mac 0x142b5f does. `SV_Shutdown` now clears `bgs` after loading-state abort cleanup. Mac 0x142a7b uses the nonlazy pointer at 0x1acce1d, whose stored address resolves to `_bgs`; native incorrectly cleared `com_errorEntered`.

`SV_Init` now registers the three WWW dvars after `nextmap` and `com_expectedHunkUsage`, matching Mac 0x14240f-0x1424d6. A script checked all 46 retail registrations for type, name, default, domain, flags and order. `SV_SpawnServer` now calls `UI_LoadIngameMenus` after client loading begins, as Mac 0x143065 does; dedicated builds still exclude client work.

## Comparison and boundaries

The clear-server, baseline, final-message, client-drop, archive-free, max-client clamp and system-info helpers were compared inside their Mac startup/shutdown/spawn callers. Archive buffer sizes, configstring/userinfo access, hunk usage parsing and snapshot reset fields were read directly. Exact-symbol disassembly was used to check entries, including `SV_Loaded`, whose full-binary view misdecoded the first instruction.

The optional CoD2x lifecycle/protocol hooks, native material/debug work and dedicated client exclusions remain. `set_sv_punkbuster` remains absent at the explicitly unsupported PunkBuster boundary documented by `pb_null_adapter.c` and the WS44 audit.

`SV_GetConfigValueForKey` remains unchecked. It has no native body or Mac symbol, and the Linux-only helper has no callers. No Mac implementation or inlined caller was established; absence is not recorded as equality.

## Verification and limits

The arm64 Release dedicated target compiled at low priority with two jobs. The client translation unit also compiled, covering the restored menu call. All three assigned translation units' legacy preprocessed output matches the base after normalizing line-derived assertion typedef names. No tests, full CI or replays were added or run. The corrections are compiled and reviewed, not exercised. The restored master timestamp is consumed by the server-client worker's restored authorization path.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-server-main.noindex/`.
