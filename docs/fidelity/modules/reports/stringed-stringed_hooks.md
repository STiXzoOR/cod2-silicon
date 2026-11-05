# stringed/stringed_hooks reconciliation

Updated 2026-11-05.

**32 entries: 16 equal, 6 fixed, 2 intentional, 8 unchecked.** Exact Mac 1.3 i386 entries and their inline blocks were compared by hand. Linux supplied names only. The fifteen language names, sentinel, bounds and registration constants match the original data. The legacy OFF source is preserved.

The language-8 reader at 0x31a64/0x31ba9 accepts B0–C8 lead bytes; native also accepted E0–EF. The language-11 reader at 0x31b54 accepts 81–FE leads and 41–FE trails; native required A1–F7 and A1–FE. Its punctuation at 0x31b83 starts at 8141, not A1A1. These three reader corrections are native-only. Historical native helper names describe different locales from their actual dispatch; the ledger records the language index and Mac block.

The message-token branch at 0x32247–0x3224e bypasses translation for one-character tokens. Native treated them as missing translations. The standalone token-helper row and enclosing message function record this same correction.

Existing startup null guards in language loading/refresh remain intentional. The documented native rune table supplies both the Mac 0x400 digit classification and its native alias 4, so the existing digit-mask adapter remains. Both safe-translation concatenations retain the verified native destination/size/source ABI.

No matching complete Mac/native identities were established for Chinese, Japanese, Korean and traditional-Chinese character-to-index APIs; the three standalone Chinese/Japanese/Korean character predicates; or LanguageUsesSpaces. These eight rows remain unchecked. Neighboring reader predicates do not establish them.

The Release arm64 dedicated target links, and this worktree's actual client translation unit compiles. Existing ASan/UBSan language and localized-placeholder fixtures pass; they do not exercise the corrected byte ranges or one-character branch. No new cases were added. The guard check passes every OFF configuration; its sole remaining difference is the intended native Linux configuration. There are no __LINE__ sites in this module. An intermediate fixture run failed because its raw-source extractor counted mutually exclusive opening braces; the guard was reshaped and the unchanged fixture passed. Initial logs, that failure and final evidence remain private under recon-stringed.noindex. Oversize and missing-placeholder paths were compared, not exercised; no full game run or replay was performed.

## Integrated batch verification

Integrated production source 02a2156 plus existing-fixture correction ac63727 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.
