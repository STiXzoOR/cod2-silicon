# ui/ui_gameinfo_mp module reconciliation

Updated 2026-11-05.

**2/2 reconciled: 1 equal, 1 fixed.** Compared both complete Mac bodies by hand and reviewed the call/string and constant checks. `UI_ParseInfos` at 0x180a0c matches the brace/error handling, 1024-byte key/value buffers, missing-value substitution, allocation sizing, null-allocation retry, and info copying.

`UI_LoadArenas` at 0x180bee read the wrong key. Mac 0x180dd8 passes the string `gametype` at 0x2d5f58; native passed `type`. The native path now reads `gametype`. Mac 0x180ea9 also continues the game-type scan after setting a matching bit; native stopped at its first match. The scan now preserves every matching entry. An unsigned shift represents the Mac 32-bit mask, including bit 31 of the 32-entry game-type table. These changes are confined to `COD2_X64`; the legacy text is preserved.

The remaining file-list iteration, 8191-byte size limit, arena cap, map names, loadscreen registration, absent-game-type mask, and 128-map limit match. No visible platform-specific behavior required Windows comparison.

The arm64 Release dedicated target built successfully. The actual production client `ui_gameinfo_mp.c` translation unit also compiled under the configured flags; it reported three existing size-to-int conversion warnings. The corrections are compiled and reviewed, but were not exercised in the UI. No tests or replays were added or run. All five inactive configurations from the existing guard checker preserve the baseline bodies; neither changed file contains `__LINE__`.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-ui-support.noindex/`.
