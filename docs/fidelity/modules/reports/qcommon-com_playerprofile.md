# qcommon/com_playerprofile module reconciliation

Updated 2026-11-04.

**7/7 assigned ledger entries reconciled: 4 equal, 1 fixed, 2 intentional.** Each function was read against Mac 1.3, including the inlined existence, path-building and set/execute helpers. Exact strings were decoded from the binary. The exact-symbol disassembly was used for `Com_HasPlayerProfile` because the full-binary output misdecoded its entry.

## Correction

**Missing active profiles loaded an invented default profile.** Mac `Com_InitPlayerProfiles` at 0x111b26 goes to `Com_ExecStartupConfigs(NULL)` at 0x111b4b after a failed `players/active.txt` read. Native instead created `default`, wrote the active file, selected that profile and forced the chosen flag. The native branch now follows the reference fallback. Windows 0x42d1da–0x42d1e9 returns false for the same failed read, and 0x42d2f6–0x42d34f executes the unprofiled startup sequence; it also does not create a default profile.

The path builders retain Mac's signed prefix/suffix length checks and truncation result. Profile changes retain the active-file write, disconnect, external dvar reset, selected config, recommendations and conditional sound restart in the same order.

## Intentional differences

New/delete profile directories remain under writable `fs_homepath` and the current `fs_gamedir`. Mac 0x111c52 and 0x1118a7 load `fs_basepath` through import slot 0x1accd31. The WS47 and WS102 reports document this existing adaptation to the port's read-only install and writable-home contract. All other paths in those functions were compared.

## Verification and limits

The isolated arm64 Release `cod2_macos_ded` build passed with two jobs at low priority. Both changed translation units compiled. The inactive-source guard against 15c8f12 reports zero mismatches in ten legacy configurations; this is a token check, not a byte-identical i386 build. The existing stock profile fixture passed with zero failures; it covers the reviewed profile lifecycle and paths, not the corrected missing-file fallback. No new tests, full CI or replays were run. The corrected missing-file path is compiled and reviewed, not exercised. No assigned entry remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-profile.noindex/`.

## Integrated batch verification

Integrated engine source through `aa2f246` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients and function/callback ABI audits. The first source-fixture run failed linking the existing network-profile fixture because it lacked the restored dvar lookup dependency. Fixture commit `42d8893` supplies the dependency and replaces its outdated aggregate-column expectation with the binary's i386 variadic word behavior; its focused ASan run and the complete source-fixture rerun passed. The original failure log is retained. Engine preprocessing differences are confined to `linux-x86_64-port-on`; existing fixture adaptations also differ under their test configurations. Independent reviews covered the six units' changed source and nearby callers against binary evidence, with no retained findings. These are changed-code reviews, not second whole-module audits.

The first stock recording attempt stopped before map load because its isolated home had no selected player profile. The restored engine behavior requires one. Harness commit `4e8b534` seeds the default profile; 38 existing harness checks and its independent review passed. Failed recording evidence is preserved. After this prerequisite correction, both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baseline in players, state hashes and entities, and match each other. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. Unchanged traces do not prove that each correction was reached; unreached branches remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable: its unchanged `410342a` reference fails compiling GCC `__stdcall`/`__cdecl` declarations. The workflow already documents that limitation and marks the job nonblocking. Preprocessing equality is not binary-byte parity.
