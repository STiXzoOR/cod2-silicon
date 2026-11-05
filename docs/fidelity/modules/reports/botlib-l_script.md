# botlib/l_script module reconciliation

Updated 2026-11-05.

**17/37 reconciled: 8 equal, 8 fixed, 1 intentional, 20 unchecked.** Mac 1.3 is the authority; Linux was read only for helper names and intent. Native corrections are under COD2_X64 guards. The selected legacy source tokens are unchanged in all five OFF configurations, and neither assigned source uses __LINE__.

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

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Remaining-identity audit, 2026-11-05

All 20 open identities remain unchecked. The original Mac SO/SLINE/FUN and lexical-scope inventory, full executable symbol inventory and actual native owner/call sites were examined. The full symbol inventory has no botlib SOL inclusion path that supplies another executable owner. Absolute-zero exception names and parameter metadata do not establish callable bodies.

LoadScriptFile's punctuation setup at 0xc2e0b-0xc2e1b specializes the default table; it does not establish the complete custom/default SetScriptPunctuations contract. Initializing a new script does not establish ResetScript's existing-state behavior. The PS_ReadToken token-cache read does not establish PS_UnreadToken or PS_UnreadLastToken writes. Single quotes still dispatch to PS_ReadString, which does not prove the different single-character PS_ReadLiteral identity. No related partial operation or absence-only argument was used to close a row, and all 17 resolved verdicts are preserved exactly.

Both l_script.c and l_precomp.c compiled as actual configured stock and CoD2x arm64 Release client objects with background priority and at most two jobs. Existing narrowing warnings remain. There is no focused botlib runtime fixture in this checkout. No production source changed, so inactive source and actual __LINE__ expansions are unchanged. No new tests/cases/assertions, full CI, replay or push ran. These checks do not exercise the missing APIs. Private evidence is in `~/Library/Application Support/CoD2-native-decomp/recon-botlib-remaining.noindex/`.
