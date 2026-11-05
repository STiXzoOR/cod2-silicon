# script/scr_yacc module reconciliation

Updated 2026-11-05.

**45 entries: 17 equal, 8 fixed, 9 intentional, 11 unchecked.** Mac 1.3 i386 is the authority. The live scanner and parser implementation is included from `yyparse_impl.h`.

## Corrections

**Path tokens had the wrong identity.** The lexer jump table at 0x33abac sends action 91 to 0x10ce89, which returns token 0x144 at 0x10cece. Native returned identifier token 0x102. The original DFA accepts a backslash path such as `A\A` through action 91; that action now retains its distinct token.

**EOF state was incomplete.** Mac 0x10fdc8 skips buffer growth when EOF is already pending. Mac 0x10ff9c-0x110089 resets the exhausted buffer, sentinels, BOL/status/file fields and scanner globals. Native now follows both paths. EOF actions 95-97 return directly at 0x10d047; native had sent them through refill handling. EOF also preserves the previous source position, as Mac does.

**The NUL transition accepted state zero.** Mac 0x1101df rejects zero as well as jam state 255; native only rejected 255. Both are now rejected.

**Three reductions allocated in a different order.** Reductions 111, 114 and 121 now sequence their allocations as Mac 0x10d535, 0x10d74c and 0x10d2fb do. Their AST field values were already correct.

## Comparison

Decoded all 131 reduction entries at 0x33a9a0 and all 98 lexer action entries at 0x33abac. Compared every reduction tag, semantic argument and source-position field, plus stack growth, error recovery, string escapes/interning, numeric scans, DFA transitions and buffer refill control.

A binary-byte comparison found no differences in the nine parser table arrays and seven lexer arrays over their used ranges. For `yy_ec`, this is the first 1,024 bytes indexed by the 256 input-byte values; the oversized native declaration contains unused trailing data. No tables or shared layouts changed. Full-width native semantic/AST and scanner-buffer pointers remain intact.

Named Linux helpers were reconciled only where their operations were identified in Mac `yyparse`/`yy_create_buffer`, including lowercase conversion, string unescaping, numeric scans, string-length interning, diagnostics, allocation/reallocation, state replay and EOF restart.

## Verification and limits

The arm64 Release `cod2_macos_ded` target builds at low priority with two jobs. Four existing parser fixtures pass: native nodes, stack growth, bad syntax and unexpected EOF, using the runner's ASan/UBSan configuration. They cover basic parser behavior and EOF diagnostics; path-token handling, embedded NUL and long-token/refill-capacity boundaries are compiled and reviewed without direct runtime coverage. EOF reset fields and allocation order were not independently asserted. No tests were added or fixtures changed. Full CI and replays were not run.

The parser group has zero inactive-token mismatches against b275adb09e414d75706f61b1770a6e85b1903d38 across four files and seven `COD2_X64=OFF` configurations.

Fourteen helper identities remain unchecked: `TransferRefToParseUser`, `input`, `yyunput`, `yy_delete_buffer`, the three memory-scan APIs, `yy_switch_to_buffer`, and the six input/output/text/length accessors. They have specific owner investigations in the private ledger. The string-reference helper is engine code, not a generated scanner API. Similar global loads or direct buffer replacement do not establish callable helper identity.

Private table checks, decoded dispatch evidence, build/fixture receipts and the complete unit ledger remain outside Git.

## Remaining scanner ownership review

The Mac STABS compilation unit names `script/scr_yacc.c`, `scr_yacc.txt`, `scr_lex.h` and `scr_lex.txt`. Its descriptor is 40 bytes with ten fields; the scanner has one `yy_current_buffer` pointer at 0xda2288. The Linux naming reference identifies a different generated interface, but its version label is not authority for Mac behavior. No exact Mac generator version is claimed.

Whole-text symbol-reference inspection and exact-entry disassembly identify the live Mac graph: `Scr_LoadScript` calls `ScriptParse`; `ScriptParse` calls `yyparse`; only `yyparse` calls `yy_create_buffer`. The buffer pointer, input stream and token-text globals are owned by these three functions; output stream and token length are consumed inside `yyparse`. Native `Scr_LoadScript` and the native debugger's `Scr_CompileTextInternal` both use `ScriptParse` with `scrCompilePub.in_ptr/parseBuf`, and refill goes through `Scr_ScanFile`.

`ScriptParse` installs a stack descriptor over static `ch_buf` at 0x110257–0x1102c6 and returns the result at 0x1102cb–0x1102d7 without restoring a previous descriptor. This does not make every scanner allocation stack-owned. `yy_create_buffer` allocates both descriptor and character storage at 0x10be39–0x10be68. Scanner initialization at 0x10fa8e–0x10faaa and EOF restart at 0x10ffb5–0x10ffd4 can install a newly allocated descriptor. Growth at 0x1100bb–0x110126 reallocates owned character storage; EOF restart resets the same active descriptor at 0x10ffde–0x110089. These paths are retained natively.

The three buffer-stack APIs and whole-stack `yylex_destroy` are intentional unsupported interfaces under that single-buffer ownership contract. There is no descriptor stack, saved predecessor, capacity or stack lifetime to implement. This decision does not close the older deletion/free/switch helpers. Scanner debug and line-counter accessors are also unsupported interfaces: Mac tokens carry cumulative byte positions through `g_out_pos/g_sourcePos`, and diagnostics consume those positions directly. Neither the recorded scanner state nor the retained DFA/action flow owns a mutable scanner line counter or tracing flag. The engine's script debugger is a separate feature.

The no-op semantic destructor is reconciled at its actual owner. Lookahead discard at 0x10c5e6–0x10c5fe resets `yychar`; error recovery at 0x10c14b–0x10c16b moves stack pointers. Abort and success at 0x10c35a–0x10c366 and 0x10fae2–0x10fae4 return without freeing semantic values or transferring string references. Native error recovery and returns have the same behavior.

This follow-up changes documentation only; the 21 previously resolved verdicts retain their exact evidence. Stock and CoD2x configured Release scanner objects compile. The four existing parser fixtures pass under ASan/UBSan in each configuration: native nodes, stack growth, bad syntax and unexpected EOF. They do not exercise the unsupported interfaces or prove the 15 remaining identities. No source, tests, assertions, diagnostic line maps or OFF preprocessing changed; no CI or replay was run for this follow-up.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.


## Native descriptor lifetime follow-up

The ownership review above describes the original Mac automatic descriptor and the native snapshot before the follow-up. Native `ScriptParse` now retains a static descriptor across normal and nonlocal returns; its row is intentional rather than equal. This removes the expired pointer and indeterminate alias read while preserving the existing serialized scanner contract. See [scanner descriptor lifetime](scanner-lifetime.md) for exact evidence and limits. The nine runtime API conclusions remain valid under this persistent single-buffer owner.


## Original raw-release helper identity

Original Linux1.3 `yyfree` at 0x808b9ca–0x808b9dc forwards its sole pointer unchanged to libc `free` and returns. Original1.0 sub_80871EE has the same complete operation. Positive original scanner deletion calls at 0x808b33a/0x808b345 establish that ownership checks and descriptor-state updates belong to its caller, outside this helper. Linux supplies the original helper identity; its larger descriptor and buffer-stack lifecycle are not imported.

The complete strict release operation maps to the existing ordinary libc provider. Mac indirect symbols bind free at 0x1acc4dd, malloc at 0x1acc514 and realloc at 0x1acc98d; fresh exact-entry scanner bodies call malloc at 0x10be40/0x10be58 and realloc at 0x1100dc. Native retains direct stdlib malloc/realloc for those same allocations, so its matching release provider is ordinary free, without zone guards or header subtraction. `yyfree` is equal under this complete-provider mapping; no standalone scanner symbol or deletion callsite is claimed or added.

This resolves only raw pointer release. `yy_delete_buffer`, memory-scan APIs, switching and accessors remain unchecked, and the approved persistent native `ScriptParse` descriptor remains unchanged. The older ownership review above remains historical evidence, with this one helper now resolved. Independent review accepted this narrow mapping. This follow-up changes documentation only: no source, tests, CI, replay, build or compilation, and no runtime verification.


Original1.0 IDB body 0x808529a also uniquely maps to original1.3 `TransferRefToParseUser` at 0x8085902. It loads the parse-user byte and passes it with the input string ID to `SL_TransferRefToUser`. This positive original identity does not equate Mac parser interning or lowercase conversion with reference transfer. The captured-global transfer operation remains unchecked; it is not a strict pointer-only provider alias like `yyfree`.


## Original scanner lifecycle and accessor identities

The retained original scanner packet now anchors all thirteen remaining scanner helpers in the ledger. Original yy_scan_string0x808b79e strictly calls strlen then yy_scan_bytes0x808b7c0; that complete provider allocates length+2, copies input, appends two NULs, invokes yy_scan_buffer0x808b6d1 and marks owned storage. The buffer scanner validates both sentinels, initializes a48-byte descriptor with borrowed storage and filling disabled, then calls yy_switch_to_buffer0x808b11d. The complete switch preserves old hold/cursor/count through its descriptor stack before installing and loading new state. The known Mac token-scanning/init path does not establish this full memory-scan/switch operation. Resolving raw yyfree earlier does not resolve delete-buffer ownership or slot clearing.

Complete original accessors0x808b89d–0x808b8eb identify input/output stream, token length/text loads and argument stores. yyunput0x808ae10 includes pushback relocation/overflow handling; input0x808af78 includes refill/restart/recursive byte-return behavior. These are positive original contracts; prior Mac global-load or token-path intersections remain insufficient complete counterparts. All thirteen remain unchecked. This integrates retained worker original evidence with parent body spot-review; it does not claim a new complete Mac scan or execution. Approved persistent native descriptor and allocator ownership remain unchanged. No source change, compilation, tests, CI, replay or runtime verification.

## Complete current-token reads (lexget25)

`yyget_leng` and `yyget_text` are equal through complete actual current-token reads in lexer actions 90/91. Full original 1.3 getters at 0x808b8b1–0x808b8ba and 0x808b8bb–0x808b8c4 perform only the respective global read and return. The original token producer at 0x808963d/0x8089649 confirms those globals hold the token-start pointer and endpoint-minus-start integer length. This new evidence goes beyond the earlier global-reference inventory: it establishes complete actual value results and concrete consumers.

Mac's actual producer writes yytext at 0x10c4ef and yyleng at 0x10c4f9, saves the hold character, terminates the token and dispatches. Independently decoded table 0x33abac maps actions 90/91 to 0x10ced8/0x10ce89. Complete integer reads at 0x10ce98/0x10cee7 feed full source-position advancement and the separate length+1 provider argument. Complete pointer reads at 0x10cebc/0x10cf0b pass yytext unchanged into `SL_GetStringOfLen` at 0x10cec4/0x10cf13. Native `int slen = yyleng` and direct yytext arguments preserve those complete results. They are not masked or Boolean-only probes, trimmed substrings, or invented pointer adapters.

The mapping requires a live matched token in the existing serialized scanner, valid backing and representable length; a defined consumer also requires representable length+1 and valid provider storage. It does not claim arbitrary stale, null or oversized state is checked. The getters have no mutation or reference effect, and their mapping ends before later arithmetic/provider behavior. No exported ABI, literal inline ancestry, future-token pointer lifetime, reentrancy/concurrency, interning/error-result or whole scanner/DFA/token-code equivalence is claimed. Existing persistent descriptor, native pointer width and ownership adaptations remain unchanged. Unrelated scanner repairs do not turn these two already-correct reads into fixed verdicts.

Parent independently reviewed fresh complete owners, original bodies, producer state, raw jump table and source hashes: `units/review-provenance-lexget25.md`. Private evidence is `recon-provenance-lexget25.noindex`. Two closures, no source change or new defect; no compilation, tests, CI, full build, replay or runtime verification.

## Selected input stream read and rejected output route (lexstream26)

`yyget_in` is fixed through the complete actual EOF input-pointer snapshot, inheriting the existing EOF repair09c27097. Original1.3 getter0x808b89d–0x808b8a6 only reads its FILE-pointer global and returns. Mac0x10ffaa reads complete yyin and saves it locally;0x11002d reloads it and0x110033 stores it unchanged in the descriptor. Nonnull additionally reaches fileno; NULL skips that call but is still a complete pointer result consumed by the descriptor store. Native `FILE *input=yyin` and the unchanged descriptor assignment were introduced by that existing repair. This is not a new defect or a claim that fileno runs for NULL.

The selected route is backed by actual input and parser state. Scr_LoadScript installs the synthetic '+' prefix and finite source; ScriptParse installs its descriptor and start3 before yyparse. Raw scanner action28 returns prefix token0x118; raw parser translation26 shifts state1, default reduction130 has length0 and goes to state3 after successful ordinary empty-list allocation. State3 requests the next token. A finite newline source selects ignored action1; the following boundary sentinels select EOB94 with cursor=start+1, hence retained count0. Scr_ScanFile reaches the source NUL with parseBufNULL and returns0, selecting the EOF snapshot. This is static table/control-flow evidence, not execution. The earlier equal cold-creation proposal is withdrawn because ScriptParse already installs the actual descriptor.

`yyget_out` remains unchecked. The generated action93 contains a full output-pointer read passed to fwrite, but all256 raw yy_accept entries contain only0–92 and94. Normal/fallback selectors use that table; synthetic EOF actions are95–97 for the actual assigned starts1/3/5. The recorded valid selector flow does not select action93, so its jump-table entry alone cannot establish a complete consumed getter. This precise rejected route does not imply an intentional unsupported API or a closure from absent symbols.

The input mapping assumes actual serialized finite input, live descriptor/backing, successful ordinary parser allocation and defined counts. It covers the complete logical pointer result and unchanged descriptor consumer, preserving the approved persistent descriptor and full native pointer width. No standalone/raw ABI, literal inline ancestry, stale or forged-state safety, stream ownership, concurrent/reentrant operation, subsequent provider/I/O result or whole scanner/parser equivalence is claimed. Independent review accepted the split in `units/review-provenance-lexstream26.md`; private original/full Mac bodies, raw selectors, prefix-parser entries and hashes remain in `recon-provenance-lexstream26.noindex`. One closure, no source change or new defect; no compilation, tests, CI, full build, replay or runtime verification.

## Remaining stream writers (lexset27)

Both stream setters remain unchecked after complete writer-path review. Original1.3 yyset_in0x808b8d2–0x808b8de and yyset_out0x808b8df–0x808b8eb store their caller-provided FILE pointer directly into the respective global. Mac yy_create_buffer receives an actual FILE argument, but0x10be96 stores it only into descriptor input_file. Its conditional global write0x10beee reads the old descriptor field before that argument store, so it does not supply the missing argument-to-global forwarding operation. Selected initialization and EOF writes load descriptor state or restore the already captured current yyin. The accepted complete EOF getter does not establish a new external-argument setter.

The output global write0x10fa6d supplies the platform stdout default under its initialization guard; it does not accept a caller-selected stream. The independently rejected generated action93 output read cannot supply the writer contract either. These are positive, distinct input-role limitations in the inspected complete owners, not closure from absent symbols or a decision to invent new stream interfaces. Existing persistent descriptor, input-source and ownership policies remain unchanged. Complete original setters and exact owners are retained in `recon-provenance-lexset27.noindex`. Two unchecked refinements, no closures, source changes or defects; no compilation, tests, CI, full build, replay or runtime verification.
