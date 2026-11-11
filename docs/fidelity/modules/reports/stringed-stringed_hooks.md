# stringed/stringed_hooks reconciliation

Updated 2026-11-05.

**32 entries: 17 equal, 8 fixed, 2 intentional, 5 unchecked.** Exact Mac 1.3 i386 entries and their inline blocks were compared by hand. Linux supplied names only. The fifteen language names, sentinel, bounds and registration constants match the original data. The legacy OFF source is preserved.

The language-8 reader at 0x31a64/0x31ba9 accepts B0–C8 lead bytes; native also accepted E0–EF. The language-11 reader at 0x31b54 accepts 81–FE leads and 41–FE trails; native required A1–F7 and A1–FE. Its punctuation at 0x31b83 starts at 8141, not A1A1. These three reader corrections are native-only. Historical native helper names describe different locales from their actual dispatch; the ledger records the language index and Mac block.

The message-token branch at 0x32247–0x3224e bypasses translation for one-character tokens. Native treated them as missing translations. The standalone token-helper row and enclosing message function record this same correction.

Existing startup null guards in language loading/refresh remain intentional. The documented native rune table supplies both the Mac 0x400 digit classification and its native alias 4, so the existing digit-mask adapter remains. Both safe-translation concatenations retain the verified native destination/size/source ABI.

No matching complete Mac/native identities were established for Chinese, Japanese, Korean and traditional-Chinese character-to-index APIs or LanguageUsesSpaces. These five rows remain unchecked. The three packed-character predicates are mapped below using newly recovered original input projections.

The Release arm64 dedicated target links, and this worktree's actual client translation unit compiles. Existing ASan/UBSan language and localized-placeholder fixtures pass; they do not exercise the corrected byte ranges or one-character branch. No new cases were added. The guard check passes every OFF configuration; its sole remaining difference is the intended native Linux configuration. There are no __LINE__ sites in this module. An intermediate fixture run failed because its raw-source extractor counted mutually exclusive opening braces; the guard was reshaped and the unchanged fixture passed. Initial logs, that failure and final evidence remain private under recon-stringed.noindex. Oversize and missing-placeholder paths were compared, not exercised; no full game run or replay was performed.

## Integrated batch verification

Integrated production source d26c2d3 plus existing-fixture correction 9391076 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. All five guard-visible production changes and the existing numeric-field fixture differ only under native port-on; the Apple-only StringEd source has no inactive differences. Actual assertion line expansions in both UI source files match the previous source in all five inactive configurations. Step 13 first failed because the existing UI text-color extraction counted both conditional opening braces and omitted the function; the fixture now preprocesses it using engine flags, as two other existing suites already do. All unchanged online fixtures passed in stock and CoD2x, followed by the complete source-stage retry. Original failure evidence is retained. No new tests were added. These checks do not establish i386 byte parity.

Parent and fresh independent source/binary reviews covered all six units, including the corrected early-download text path. Fourteen entries remain unchecked: thirteen StringEd identities and one UI missing-player string boundary. Localization parser/tree exception cleanup and unreached client UI/audio paths remain compiled and reviewed; the existing fixtures do not establish complete retail asset behavior. Group voice preserves the documented playback-only native capture boundary.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. The first stock attempt stopped before engine launch because previous output existed; verified backed-up output was cleared and the complete replay passed. Dedicated replay does not exercise UI interaction, client sound output or microphone capture.

Catalog coverage passed without semantic catalog changes. The predecessor batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run is incomplete. Hosted ILP32 comparison remains unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Original packed-character input projections

Original Linux 1.3 wrappers at 0x813926c–0x813928f (Korean), 0x8139290–0x81392b3 (Japanese), and 0x81392b4–0x81392d7 (Chinese) extract the low byte and bits8–15 from their unsigned input, call the corresponding byte-pair predicate, and zero-extend its boolean byte. They have no other effects. Upper16 input bits are explicitly discarded. The original reader at 0x8138e64 calls the Chinese packed wrapper at 0x813900c and the Korean/Japanese byte providers at 0x8138ed0/0x8138f9e. Original 1.0 wrappers0x8136e40/64/88 independently anchor the same identities.

This supplies the missing full input/result contract from the earlier bounded-reader audit. Complete Mac language8/10/11 predicates correspond to the original providers: Korean at0x31a64–0x31a78 and0x31ba9–0x31bc0; Japanese at0x31afb–0x31b18 and0x31b99–0x31ba4; Chinese at0x31b54–0x31b70 with false path0x31bc5. Native's historically named Japanese/Chinese/Thai DoubleByte helpers implement those complete respective predicates. The original projection reduces every uint32 input to the same byte pair, so no wider-input classification is inferred from bounded reader values.

SEH_IsJapaneseChar is equal; Korean and Chinese packed predicates inherit their existing byte-range repairs and are fixed. These are complete consumed-operation mappings, not claims of standalone exported Mac wrappers or newly added native APIs. The four index converters and LanguageUsesSpaces remain unchecked. Fresh independent original/Mac/native review supports these three mappings. No source change, compilation, tests, CI, replay or runtime exercise occurred. Existing native policies and runtime limits remain.


## Original index and spacing contracts

The remaining four index operations have exact original1.3 bodies: Korean0x8138b7e, Traditional Chinese0x8138c5a, Japanese0x8138d40 and Chinese0x8138e18. Each invokes its actual original classification provider before the complete offset/gap/stride arithmetic and zero rejection. These bodies establish integer-index intent, including arithmetic on the full input word. Complete fresh Mac R_GetCharacterGlyph0xec2d8 instead directly indexes ASCII or searches stored letter codes and returns a glyph pointer/fallback. The selected native provider has the same distinct ownership and return contract; the locale byte predicates do not supply index conversion.

The original spacing predicate0x81390ea calls the language provider and returns false for9..11, true otherwise. Exact Mac Language_IsAsian0x31682 simply reads g_currentAsian. That cached classification is not proof of the complete spacing predicate. All five rows remain unchecked; original caller/address searches are retained only as bounded search evidence. No source, verdict, compilation, tests, CI, replay or runtime exercise. Private bodies: recon-locale9.noindex.


## Locale22 spacing producer and word-loop state boundary

SEH_LanguageUsesSpaces remains unchecked after following the actual producer
and consumer. The original 1.3 helper at 0x81390ea calls current-language
provider 0x8138154, which reads loc_language.current.integer at that call, then
returns zero for unsigned(language-9)<=2 and one otherwise. The prior original
body remains retained in recon-locale9.noindex.

Fresh exact Mac SEH_InitLanguage writes g_currentAsian at 0x317b1 after the
unsigned(language-8)<=4 comparison at 0x317a5–0x317ae. Complete UpdateLanguageInfo
repeats that sample/store initially at 0x318d7–0x318eb, after each fallback
Dvar_SetInt at 0x31962–0x31976, and after final reset at 0x319a3–0x319b7. Native
stringed_hooks.c preserves these actual sampling points. Language_IsAsian
0x31682 reads the stored Boolean; it does not read the current language anew.

Both original and Mac binary name tables were extracted directly: original
0x8188ae0 and Mac 0x341d40 contain the same fifteen names in order, including
8=korean, 9=taiwanese, 10=japanese, 11=chinese and 12=thai. Thus there is no enum
renumbering that makes cached !Asian identical to the original spacing set.
This difference alone is not grounds to reject an actual Mac operation: Mac
behavior remains authoritative, and no source change toward Linux is proposed.

There is a real negated word-space consumer, not merely a proposed negation of
a getter. Full exact CL_ConsolePrint_AddLine 0x15ed28 first queries the cached
flag at 0x15ed8c. Its zero branch goes to generic byte-word processing at
0x15edb3. Only the nonzero branch and message types 1/2/3 enter the pixel path
through 0x15f336, 0x15f1f5 or 0x15efe3. Within that path, word lookahead reads a
character, stops at <=space, checks character/pixel limits, then calls
Language_IsAsian at 0x15f0e7. TEST/JE at 0x15f0ec/0x15f0ee repeats lookahead at
0x15f08b on cached false. Native cl_console_mp.c has the same initial domain and
do/while(!Language_IsAsian()) loop.

The complete intraprocedural direct CFG was checked, including shared tails:
generic entry 0x15edb3 cannot reach the inner call; the pixel path eventually
shares completion at 0x15eeae, not a generic-to-pixel entrance. Calls are modeled
as returning to their following instructions for this control-flow finding,
without assuming they cannot change state. Font/width and character-reader
calls occur before the inner cached read. Therefore no assertion is made that
the cached flag must remain true after the initial pixel gate, or that its
zero branch is globally impossible.

This establishes a complete cached classification and a message-domain word
continuation use, but does not identify the fresh-current spacing-query
operation or a complete input/state adapter to it. Equating them would assume
a relationship between current language and the last cached sample, or broaden
the gated inner continuation into a standalone all-language query. Neither
relationship is established by these owners. The original arbitrary-current
API, stale-cache/update timing and message-domain effects remain outside the
proved mapping; no hypothetical fresh accessor or normalization is added.

One unchecked evidence refinement, zero closures/defects/source changes. The
four locale character-index rows and all previous decisions remain untouched.
Exact producers/getter/console owner, both binary tables, original current
accessor and direct-CFG receipt remain in recon-provenance-locale22.noindex.
Static inspection only: no compilation, tests, CI, replays or runtime exercise.
