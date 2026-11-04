# botlib/l_precomp module reconciliation

Updated 2026-11-05.

**58/77 reconciled: 42 equal, 10 fixed, 6 intentional; 19 unchecked.** Read all 40 Mac function bodies and anchored 18 Linux-only helper identities to their inlined Mac callers. Linux names identify ledger rows; Linux implementation text was not used as authority. All source changes are under `COD2_X64`; the legacy guard comparison passes in all ten inactive configurations.

## Corrections

**Expression evaluation used the wrong operator IDs and algorithm.** Mac `PC_EvaluateTokens` at 0xbde44 uses the scanner's punctuation IDs: multiplication is 26, addition is 29, and subtraction is 30. Native interpreted unrelated IDs as these operations and rejected `?:`. The native evaluator now follows Mac's 64-entry value/operator lists, parenthesis and priority reduction order, signed 32-bit arithmetic, restricted unary minus, independent integer/double channels, ternary state, and diagnostics. The three 39-entry dispatch tables at 0x32a750, 0x32a7ec and 0x32a888 were decoded. All 39 entries in the existing operator-priority table match Mac 0x32a6a0.

**Macro expansion confused paste with stringize.** At 0xbf931 Mac checks for exactly `#` followed by NUL. Native tested only the first character, so `##` entered stringizing. At 0xbf97e Mac warns and omits an invalid stringizing operator; native copied it. Both paths now follow Mac.

**Builtin filename and numeric tokens differed.** Mac 0xbf792 takes `__FILE__` from the active script, including an include, rather than the root source. Synthetic unsigned integer fields now retain the Mac low 32 bits. Float evaluation clears the sign after narrowing to float, including negative zero, before formatting (0xc1788 and 0xc2108). Dollar float evaluation stores the low 32 bits of a truncating int64 conversion (0xc215b); native also defines the x87 invalid-conversion result's low bits as zero.

**The public token wrapper skipped invalid input.** Mac 0xc0700 calls the reader once. The native wrapper skipped or retried backticks. Blame traces that loop to the initial reconstruction, and no native compatibility contract was found in history or reports. The loop was removed. The adjacent script worker independently removed loader sanitization; integration owns combining those changes.

**Five diagnostics differed.** Restored `missing #endif` at 0xbdc80, the `#` and `$` end-of-line errors at 0xbee63 and 0xbeca5, the missing-ifdef name at 0xc018e, and the missing-undef name at 0xc0343. Both directive tables were checked: 14 name/handler pairs at 0x349ec0 and two at 0x349e20 match native.

## Defined native storage

Mac's synthetic `#eval`, sign, and stringizing tokens copy unwritten stack fields. Native retains initialized fields; stringized tokens now use the same initialized synthetic-token path. Mac's failed token-handle read also copies unwritten stack data, so native retains its existing success-only output copy. The line-handle wrapper still copies the token on failure, but initializes it first. Inspected UI numeric parser callers check failure before consuming output.

Mac stringizing can concatenate to 1024 characters and write its terminating NUL into the adjacent `token.type` field. The caller provides a declared stack `token_t`, with a 1024-byte string and no extra string capacity. Native bounds concatenation and termination to that string instead of copying the overwrite. This is an intentional adaptation, not byte equivalence at that boundary.

Mac date/time builtins free the result of `ctime` at 0xbf75d and 0xbf862. Native `ctime` returns libc-owned static storage, not a `GetMemory` allocation. Native omits those invalid frees while retaining formatting and token types.

The evaluator rejects missing operand/name storage rather than dereferencing null. Arithmetic uses defined 32-bit wrapping; the `INT_MIN / -1` and remainder corner cases do not reproduce the x86 arithmetic trap. Existing 64-byte include-path overflow and overlapping-copy behavior were not changed or exercised. No claim is made that those inherited paths are safe.

## Verification and limits

Configured an arm64 Release build and built `cod2_macos_ded` with background priority and at most two parallel jobs. The final incremental build compiled the actual production `l_precomp.c` translation unit and linked successfully. Existing narrowing and linker warnings remain. A separate production translation-unit compile also passed. `git diff --check` passes. Legacy guard comparison reports one source file, ten inactive configurations, zero mismatches; this source has no `__LINE__` expansion to preserve.

No new tests, runtime fixtures, full CI, replays, or pushes were run. The corrections are compiled and reviewed but not exercised. Parent integration owns independent review and runtime validation.

The 19 unchecked entries have no native body or caller and no verified standalone Mac symbol: `LoadSourceMemory`, `PC_AddBuiltinDefines`, `PC_AddDefine`, `PC_AddGlobalDefine`, `PC_CheckOpenSourceHandles`, `PC_CheckTokenString`, `PC_CheckTokenType`, `PC_DefineFromString`, `PC_ExpectAnyToken`, `PC_ExpectTokenString`, `PC_ExpectTokenType`, `PC_FindDefine`, `PC_RemoveAllGlobalDefines`, `PC_RemoveGlobalDefine`, `PC_SetIncludePath`, `PC_SetPunctuations`, `PC_SkipUntilString`, `PC_UnreadLastToken`, and `PC_UnreadToken`. Missing identity is not recorded as equality.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-botlib-precomp.noindex/`. Exact verdict IDs are in the external `botlib-precomp.ledger.json` unit ledger; global ledgers were not edited.
