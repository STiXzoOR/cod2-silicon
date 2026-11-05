# Mac/Tools/VK_Map module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 fixed.** Mac 1.3 i386 is the authority. The native branch now reconstructs `TranslateKeyCodeToVK` at 0xccd4: cache French for language 1, German for language 2, otherwise ANSI, preserving the original second language query and unchecked indexing contract. The former function returned zero for every key.

The complete 128-byte tables at Mac 0x341240 (German), 0x3412c0 (French), and 0x341340 (ANSI) were compared with the preprocessed native initializers. Twelve incorrect bytes were restored: keypad Enter/Subtract and the final arrow entries in both ISO maps, plus key 106 in French and ANSI. All 384 bytes now match. Changes use `COD2_X64`; all five inactive guard configurations preserve the baseline source bodies, and neither changed file contains `__LINE__`.

This repairs a dormant Carbon callback contract. `mac_input.c` still contains calls from `CCallOfDutyEngine_DoKeyDown` and `DoKeyUp`, but the selected native event pump in `src/unix/linux_input.c` maps SDL key symbols directly to `Sys_QueEvent`. `MacResources_GetLanguageCode` remains a zero-returning dormant provider, audited by the MacResources unit. The correction does not establish working French/German resource detection or live keyboard-layout parity.

The arm64 Release `cod2_macos_ded` target compiled at background priority with two jobs. No runtime, replay, full client build, or new test was run. The translator and tables are compiled and reviewed, not exercised. No entries remain unchecked.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-display.noindex/`.
