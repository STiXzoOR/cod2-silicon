# qcommon/com_playerprofile module reconciliation

Updated 2026-11-04.

**7/7 assigned ledger entries reconciled: 4 equal, 1 fixed, 2 intentional.** Each function was read against Mac 1.3, including the inlined existence, path-building and set/execute helpers. Exact strings were decoded from the binary. The exact-symbol disassembly was used for `Com_HasPlayerProfile` because the full-binary output misdecoded its entry.

## Correction

**Missing active profiles loaded an invented default profile.** Mac `Com_InitPlayerProfiles` at 0x111b26 goes to `Com_ExecStartupConfigs(NULL)` at 0x111b4b after a failed `players/active.txt` read. Native instead created `default`, wrote the active file, selected that profile and forced the chosen flag. The native branch now follows the reference fallback. Windows 0x42d1da–0x42d1e9 returns false for the same failed read, and 0x42d2f6–0x42d34f executes the unprofiled startup sequence; it also does not create a default profile.

The path builders retain Mac's signed prefix/suffix length checks and truncation result. Profile changes retain the active-file write, disconnect, external dvar reset, selected config, recommendations and conditional sound restart in the same order.

## Intentional differences

New/delete profile directories remain under writable `fs_homepath` and the current `fs_gamedir`. Mac 0x111c52 and 0x1118a7 load `fs_basepath` through import slot 0x1accd31. The WS47 and WS102 reports document this existing adaptation to the port's read-only install and writable-home contract. All other paths in those functions were compared.

## Verification and limits

The isolated arm64 Release `cod2_macos_ded` build passed with two jobs at low priority. Both changed translation units compiled. The inactive-source guard against 27d1e9b reports zero mismatches in ten legacy configurations; this is a token check, not a byte-identical i386 build. The existing stock profile fixture passed with zero failures; it covers the reviewed profile lifecycle and paths, not the corrected missing-file fallback. No new tests, full CI or replays were run. The corrected missing-file path is compiled and reviewed, not exercised. No assigned entry remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-qcommon-profile.noindex/`.
