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

## Integrated batch verification

Integrated source `7e8d818` with ownership/evidence commit `2ee4eb9` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.
