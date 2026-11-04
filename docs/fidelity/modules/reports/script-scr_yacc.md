# script/scr_yacc module reconciliation

Updated 2026-11-04.

**45 entries: 14 equal, 7 fixed, 24 unchecked.** Mac 1.3 i386 is the authority. The live scanner and parser implementation is included from `yyparse_impl.h`.

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

Twenty-four Linux flex-only rows remain unchecked, including scan/switch/delete APIs, buffer stacks, input/unput, destruction/free, debug/stream/text/line accessors, the no-op destructor and `TransferRefToParseUser`. No Mac identity or equivalent inlined caller was established for them. The Linux scanner generation differs from the older Mac scanner; missing identities were not relabeled equal.

Private table checks, decoded dispatch evidence, build/fixture receipts and the complete unit ledger remain outside Git.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.
