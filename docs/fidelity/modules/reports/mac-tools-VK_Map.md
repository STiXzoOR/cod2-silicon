# Mac/Tools/VK_Map module reconciliation

Updated 2026-11-05.

**1/1 reconciled: 1 fixed.** Mac 1.3 i386 is the authority. The native branch now reconstructs `TranslateKeyCodeToVK` at 0xccd4: cache French for language 1, German for language 2, otherwise ANSI, preserving the original second language query and unchecked indexing contract. The former function returned zero for every key.

The complete 128-byte tables at Mac 0x341240 (German), 0x3412c0 (French), and 0x341340 (ANSI) were compared with the preprocessed native initializers. Twelve incorrect bytes were restored: keypad Enter/Subtract and the final arrow entries in both ISO maps, plus key 106 in French and ANSI. All 384 bytes now match. Changes use `COD2_X64`; all five inactive guard configurations preserve the baseline source bodies, and neither changed file contains `__LINE__`.

This repairs a dormant Carbon callback contract. `mac_input.c` still contains calls from `CCallOfDutyEngine_DoKeyDown` and `DoKeyUp`, but the selected native event pump in `src/unix/linux_input.c` maps SDL key symbols directly to `Sys_QueEvent`. `MacResources_GetLanguageCode` remains a zero-returning dormant provider, audited by the MacResources unit. The correction does not establish working French/German resource detection or live keyboard-layout parity.

The arm64 Release `cod2_macos_ded` target compiled at background priority with two jobs. No runtime, replay, full client build, or new test was run. The translator and tables are compiled and reviewed, not exercised. No entries remain unchecked.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-display.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
