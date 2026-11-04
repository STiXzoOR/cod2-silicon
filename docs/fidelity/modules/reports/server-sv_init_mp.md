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

## Integrated batch verification

Integrated engine source through `868a08f` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `b1f110d` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `b275adb` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
