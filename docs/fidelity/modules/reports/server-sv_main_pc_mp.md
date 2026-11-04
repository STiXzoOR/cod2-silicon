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

## Integrated batch verification

Integrated engine source through `868a08f` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `b1f110d` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `b275adb` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
