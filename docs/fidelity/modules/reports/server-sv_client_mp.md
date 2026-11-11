# server/sv_client_mp module reconciliation

Updated 2026-11-04.

**43/43 reconciled: 25 equal, 8 fixed, 10 intentional.** Compared the assigned Mac 1.3 i386 bodies and their inlined helpers with native code. Triage and constant scans were followed by hand comparison; the 13-entry client-command table at 0x34b880 was decoded. The `COD2_X64`-undefined source projection is byte-identical to the base.

## Corrections

Authorization had no master-liveness updater. `SV_UpdateLastTimeMasterServerCommunicated` now compares the source with the master base address and records server time, as Mac 0x17c464 does. `SV_GetChallenge` now uses the 1,200,000 ms master-liveness grace and 7,000 ms challenge grace at 0x17dc5f, and the authorization port is 20700 at 0x17dc0e. Windows agrees at 0x452fbd, 0x45301e and 0x45302d. The server_main worker supplies startup initialization and the four connectionless dispatch call sites.

Direct connect used `pingTime` as a cached ping. Mac 0x17ebe2 caches `svs.time - pingTime` in `firstPing`. The successful connection path now copies Mac's client-slot-indexed `firstPing` reset at 0x17f417, including that unusual index, and initializes `gamestateMessageNum` at 0x17f481 instead of `deltaMessage`. The zero-GUID diagnostic at 0x17f357 was also missing.

Dropping a client now saves the name before `SV_FreeClient` clears userinfo, as Mac 0x17e032 does. Mac 0x17e085 checks whether the reason is a localization reference; its result controls both the broadcast translation marker and the reliable translated/literal disconnect formats. The literal engine-facing format includes the retail `PB` suffix even though the bundled PB backend stays excluded.

`SV_UserMove` now selects its delta acknowledgement from the delta flag alone, as Mac 0x17fede does. `SV_ExecuteClientMessage` now checks purity before EOF, updates flood deadlines for ordinary commands while exempting `team`, `score` and `mr`, and resends a lost gamestate without the extra native state/fragment gates or another authorization request. These paths are at 0x1803bd, 0x18043f and 0x180622. The Linux-only `SV_ClientCommand` ledger row is the inlined reliable-command block in this function.

Download errors now check disabled downloads before IW archives, use `EXE_CANTAUTODLGAMEPAK`, and print the retail diagnostics at 0x17f903–0x17fa1f. Windows contains the same archive/error strings. A failed `FS_Read` now appends the EOF block in the same call when the window has room, as Mac 0x17fe95 and 0x17fd7a do.

## Retained boundaries

The bundled PB callback remains the WS44 null adapter. PB GUID storage/authentication is excluded; native engine admission, bans, disconnects and downloads were still compared. Existing CoD2x digest/HWID admission, identity sidecars, version notifications and 40 Hz snapshot support remain. Existing download-path hardening and the differential recording gate remain.

The valid `SV_VerifyIwds_f` paths match. The existing native negative-count rejection traces to the initial commit, 63de35a; no earlier explicit hardening report was found. With a malformed bare `@`, Mac 0x17d7fd reads `clientChkSum[-1]`, an uninitialized adjacent stack slot. Native retains the defined `pureAuthentic = 2` rejection instead of depending on that stack value. This specific exception is recorded as intentional.

## Verification and limits

The arm64 Release `cod2_macos_ded` target compiles at low priority with two jobs. No new tests, full CI or replays were run. The existing NAT identity fixture was corrected to expire both authorization grace periods rather than claiming a timeout after 6,000 ms. Its existing ASan/UBSan target passes for the stock client and dedicated flag sets, including game and voice routing. These fixtures exercise connection and timeout admission, but do not isolate every corrected branch. Disconnect formatting, movement delta selection, packet flood/EOF handling and download failures are compiled and reviewed, not exercised directly.

Private per-function evidence is in `server-client.ledger.json` in the orchestrator's unit directory. Private disassembly, constants and logs: `~/Library/Application Support/CoD2-native-decomp/recon-server-client.noindex/`.

## Integrated batch verification

Integrated engine source through `aa2f246` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `42d8893` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `4e8b534` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
