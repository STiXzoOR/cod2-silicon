# universal/q_parse module reconciliation

Updated 2026-11-05.

**31/36 reconciled: 26 equal, 3 fixed, 2 intentional; 5 unchecked.** Mac 1.3 is the authority. Every assigned row was inspected.

## Corrections

In quoted tokens, a recognized escaped quote or backslash skips the following-character newline check on Mac (0xae4fb-0xae500). Native counted that newline. The native path now follows the same escape gate.

Restored the 0x15 prefix on the parse-session overflow diagnostic at Mac 0xadcf8.

## Comparison and native boundaries

Compared the complete `Com_ParseExt` paths: CSV, signed-byte whitespace, both comments, quoting and escapes, space-delimited tokens, numbers/exponents, identifiers, punctuation and cursor updates. All 14 punctuation strings and their NULL terminator match Mac data at 0x3486c0. Exact-symbol disassembly for `Com_Compress` corrected a misleading full-image prologue decode before its comment loops were compared.

Also compared session creation/end/reset, mark capture/restore, unget behavior, token positions, on-line behavior, token matching, scalar conversion and argument counting. Complete singleton lookup, initialization, whitespace and CSV helpers were identified inlined. `Com_ParseIntOnLine` is inlined as the unconditional parse/atoi sequence in `Com_SetRecommended` at 0x2f1fe-0x2f211; this does not certify that larger caller.

Retained bounded diagnostic formatting where Mac uses `vsprintf`. Existing defined native long-token/closing-quote bounds and the terminal-exponent NUL guard also remain. The patch does not recreate Mac writes beyond the token or reads beyond a terminal exponent.

## Evidence gaps

The initial pass left eight rows unchecked (later resolutions are documented below): `Com_Parse1DMatrix`, `Com_Parse2DMatrix`, `Com_Parse3DMatrix`, `Com_ParseFloatOnLine`, `Com_ParseRestOfLine`, `Com_SetParseNegativeNumbers`, `Com_SetScriptErrorPrefix`, and `Com_SkipBracedSection`.

Their Linux definitions were read, and native consumers/implementations and Mac identities were sought. No complete corresponding identity was established. The native UI float parser adds empty-token failure handling, so it is insufficient evidence for the unconditional float-on-line wrapper. The two setters have no identified native setting callsite; their Mac `.eh` labels are zero-address entries. Scalar parsing and delimiter helpers alone do not establish nested matrix or maximum-nesting behavior.

## Verification and limits

The arm64 Release dedicated-server build passed at low priority with two jobs. All five existing ASan/UBSan parser cases passed: long tokens, token positions, high bytes, exponents and terminal exponents. The changed escape-followed-by-LF branch and session-overflow diagnostic were compiled and reviewed, not exercised. No new tests were written. Five inactive native-port preprocessing configurations preserve legacy tokens; the source uses no `__LINE__`. Integration owns full CI and replays.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-universal-text.noindex/`.

## Integrated batch verification

Integrated source `32434b2` with ownership/evidence commit `ae8a191` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. All nine changed source files differ only under `linux-x86_64-port-on`; none uses diagnostic line-number macros. Source preprocessing does not establish i386 binary-byte parity.

Parent reviews covered all six units; independent source/binary reviews covered the changed math, dvar, text, sound and input code. Memory-file source was unchanged. Review caught an unsupported MSVC sign-bit builtin; a portable bit-copy correction was rebuilt and re-reviewed before integration. MSVC itself was not built. Native typed key storage was verified before deciding no storage mutation was required. No unresolved introduced-code finding remains.

The six units closed 543 entries. Thirty-two whole callers misattributed to universal headers were routed to their actual owners with IDs and original provenance preserved. Five separately indexed Mac rows were resolved only as exact identities of already-audited complete functions. This closes 548 work items, not 548 new implementations. The unresolved identities and platform/storage boundaries remain explicit.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `819d282` baselines and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These dedicated traces do not exercise client input/rendering or every changed diagnostic, parser, sound or numeric edge; unreached corrections remain compiled and reviewed.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. No new tests were written; the existing formatter fixture expectation was corrected against exact Mac diagnostic bytes.

## Original executable provenance follow-up

The original 1.3 ELF supplies exact identity anchors for the eight open helpers: the negative-number and error-prefix setters at 0x080b60fe/0x080b6162, braced-section skipper at 0x080b6f24, rest-of-line assembler at 0x080b704a, unconditional float-on-line wrapper at 0x080b70f4, and matrix helpers at 0x080b7166/0x080b71ec/0x080b7266. The original 1.0 anonymous matrix, float and line-assembly bodies identify their corresponding chains; 1.3 direct calls connect the nested matrix helpers. Linux remains identity/intent evidence only. No complete authoritative Mac owner was established by these anchors, and the guarded UI float conversion still cannot establish unconditional empty-token conversion. All eight remain unchecked. Exact bodies and bounded candidate evidence are retained in `recon-provenance-parser5.noindex`; no source, settled verdict or native bound changed, and no compilation, tests, CI or replay ran.

## Complete-operation boundary follow-up (parser12)

All eight remaining helpers stay unchecked. Original Com_SkipBracedSection0x80b6f24 sets exceeded when opening at depth==max without increasing depth, then still decrements for closing braces and stops at depth0 or exhausted cursor. A conventional balanced skip or immediate limit rejection is not its complete contract. Matrix parsing retains delimiter/error, dimension-loop and stride effects; scalar conversion alone cannot supply them. The prior unconditional-on-line float and arbitrary current-session setter gaps remain unchanged; guarded UI parsing and initialization-only stores were not promoted to equality.

Private original evidence remains in recon-provenance-parser5.noindex and recon-provenance-config7.noindex; new complete provider packets are in recon-provenance-parser12.noindex. No production source changes, compilation, tests, CI, replay or runtime exercise occurred.

## Complete on-line float value path (parsefloat25)

`Com_ParseFloatOnLine` is fixed through the complete operation consumed by the actual registered-float-dvar file path. Original1.0 helper0x80b4b1c maps to the complete original1.3 body0x80b70f4 through RET0x80b7120 (end0x80b7121 exclusive): on-line parse, unconditional `atof`, float32 narrowing and return. Linux establishes identity and intent only. The earlier `Float_Parse` candidate still fails because it rejects empty tokens; this actual path converts them.

Fresh complete Mac `Com_LoadDvarsFromBuffer` calls `Com_ParseOnLine`0x549df with the actual file cursor and immediately forwards its returned token to the setter0x549ea. The setter copies it to a real1024-byte local through `I_strncpyz`0x537cd; valid token lengths0..1023, including empty, preserve all bytes through NUL. No intervening parse mutates the token. Decoded type1 dispatch0x328b60→0x514bb calls `atof`0x514c1, stores the double and narrows via `CVTSD2SS`0x514c9. The complete float result is then passed to actual `Dvar_SetVariant`0x537f8 before later clamp/policy. Native preserves this full parser/copy/conversion/result-consumer chain. Raw Mac table0x34a3c0 and indirect slot0x1acd299 connect the real sun loader to `r_sunsprite_size`, registered as float0xf5dcf and consumed0xf62d9–0xf62e1.

The mapped operation includes the complete on-line provider's cursor, backup/unget, line-counter and token effects. Its fixed status inherits the existing `Com_ParseExt` escaped-character newline correction0xae4fb–0xae500, with no new defect. Existing complete primary parser review remains the authority for settled tokenization paths. Preserve native long-token/closing-quote bounds and terminal-exponent NUL protection; no invalid-storage behavior is reintroduced. The later file-loader skip-rest-of-line correction is outside this operation.

Scope is the complete selected registered-float value operation with valid parser/session/cursor/token storage and stable nonconcurrent type. It does not establish a standalone export, arbitrary missing-dvar behavior, whole loader/setter, all-type conversion, malformed-token storage, libc/locale-wide identity or x87 FP-control/sticky effects. Mac's observed double-to-float conversion governs. Independently accepted in `units/review-provenance-parsefloat25.md`; fresh bodies/data/source hashes remain private in `recon-provenance-parsefloat25.noindex`. One unchecked row becomes fixed; no source changes, new defects, compilation, tests, CI, replay or runtime exercise. Existing parser correction remains compiled and reviewed with its prior unexercised-path limits.


## Actual current-session negative-number disable (parserflag40)

Com_SetParseNegativeNumbers is equal through the complete false-byte operation in an actual material-file parse session. Full original0x80b60fe–RET0x80b6132 obtains its constant global parse provider0x80b5eb0, selects the current record and writes the entire normalized Boolean byte+0x408. Mac BeginParseSession selects its newly current record0xadc58–62 and writes literal0 at0xadc8f. Native writes the same false value to its typed current record. Other initialization, current-index increment and preceding overflow handling are outside the selected setter phase; no arbitrary nonzero-input exported setter is certified.

The new evidence establishes a real consumer beyond initialization. Actual Material_LoadPassStateMap0x103690 begins the session, changes warning prefix, disables space-delimited parsing0x1036a8 and calls ParseRuleSet0x1036f6 for the literal alphaTest rule. RuleSet→MatchToken→ParseExt selects the same record; an ordinary unquoted alphaTest token bypasses digit recognition and reads the complete negativeNumbers byte at0xae315. Its zero branch proceeds to the identifier path, and MatchToken consumes the full token at0xae765. This is an actual full-byte state read/branch; alphaTest itself would also tokenize identically with the flag true after the minus check, so no observed token difference is claimed. The normal default space-delimited path would bypass this read and is not the supporting route.

Native material allocation/name copying precedes BeginSession while Mac does it after. The accepted scope is the successful serialized setter operation on the resulting live current record, not whole allocation/initialization timing. Valid session index/backing and returning nonreentrant providers are required. Arbitrary API/nonzero input, overflow continuation, concurrent/reentrant parse state, whole parser/material loader and runtime behavior remain excluded. No existing preceding diagnostic or downstream parser/material repair is inherited by the unchanged byte write. Com_SetScriptErrorPrefix remains unchecked; warning-prefix use is not its error-prefix consumer.

Independent review accepted in units/review-provenance-parserflag40.md. Fresh complete original/provider and seven Mac bodies, exact raw store/read/literal bytes and source hashes are private in recon-provenance-parserflag40.noindex. One equal closure, zero new defects or source changes. No compilation, tests, CI, full build, replay or runtime verification.


## Complete material literal array parse (parse1d47)

Com_Parse1DMatrix is equal through the actual successful positive1..4-element material vector literal operation. The full original1.3 0x80b7166–RET0x80b71eb (original1.0 anchor0x80b4b8e) takes cursor/count/float-output, matches parentheses with error-mode0, pretests its signed count and performs all n parse/atof/store steps. It returns void; shared parser state and cursor updates are part of its operation. Its whitespace-token grammar, nonpositive-count behavior and returned-error continuation are not substituted for Mac policy.

Actual Material_FinishLoadingInstance reads the .tech backing at0x10478f, begins its session0x1047a5, disables space-delimited parsing0x1047bd and supplies that same cursor to shader argument parsing0x1053f0/0x10544f. A matched float CTAB record selects float1..float4 with actual counts1..4 and a backed four-float local. The native Apple shader-cache provider supports this nonzero validated metadata path; the generic no-CTAB fallback is not claimed to select it. Valid count1..255 metadata, matching float record, rowCount1 and successful preceding allocations/loading are required. No whole material/cache provider certification is added.

Complete Mac Material_ParseVector0x101b5c matches '(', parses/stores every actual element with comma matches between, and consumes ')'. Native retains this full do-first loop and warning-mode failure policy. Actual ParseFloat0xae7ba selects the same current parser record, consumes through ParseExt with linebreaks allowed, calls atof and narrows to float. The selected phase includes token clearing/termination, backup cursor/line, line counting, previous/current token positions and the final advanced caller cursor. At successful completion all n output floats coexist in the real array.

The vector Boolean is ignored by both Mac and native callers; no Boolean consumer is invented for the void original role. Actual Material_RegisterLiteral0x102ca8 consumes all four local words, including the independently initialized suffix for n<4, by complete comparison or full storage in its existing literal pool; the returned pointer goes into the shader argument0x102cb3. The same updated cursor then reaches the actual semicolon match0x1026ef. Thus both complete array output and parse-position/state effects have real consumers. Existing pool-count behavior and native storage policies are preserved.

Scope is successful ordinary unquoted numeric syntax recognized by the actual session, positive1..4 counts, valid stable nonconcurrent parser/cursor/output/file backing, bounded tokens/delimiters and representable line accounting. Each atof result must be finite and within representable float range before narrowing; short token length alone does not make 1e100 safe. No arbitrary n<=0, malformed/quoted/oversized token, returned diagnostic continuation, rollback, arbitrary exported ABI/overlap, same-text Linux cursor/grammar equivalence, nested2D/3D parse, whole loader/renderer or runtime claim. The existing escaped-quote parser fix and CTAB/row/LP64 owner fixes lie outside this unchanged selected numeric/punctuation parse result.

Independent review accepted in units/review-provenance-parse1d47.md; exact originals, eleven Mac containers, raw delimiters/floatN words, source hashes and peer receipts remain private in recon-provenance-parse1d47.noindex. One equal closure, zero new defects or source changes. No compilation, tests, CI, full build, replay or runtime execution. Five helpers remain unchecked: Com_Parse2DMatrix, Com_Parse3DMatrix, Com_ParseRestOfLine, Com_SetScriptErrorPrefix and Com_SkipBracedSection.
