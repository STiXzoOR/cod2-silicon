# script/scr_yacc module reconciliation

Updated 2026-11-05.

**45 entries: 15 equal, 7 fixed, 9 intentional, 14 unchecked.** Mac 1.3 i386 is the authority. The live scanner and parser implementation is included from `yyparse_impl.h`.

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
