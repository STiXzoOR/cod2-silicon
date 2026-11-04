# stringed/stringed_hooks reconciliation

Updated 2026-11-05.

**32 entries: 16 equal, 6 fixed, 2 intentional, 8 unchecked.** Exact Mac 1.3 i386 entries and their inline blocks were compared by hand. Linux supplied names only. The fifteen language names, sentinel, bounds and registration constants match the original data. The legacy OFF source is preserved.

The language-8 reader at 0x31a64/0x31ba9 accepts B0–C8 lead bytes; native also accepted E0–EF. The language-11 reader at 0x31b54 accepts 81–FE leads and 41–FE trails; native required A1–F7 and A1–FE. Its punctuation at 0x31b83 starts at 8141, not A1A1. These three reader corrections are native-only. Historical native helper names describe different locales from their actual dispatch; the ledger records the language index and Mac block.

The message-token branch at 0x32247–0x3224e bypasses translation for one-character tokens. Native treated them as missing translations. The standalone token-helper row and enclosing message function record this same correction.

Existing startup null guards in language loading/refresh remain intentional. The documented native rune table supplies both the Mac 0x400 digit classification and its native alias 4, so the existing digit-mask adapter remains. Both safe-translation concatenations retain the verified native destination/size/source ABI.

No matching complete Mac/native identities were established for Chinese, Japanese, Korean and traditional-Chinese character-to-index APIs; the three standalone Chinese/Japanese/Korean character predicates; or LanguageUsesSpaces. These eight rows remain unchecked. Neighboring reader predicates do not establish them.

The Release arm64 dedicated target links, and this worktree's actual client translation unit compiles. Existing ASan/UBSan language and localized-placeholder fixtures pass; they do not exercise the corrected byte ranges or one-character branch. No new cases were added. The guard check passes every OFF configuration; its sole remaining difference is the intended native Linux configuration. There are no __LINE__ sites in this module. An intermediate fixture run failed because its raw-source extractor counted mutually exclusive opening braces; the guard was reshaped and the unchanged fixture passed. Initial logs, that failure and final evidence remain private under recon-stringed.noindex. Oversize and missing-placeholder paths were compared, not exercised; no full game run or replay was performed.
