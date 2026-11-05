# script/scr_parser module reconciliation

Updated 2026-11-05.

**47 entries: 33 equal, 2 fixed, 1 intentional, 11 unchecked.** Mac 1.3 i386 is the authority. Linux source was used only to identify helper names. The AST constructors assigned to this ledger live in `scr_parsetree.c`; they were checked there.

## Corrections

`Scr_AddSourceBufferInternal` allocated one byte less than Mac. The allocation at 0x9b2c1 is filename length including NUL, plus source length, plus two. Native now uses that count under `COD2_X64`.

`Scr_ReadFile` had a different filesystem open, temporary allocator, short-read policy and comment stripping. Its original behavior is identifiable in the nonarchive branch of Mac `Scr_AddSourceBuffer`, 0x9b4ec-0x9b560 and 0x9b599-0x9b5c0. Native now uses that open/read/terminate/close/register sequence and records missing files with length -1. `Scr_AddSourceBuffer` delegates its matching nonarchive branch to this helper. Neither path strips comments.

## Comparison and native boundary

Compared the opcode/source lookup gates, growth, sentinel indices and counters; source-buffer archive restore; diagnostic formatting and frame traversal; and every AST constructor's cell count and field order. Native AST cells and pointer-containing lookup records retain their existing pointer-sized representation. No shared header changed.

The inlined helpers checked in their Mac callers include the preceding-opcode binary search, source index load, NUL-separated line/column scan, clipped line copy, source-buffer growth and runtime caller printing.

`Scr_PrintPrevCodePos` retains the existing native low-address diagnostic guard and missing-lookup fallback. Native callers decode packed code positions before this boundary; VM frame positions are full pointers. Mac 0x9b9c6 can produce a NULL lookup and 0x9b9df then reads through it. Retaining defined diagnostics for invalid native input is intentional. The valid-pointer/found-lookup branches match Mac.

## Verification and limits

The arm64 Release `cod2_macos_ded` target builds at low priority with two jobs. The four existing parser fixtures for native nodes, stack growth, bad syntax and unexpected EOF pass with the runner's ASan/UBSan configuration. They exercise the parser/AST, not these source-file reads or source-buffer allocation changes; those are compiled and reviewed only. No fixtures were changed and no tests were added. Full CI and game replays were not run.

The parser group has zero inactive-token mismatches against b275adb09e414d75706f61b1770a6e85b1903d38 across four files and seven `COD2_X64=OFF` configurations.

Twelve Linux-only rows remain unchecked: the typed opcode/source debugger searches and formatters, file/line and line-number wrappers, developer accessor, exact-match lookup, `node3_`, `node4_` and `concat_list`. No matching Mac identity or inlined caller was established. Their presence in Linux or absence from the Mac symbol table is not an equality verdict.

Private evidence and the complete per-entry ledger are outside Git under `recon-script-parser.noindex` and the assigned unit ledger path.

## Integrated batch verification

Integrated source `8f74202` with cross-module evidence commit `03ebea4` passed both dedicated builds and all local macOS CI steps 02–13, including both Release clients, function/callback ABI audits and existing source fixtures. Guard inspection found eleven changed source files, all differing only under `linux-x86_64-port-on`. Actual allocator diagnostic line-number expansions were separately checked in all five OFF configurations because the ordinary guard check normalizes that macro.

Independent source/binary reviews covered all six units. Review caught an incorrect model-conflict diagnostic, a missing truncated collision-record bound, and legacy allocator line-number drift. All were corrected, rebuilt and re-reviewed before integration. No unresolved changed-code findings remain. The first source-fixture run failed because the existing download-name fixture lacked the newly required I_strnicmp service adapter. Fixture-only correction `67e46a9` added that dependency without changing assertions; focused stock/CoD2x checks, independent review and the complete source-fixture rerun passed. Original failed logs are preserved. Seven public animation helpers were resolved only after their complete inline read identities or shared DObj corrections were established.

Both 2,000-frame stock and CoD2x replays exactly match the fresh `d873794` baseline and each other in players, state hashes and entities. Against the original Linux recording, all six logic fields agree; maximum origin drift remains 0.11792 and velocity drift 2.0. These unchanged traces do not exercise every corrected path. Unreached diagnostic, malformed-input and allocation edges remain compiled and reviewed. Missing helper identities and storage-dependent behavior remain explicitly unchecked or documented native adaptations.

Hosted ILP32 byte comparison remains unavailable because its unchanged reference fails to compile. Source preprocessing checks do not establish binary-byte parity.

## Remaining-helper audit, 2026-11-05

Original ordered Mac stabs, executable symbols and the native debugger caller graph were checked for the twelve remaining helper identities. The debugger extension actually calls `Scr_Debugger_CodePosToLine` and `Scr_ResolveSourcePos`; those use a preceding-opcode lookup and the first source entry. Its glue also contains live same-named typed-source stubs, a guarded `Scr_GetLineNum` wrapper and a filename(line) `Scr_GetSourcePos` formatter. Older unchecked evidence that described these as absent native bodies was inaccurate and is corrected. That does not establish the omitted typed searches, exact-match lookup, formatter contracts or standalone developer accessor. The line-count primitive does not establish the buffer-index wrapper. The AST constructors and grammar calls provide no positive attribution for `node3_`, `node4_` or `concat_list`; treating typed constructors as the untyped helpers would discard their different argument contracts. All twelve remain unchecked. No replacement boundary or equality is inferred from absent symbols. Native code is unchanged.

The assigned files compiled as actual configured stock and CoD2x arm64 Release client objects in a separate evidence directory, with low priority and at most two jobs. No source or fixture was edited. Previously resolved ledger evidence is preserved exactly in the full `script-remaining.ledger.json`. No full CI or replay ran.

Independent source/binary review at source freeze `28e7d452399e38b2c296b8b4230e3eb8017b8c52` agreed with the two compiler overload closures and sixteen remaining gaps. Eight existing compiler/parser cases passed in each stock and CoD2x configuration: four binary-condition loop cases and four parser cases, using the existing ASan/UBSan runner. These checks do not exercise all reference-count types or the unresolved debugger APIs. No new cases or assertions were added.


## Original helper and caller provenance

Retained original1.0 IDB bytes now anchor the unresolved typed searches in the original1.3 executable: opcode-by-type at 0x80776a2, closest-source-by-type at 0x8077874, exact opcode lookup at 0x8077ab4, line-number wrapper at 0x8077be6 and typed source lookup at 0x8077c24. Original `Scr_GetCodePos` at 0x8078536 calls the typed source lookup at 0x8078585 and complete source formatter at 0x80785a5. Original `Scr_GetFileAndLine` at 0x80785ac calls the preceding lookup and the line wrapper. These positive chains establish original contracts independently of reconstructed names, but do not identify complete Mac owners. Exact lookup is still distinct from preceding lookup; typed selection is distinct from selecting the first source entry; the source-line/savegame formatter is distinct from native filename(line) formatting.

The original allocation-free `concat_list` body at 0x8078e94 links two complete lists and returns the first. Append/prepend allocate a node and cannot supply that complete operation. Original untyped node3_/node4_ byte bodies duplicate typed constructors on ILP32, so masked body searches yield two matches each. This does not erase native full-width untyped argument requirements. The original developer accessor candidate at 0x8077cec is anchored by the source neighborhood, but its small byte-load pattern is not unique; Mac condition checks do not establish its complete byte-return identity. All twelve rows remain unchecked. This provenance follow-up changes no production code, verdict or native adaptation and ran no compilation, tests, CI or replay.

## Complete indexed source-line operation (scriptline24)

`Scr_GetLineNum` is equal for the complete consumed indexed-buffer line count. Original 1.3 wrapper 0x8077be6–0x8077c23 reads sourceBufferLookup[index].sourceBuf with the original 24-byte stride/+8 field, calls the complete line provider using private scratch outputs, and returns its zero-based NUL-delimited line count. The new evidence supplies the previously missing actual buffer-index adapter: Mac `Scr_PrintPrevCodePos` at 0x9b9cd–0x9b9d6 selects that field, forwards the actual source offset at 0x9b9df–0x9b9ee, and tailcalls `Scr_PrintSourcePos` at 0x9b9fb. Its complete zero-offset case or scan at 0x9b705–0x9b739 creates the actual zero-based line result, stored at 0x9b733 and consumed at 0x9b653 before the separate display increment at 0x9b659. Native active caller and scan preserve that complete operation. This is not the optional debugger wrapper or a hypothetical buffer substitution.

The domain is explicit: resolved non-sentinel `0 <= sourcePos <= selected len < INT_MAX`, with valid selected lookup/source storage. Exact file/archive loaders use signed lengths, negative failure paths, and len+1 allocation; registered source storage uses filenameLen+len+2. Successful defined native backing requires those size expressions to be representable, supporting the bound rather than assuming an arbitrary restriction. Actual lexer byte positions and synthetic-prefix compensation supply resolved offsets. The -1/-2 source markers have concrete delayed/thread-start replacement paths; the printer does not validate them, and this mapping does not claim every interrupted or malformed compilation resolves all markers. Native signed scan index, line count, column and display count+1 are representable in this backed domain. A static defined example is transformed bytes `a\0bc\0` at offset 3: zero-based line 1, column 1, displayed line 2, with a nonempty current line. No execution is claimed.

The result mapping excludes unresolved sentinels, oversized offsets, malformed or mismatched lookup state, storage-size overflow, arbitrary standalone ABI, concurrency, subsequent formatting/savegame/output behavior and the later empty-line `line[lineLen-1]` access. It does not recertify either whole owner or reopen their existing storage/guard adaptations. The selected count needs no source correction or inherited fix. Independent review and parent review accepted the mapping; `units/review-provenance-scriptline24.md` and private `recon-provenance-scriptline24.noindex` retain full original/Mac producers, provider, consumer, source hash and domain evidence. One closure; no source change, compilation, tests, CI, replay or runtime verification.
