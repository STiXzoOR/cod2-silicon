# script/scr_parser module reconciliation

Updated 2026-11-04.

**47 entries: 32 equal, 2 fixed, 1 intentional, 12 unchecked.** Mac 1.3 i386 is the authority. Linux source was used only to identify helper names. The AST constructors assigned to this ledger live in `scr_parsetree.c`; they were checked there.

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
