# botlib/l_precomp module reconciliation

Updated 2026-11-05.

**60/77 reconciled: 44 equal, 10 fixed, 6 intentional; 17 unchecked.** Read all 40 Mac function bodies and anchored Linux-only helper identities to their inlined Mac callers. Linux names identify ledger rows; Linux implementation text was not used as authority. All source changes are under `COD2_X64`; the legacy guard comparison passes in all ten inactive configurations.

## Corrections

**Expression evaluation used the wrong operator IDs and algorithm.** Mac `PC_EvaluateTokens` at 0xbde44 uses the scanner's punctuation IDs: multiplication is 26, addition is 29, and subtraction is 30. Native interpreted unrelated IDs as these operations and rejected `?:`. The native evaluator now follows Mac's 64-entry value/operator lists, parenthesis and priority reduction order, signed 32-bit arithmetic, restricted unary minus, independent integer/double channels, ternary state, and diagnostics. The three 39-entry dispatch tables at 0x32a750, 0x32a7ec and 0x32a888 were decoded. All 39 entries in the existing operator-priority table match Mac 0x32a6a0.

**Macro expansion confused paste with stringize.** At 0xbf931 Mac checks for exactly `#` followed by NUL. Native tested only the first character, so `##` entered stringizing. At 0xbf97e Mac warns and omits an invalid stringizing operator; native copied it. Both paths now follow Mac.

**Builtin filename and numeric tokens differed.** Mac 0xbf792 takes `__FILE__` from the active script, including an include, rather than the root source. Synthetic unsigned integer fields now retain the Mac low 32 bits. Float evaluation clears the sign after narrowing to float, including negative zero, before formatting (0xc1788 and 0xc2108). Dollar float evaluation stores the low 32 bits of a truncating int64 conversion (0xc215b); native also defines the x87 invalid-conversion result's low bits as zero.

**The public token wrapper skipped invalid input.** Mac 0xc0700 calls the reader once. The native wrapper skipped or retried backticks. Blame traces that loop to the initial reconstruction, and no native compatibility contract was found in history or reports. The loop was removed. The adjacent script worker independently removed loader sanitization; integration owns combining those changes.

**Five diagnostics differed.** Restored `missing #endif` at 0xbdc80, the `#` and `$` end-of-line errors at 0xbee63 and 0xbeca5, the missing-ifdef name at 0xc018e, and the missing-undef name at 0xc0343. Both directive tables were checked: 14 name/handler pairs at 0x349ec0 and two at 0x349e20 match native.

## Defined native storage

Mac's synthetic `#eval`, sign, and stringizing tokens copy unwritten stack fields. Native retains initialized fields; stringized tokens now use the same initialized synthetic-token path. Mac's failed token-handle read also copies unwritten stack data, so native retains its existing success-only output copy. The line-handle wrapper still copies the token on failure, but initializes it first. Inspected UI numeric parser callers check failure before consuming output.

Independent review found a combined-unit storage boundary: the corrected script reader can produce a 1024-byte primitive, with its NUL in the enclosing token's type field, while the public token has only a 1024-byte string. Both native public wrappers now inspect only the input string's declared extent and copy at most 1023 bytes plus a NUL. This intentionally truncates that exceptional primitive instead of overflowing the public string. Ordinary terminated tokens retain their content. No current production writer enabling primitive mode was found; the explicit bound nevertheless preserves defined storage at this public API.

Mac stringizing can concatenate to 1024 characters and write its terminating NUL into the adjacent `token.type` field. The caller provides a declared stack `token_t`, with a 1024-byte string and no extra string capacity. Native bounds concatenation and termination to that string instead of copying the overwrite. This is an intentional adaptation, not byte equivalence at that boundary.

Mac date/time builtins free the result of `ctime` at 0xbf75d and 0xbf862. Native `ctime` returns libc-owned static storage, not a `GetMemory` allocation. Native omits those invalid frees while retaining formatting and token types.

The evaluator rejects missing operand/name storage rather than dereferencing null. Arithmetic uses defined 32-bit wrapping; the `INT_MIN / -1` and remainder corner cases do not reproduce the x86 arithmetic trap. Existing 64-byte include-path overflow and overlapping-copy behavior were not changed or exercised. No claim is made that those inherited paths are safe.

## Verification and limits

Configured an arm64 Release build and built `cod2_macos_ded` with background priority and at most two parallel jobs. The final incremental build compiled the actual production `l_precomp.c` translation unit and linked successfully. Existing narrowing and linker warnings remain. A separate production translation-unit compile also passed. `git diff --check` passes. Legacy guard comparison reports one source file, ten inactive configurations, zero mismatches; this source has no `__LINE__` expansion to preserve.

No new tests, runtime fixtures, full CI, replays, or pushes were run. The corrections are compiled and reviewed but not exercised. Parent integration owns independent review and runtime validation.

The 18 remaining unchecked entries have no complete verified Mac body or native replacement boundary: `LoadSourceMemory`, `PC_AddBuiltinDefines`, `PC_AddDefine`, `PC_AddGlobalDefine`, `PC_CheckOpenSourceHandles`, `PC_CheckTokenType`, `PC_DefineFromString`, `PC_ExpectAnyToken`, `PC_ExpectTokenString`, `PC_ExpectTokenType`, `PC_FindDefine`, `PC_RemoveAllGlobalDefines`, `PC_RemoveGlobalDefine`, `PC_SetIncludePath`, `PC_SetPunctuations`, `PC_SkipUntilString`, `PC_UnreadLastToken`, and `PC_UnreadToken`. Missing identity is not recorded as equality.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-botlib-precomp.noindex/`. Exact verdict IDs are in the external `botlib-precomp.ledger.json` unit ledger; global ledgers were not edited.

## Integrated batch verification

Integrated source be41fb3 passed both dedicated and client builds. The full local CI run passed steps 02–10 and 12–13; its initial ABI stage found an SDK OSStatus return-width mismatch. The independently reviewed declaration correction passed the complete function/callback/strict ABI retry. Combined validation covers stock/CoD2x Release clients, platform sanitizers, function/callback ABI audits and existing source fixtures. Ten guard-visible production changes differ only under native port-on; other Apple-only source changes have no inactive differences. The new Apple SDK source is empty in all six inactive configurations. Actual net-channel assertion line expansions match the previous source in all five inactive configurations. The existing audio fixture expectation was corrected to the restored playback timeline, and the existing network runner handles the actual transport return signature. No new tests were added; these checks do not establish i386 byte parity.

Parent and fresh independent source/binary review covered all six units, including public-token termination, packaged-app lookup and owned empty clipboard storage. Sixty-three entries remain unchecked across these units. Original capture remains unavailable, and Apple public cinematic wrappers remain disabled. Unreached parser failure/exception paths, filesystem deletion, app launch, hard network errors, SOCKS, device output and decoder branches remain compiled/reviewed only. Existing focused network/audio/voice fixtures do not establish every retail input or hardware behavior.

Both 2,000-frame stock and CoD2x replays exactly match both d873794 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client rendering, platform dialogs, real audio output or microphone capture.

Catalog coverage passed without semantic catalog changes. The earlier batch 8 hosted macOS run exceeded its one-hour limit during ABI audits despite passing full local CI; that hosted run remains incomplete. Hosted ILP32 comparison is unavailable because the unchanged reference fails compilation; no byte parity is claimed.

## Remaining-identity audit, 2026-11-05

Original Mac STABS resolves the complete live use of PC_CheckTokenString inside PC_Directive_define. Its source/string parameter scope starts at original line 2810; the tok local is line 2812. SLINE 2814 maps the read at 0xc0a6f-0xc0a8e, and SLINE 2817 maps the comparison at 0xc0be3-0xc0c0f. The binary string at 0x24d6f8 is `)`. Failure continues without a token, an exact match proceeds past the empty parameter list, and mismatch allocates and copies the full token, increments numtokens, and pushes it back onto the source queue at 0xc0edb-0xc0f32. The allocation-failure branch is at 0xc0f4c-0xc0f62.

Native l_precomp.c:2658-2666 and its PC_UnreadSourceToken/PC_AllocTokenCopy owners retain all three outcomes and the complete queue operation. This establishes the complete live specialized use; it does not assert that a separately exported generic API exists. No source edit was needed. The existing PC_Directive_define verdict is preserved exactly in the full unit ledger.

The other 18 identities remain unchecked after reviewing original SO/SLINE/FUN and lexical scopes, the full symbol inventory and actual native owners. PC_FindDefineInSource traverses hashnext and is the hashed lookup, not proof of the distinct linear PC_FindDefine. Source-token queue copies do not identify PC_UnreadLastToken or PC_UnreadToken wrappers. Fresh source initialization and global-define copying do not establish the include-path setter, custom punctuation setter, memory loader or global-define management APIs. Parameter/local metadata without an executable start is not a body. No absence-only closure was made.

Both assigned production translation units compiled as actual configured arm64 Release client objects for stock and CoD2x, with background priority and at most two jobs. Existing narrowing warnings remain. Triage and constant checks were used only as heuristics; exact-symbol disassembly and binary literals support the inline comparison. No focused botlib runtime fixture exists in this checkout. No new tests, cases or assertions, full CI, replay, push or production changes were made. The inline behavior is compiled and reviewed, not exercised. With source unchanged, all inactive source and actual __LINE__ expansions remain unchanged. Private evidence is in `~/Library/Application Support/CoD2-native-decomp/recon-botlib-remaining.noindex/`.

## Original caller provenance — 2026-11-05

`PC_UnreadToken` is now equal for its complete surviving use in `PC_ReadToken`. The earlier rejection lacked an original caller mapping; that mapping is now established. The retained Linux 1.0 address database identifies the wrapper at 0x0804f3fc and its caller at 0x0804ecb0. The original Linux 1.3 ELF independently confirms the same call graph: the non-string lookahead branch calls the two-argument wrapper at 0x0804eff3, targeting 0x0804f5ec; that complete wrapper forwards both arguments to the source-token prepend provider at 0x0804ac68. Linux supplies identity and call context only.

The authoritative Mac `PC_ReadToken` reads lookahead into its separate `newtoken` local at 0xc0531–0xc0545 and dispatches non-string lookahead into the complete inline operation at 0xc0555–0xc05a8. It allocates 0x440 bytes, checks allocation, copies the whole token, clears the next link, increments numtokens, then prepends to source->tokens and resumes the caller at 0xc0480. Allocation failure calls fatal Com_Error(0) with EXE_ERR_OUT_OF_MEMORY at 0xc0696–0xc06a5. Original STABS records the `newtoken` local and the nested token-copy scope in this same owner. Native `PC_ReadToken`, `PC_UnreadSourceToken` and `PC_AllocTokenCopy` implement this complete operation with sizeof(token_t) and typed links.

This closes the emitted wrapper operation through a concrete original caller; it does not invent a standalone exported Mac API or establish `PC_UnreadLastToken`'s cached-token selection. The other 17 identities stay unchecked. All prior resolved verdicts remain unchanged. No source defect or source edit was needed. No compilation, tests, CI or replay ran, and no new runtime verification is claimed. Private original-address and exact-entry evidence is retained in `CoD2-native-decomp/recon-provenance-helpers4.noindex`.
