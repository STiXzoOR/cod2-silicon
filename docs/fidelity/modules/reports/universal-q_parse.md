# universal/q_parse module reconciliation

Updated 2026-11-04.

**28/36 reconciled: 24 equal, 2 fixed, 2 intentional; 8 unchecked.** Mac 1.3 is the authority. Every assigned row was inspected.

## Corrections

In quoted tokens, a recognized escaped quote or backslash skips the following-character newline check on Mac (0xae4fb-0xae500). Native counted that newline. The native path now follows the same escape gate.

Restored the 0x15 prefix on the parse-session overflow diagnostic at Mac 0xadcf8.

## Comparison and native boundaries

Compared the complete `Com_ParseExt` paths: CSV, signed-byte whitespace, both comments, quoting and escapes, space-delimited tokens, numbers/exponents, identifiers, punctuation and cursor updates. All 14 punctuation strings and their NULL terminator match Mac data at 0x3486c0. Exact-symbol disassembly for `Com_Compress` corrected a misleading full-image prologue decode before its comment loops were compared.

Also compared session creation/end/reset, mark capture/restore, unget behavior, token positions, on-line behavior, token matching, scalar conversion and argument counting. Complete singleton lookup, initialization, whitespace and CSV helpers were identified inlined. `Com_ParseIntOnLine` is inlined as the unconditional parse/atoi sequence in `Com_SetRecommended` at 0x2f1fe-0x2f211; this does not certify that larger caller.

Retained bounded diagnostic formatting where Mac uses `vsprintf`. Existing defined native long-token/closing-quote bounds and the terminal-exponent NUL guard also remain. The patch does not recreate Mac writes beyond the token or reads beyond a terminal exponent.

## Evidence gaps

Eight rows remain unchecked: `Com_Parse1DMatrix`, `Com_Parse2DMatrix`, `Com_Parse3DMatrix`, `Com_ParseFloatOnLine`, `Com_ParseRestOfLine`, `Com_SetParseNegativeNumbers`, `Com_SetScriptErrorPrefix`, and `Com_SkipBracedSection`.

Their Linux definitions were read, and native consumers/implementations and Mac identities were sought. No complete corresponding identity was established. The native UI float parser adds empty-token failure handling, so it is insufficient evidence for the unconditional float-on-line wrapper. The two setters have no identified native setting callsite; their Mac `.eh` labels are zero-address entries. Scalar parsing and delimiter helpers alone do not establish nested matrix or maximum-nesting behavior.

## Verification and limits

The arm64 Release dedicated-server build passed at low priority with two jobs. All five existing ASan/UBSan parser cases passed: long tokens, token positions, high bytes, exponents and terminal exponents. The changed escape-followed-by-LF branch and session-overflow diagnostic were compiled and reviewed, not exercised. No new tests were written. Five inactive native-port preprocessing configurations preserve legacy tokens; the source uses no `__LINE__`. Integration owns full CI and replays.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-text.noindex/`.
