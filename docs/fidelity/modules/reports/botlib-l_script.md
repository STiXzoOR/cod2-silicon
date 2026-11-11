# botlib/l_script module reconciliation

Updated 2026-11-05.

**18/37 reconciled: 9 equal, 8 fixed, 1 intentional, 19 unchecked.** Mac 1.3 is the authority; Linux was read only for helper names and intent. Native corrections are under COD2_X64 guards. The selected legacy source tokens are unchanged in all five OFF configurations, and neither assigned source uses __LINE__.

## Corrections

Ordinary decimal integers were marked as floats. Mac 0xc2b11 sets subtype 0x8, whereas native set 0x800. The scanner now also consumes at most two L/U suffix characters, matching the mutually exclusive branches at 0xc2a98-0xc2aba, and preserves incoming subtype bits as Mac does. Its normal caller clears the complete token first. Hex scanning deliberately retains Mac's uppercase-A-only token quirk; the escape reader deliberately accepts all A-Z/a-z letters as hexadecimal digits.

Number conversion now wraps integer accumulation and the fractional divisor at 32 bits. Mac 0xc295a returns on a second decimal point with intvalue still zero from the function's own initialization. Native had assigned the accumulated float. Mac also processes the character immediately after the first point without checking it first, and converts the final float through signed 64-bit storage before keeping the low 32 bits. Those deterministic operations are now preserved. Native long double continues to use host precision; no extended-precision emulator was added.

A trailing point requires a storage qualification. Mac processes its NUL as digit -48 and then inspects the following byte. The production PS_ReadToken caller zeroes the whole token at 0xc310d, so that byte is defined for the accepted token lengths. Native processes the terminating byte but stops there for direct callers that provide only a terminated string, avoiding a read past their storage. Other bytes beyond a direct caller's terminator are not claimed equivalent.

A bare hexadecimal escape previously left the cursor on x. Mac 0xc274b-0xc274e advances it past x before scanning any digits. Native now does the same, and uses defined 32-bit wrapping escape arithmetic with Mac's signed comparison against 255.

Whitespace and primitive delimiters use signed byte comparisons in Mac. Native used unsigned comparisons, so high bytes were handled differently. The shared whitespace helper now matches both inline Mac scans, at 0xc311e-0xc31d7 and 0xc2f38-0xc2fa8. A line comment ending at EOF now leaves the cursor at the NUL rather than its last character.

Mac's primitive scanner accepts exactly 1024 bytes followed by a delimiter, and rejects the next byte before writing it. Native rejected as soon as it copied byte 1024. Both production layouts put token.type at byte 1024 and the complete token is zeroed first. Native now writes the terminator through the enclosing token object's byte representation, matching Mac without indexing beyond string[]. The token's complete allocation and cached copy are 1088 bytes.

LoadScriptFile at 0xc2e49 stores only the compressed length. Native additionally moved end_p to the compressed terminator and replaced backticks with spaces. Both extra operations were removed on native. The allocation includes the original file length plus a terminator, so Mac's retained end_p stays within the allocation. The failure message at 0xc3422 is also restored to `can't read token`.

## Matching behavior and storage adaptations

The punctuation table's lazy allocation, zeroing, signed bucket index, stable descending-length insertion and next links match Mac 0xc247c. A binary-data check matched all 52 punctuation strings, numeric IDs, zero links and sentinel at 0x349f60. Quote concatenation, escape flags, cursor/line rollback, name scanning, punctuation bounds, token cache copies and metadata were compared by hand in their Mac callers. The inline line-minus-lastline operation at 0xc31e9 also establishes NumLinesCrossed.

StripDoubleQuotes retains its existing defined native behavior: memmove for the overlapping one-byte shift, a null check and an empty-tail check. Mac 0xc2d1d calls overlapping strcpy and 0xc2d02 can read before an empty string. Normal nonempty stripping matches; these invalid/empty-storage edges are intentional adaptations.

## Verification and unresolved identities

The arm64 Release cod2_macos_ded target built at low priority with two jobs, including the changed production l_script TU. Both complete assigned TUs also compiled directly with unchanged production dedicated flags. Existing strlen-to-int and allocator-size narrowing warnings remain. Triage, constant checks, exact-symbol disassembly, production record-layout inspection, the five OFF preprocessing comparisons and git diff --check ran. There is no existing focused botlib parser fixture in this checkout. No new tests, full CI or replays were run. All corrections are compiled and reviewed, not exercised.

Twenty Linux-only rows remain unchecked: GetScriptFlags, LoadScriptMemory, PS_CheckTokenString, PS_CheckTokenType, PS_ExpectAnyToken, PS_ExpectTokenString, PS_ExpectTokenType, PS_NextWhiteSpaceChar, PS_ReadLiteral, PS_SkipUntilString, PS_UnreadLastToken, PS_UnreadToken, PunctuationFromNum, ReadSignedFloat, ReadSignedInt, ResetScript, ScriptSkipTo, SetScriptFlags, SetScriptPunctuations and StripSingleQuotes. No complete executable Mac body or inline caller was established for them. Several have only absolute-zero .eh names. The default punctuation setup in LoadScriptFile does not establish the full custom setter. Mac routes single quotes to PS_ReadString at 0xc3418; this does not establish Linux's different single-character PS_ReadLiteral helper. No native bodies were invented from these claims. The private unit ledger records every exact assigned ID and its individual gap.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-botlib-script.noindex/`.

## Integrated batch verification

Integrated source a3c4c7c passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Remaining-identity audit, 2026-11-05

All 20 open identities remain unchecked. The original Mac SO/SLINE/FUN and lexical-scope inventory, full executable symbol inventory and actual native owner/call sites were examined. The full symbol inventory has no botlib SOL inclusion path that supplies another executable owner. Absolute-zero exception names and parameter metadata do not establish callable bodies.

LoadScriptFile's punctuation setup at 0xc2e0b-0xc2e1b specializes the default table; it does not establish the complete custom/default SetScriptPunctuations contract. Initializing a new script does not establish ResetScript's existing-state behavior. The PS_ReadToken token-cache read does not establish PS_UnreadToken or PS_UnreadLastToken writes. Single quotes still dispatch to PS_ReadString, which does not prove the different single-character PS_ReadLiteral identity. No related partial operation or absence-only argument was used to close a row, and all 17 resolved verdicts are preserved exactly.

Both l_script.c and l_precomp.c compiled as actual configured stock and CoD2x arm64 Release client objects with background priority and at most two jobs. Existing narrowing warnings remain. There is no focused botlib runtime fixture in this checkout. No production source changed, so inactive source and actual __LINE__ expansions are unchanged. No new tests/cases/assertions, full CI, replay or push ran. These checks do not exercise the missing APIs. Private evidence is in `~/Library/Application Support/CoD2-native-decomp/recon-botlib-remaining.noindex/`.


## Original caller follow-up — botlib8

Original 1.0 IDB names and bounded source-order gaps now anchor every remaining target in these two modules. Complete original 1.3 bodies were matched or bounded by the original module sequence: 115 functions, with 78 unique masked whole-body matches and no unique-anchor contradiction. Tiny anonymous bodies are not identified by shape alone. The retained original direct-call inventory contains eleven calls into the target group; raw address records added only unwind metadata. Those bounded searches establish provenance, not absence-based equality. Mac remains sole behavior authority.

| Helper | Original 1.0 | Original 1.3 | Verdict |
|---|---|---|---|
| `GetScriptFlags` | 0x80518b6 | 0x8051ab6 | unchecked |
| `LoadScriptMemory` | 0x8051b20 | 0x8051d20 | unchecked |
| `PS_CheckTokenString` | 0x80514cc | 0x80516cc | unchecked |
| `PS_CheckTokenType` | 0x805153e | 0x805173e | unchecked |
| `PS_ExpectAnyToken` | 0x8051488 | 0x8051688 | unchecked |
| `PS_ExpectTokenString` | 0x80510ba | 0x80512ba | unchecked |
| `PS_ExpectTokenType` | 0x805115e | 0x805135e | unchecked |
| `PS_NextWhiteSpaceChar` | 0x8051662 | 0x8051862 | unchecked |
| `PS_ReadLiteral` | 0x8050b78 | 0x8050d78 | unchecked |
| `PS_SkipUntilString` | 0x80515c6 | 0x80517c6 | unchecked |
| `PS_UnreadLastToken` | 0x8051622 | 0x8051822 | unchecked |
| `PS_UnreadToken` | 0x8051632 | 0x8051832 | unchecked |
| `PunctuationFromNum` | 0x804fcaa | 0x804fe9a | unchecked |
| `ReadSignedFloat` | 0x8051728 | 0x8051928 | unchecked |
| `ReadSignedInt` | 0x80517fa | 0x80519fa | unchecked |
| `ResetScript` | 0x80518c2 | 0x8051ac2 | unchecked |
| `ScriptSkipTo` | 0x805195e | 0x8051b5e | unchecked |
| `SetScriptFlags` | 0x80518a8 | 0x8051aa8 | unchecked |
| `SetScriptPunctuations` | 0x804fdfa | 0x804fffa | equal |
| `StripSingleQuotes` | 0x80516de | 0x80518de | unchecked |

The new positive original call from LoadScriptFile resolves SetScriptPunctuations only for its complete consumed fresh-script NULL/default operation. Original 1.0 LoadScriptFile 0x80519d8 maps to original 1.3 0x8051bd8; the NULL argument is explicit at 0x8051cc3 and the setter call at 0x8051cd1 targets 0x804fffa. Its default branch performs both the full punctuation-table construction and the table-pointer assignment.

Fresh complete Mac LoadScriptFile 0xc2d60–0xc2e5b contains that exact default operation at 0xc2e0b–0xc2e22, before file read/compression. Original SLINE records mark the call and store as source lines 263/264 inside the loader. Complete PS_CreatePunctuationTable 0xc247c allocates/clears the buckets and maintains stable descending-length links; native l_script.c retains that typed provider and both loader operations. This is equal for the identified consumed use. It does not certify arbitrary custom tables, a reusable setter on an existing script, LoadScriptMemory, PC_SetPunctuations, or a separately exposed native API. Existing loader and storage adaptations are preserved. Independent review: review-provenance-botlib8-punctuation.md.

The other nineteen targets remain unchecked. The original signed-number readers call PS_ExpectAnyToken and PS_ExpectTokenType, but their complete corresponding Mac owner is unresolved. LoadScriptMemory's actual callers are the unresolved precompiler memory/definition helpers; the file loader is not its complete replacement. Original PS_CheckTokenString/Type reset script_p to lastscript_p on mismatch, PS_UnreadLastToken sets the cache flag, PS_UnreadToken copies a token then sets that flag, and ResetScript resets existing cursor/whitespace/line/cache state. The known token reader's cache consumption or a new allocation's zero initialization does not establish these distinct operations. Original PS_ReadLiteral consumes a single character/escape with different recovery and subtype behavior from the selected Mac single-quote string reader. Flags, punctuation-number lookup, whitespace iteration, substring skipping and single-quote stripping still lack a positive complete Mac identity.

No production source change or new defect. No compilation, tests, CI, replays or runtime exercise in this pass. Prior verification entries above describe earlier work only. All previously resolved verdicts remain unchanged. Private exact original/Mac bodies, address maps and actual caller packets are retained in `~/Library/Application Support/CoD2-native-decomp/recon-provenance-botlib8.noindex/`.
## Numeric-reader and flags-owner comparisons

Fresh exact Mac `ItemParse_textscale` at 0x1aae60–0x1aaf2a and `ItemParse_style` at 0x1abd54–0x1abe02 expose complete active signed-number consumers. They read public source-handle tokens, check initial and post-minus failure, store the numeric result and return boolean success. Original scanner `ReadSignedFloat` and `ReadSignedInt` instead call scanner expect helpers without checking their failure returns and return numeric values directly. The original float result also uses the extended token field; the integer reader has additional subtype requirements. Native's complete UI providers therefore do not establish these distinct scanner contracts. Both remain unchecked.

Primary `PS_ReadToken` tests only flag bit 0x10 with `TESTB` at 0xc3220; this does not supply the original full-word `GetScriptFlags` result. Fresh-object initialization and generic token-cache providers likewise do not establish arbitrary setters or cached-token selection. The prior default-punctuation consumed-operation closure remains exact. All nineteen open scanner rows stay unchecked.

Evidence is retained in `recon-provenance-botlib13.noindex`, reusing botlib8's original bodies/callers and prior metadata/string routes. No source changes, new defects, compilation, tests, CI, replay or runtime exercise ran.
