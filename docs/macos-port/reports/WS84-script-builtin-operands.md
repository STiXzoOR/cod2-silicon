# WS84: packed native builtin operand stores

Date: 2026-10-07. Baseline: `088e9bc`, with the uncommitted WS80 compiler,
parser, runner and loop fixtures preserved. This is a bounded follow-up to
the builtin-method alignment issue recorded by WS76 and WS80. No changes
were staged or committed by this worker.

## Confirmed defect and fix

`TempMallocAlign` in `scr_tempmemory.c` advances by the requested length;
it does not add alignment padding. A builtin opcode followed by a two-byte
index therefore legitimately places that index at an odd address.
`EmitBuiltinMethodOpcode` previously stored it through `short *` (baseline
line 2159; pre-WS84/with-WS80 line 2199). WS76's original method fixture
reported this UB in `/tmp/ws76-green-first/{cod2_macos,cod2_macos_ded}-result.log`.
Its later synthetic GetSelf prefix aligned that fixture's operand without
fixing the production write.

The neighboring `EmitBuiltinFunctionOpcode` has the same packed allocation
and typed-short store. Both were independently reproduced with UBSan before
changing production code. In `/tmp/ws84-red-stock-client`, the odd-address
method and function cases abort at `scr_compiler.c:2199` and `:2168`; both
aligned controls pass. `/tmp/ws84-red-088e9bc` repeats the same distinction
using the saved baseline source selected through `--compiler-source`.

Both native builtin emitters now convert the index to the same `short` and
copy its two bytes with `memcpy` under `COD2_X64`. The inactive branch retains
the original typed store. Allocation, opcode/count encoding, full native
function pointers and table indices are unchanged. No padding or bytecode
width change was introduced. This follows the existing native `VM_ReadU16`
reader's unaligned-safe copy pattern.

## Fixture and receipts

`tests/lp64/script/compiler_operands.c` includes the complete production
compiler and production temporary-memory allocator. Only the Hunk backing
buffer and compiler diagnostic/source-position dependencies use synthetic
seams. Unused value/folding dependencies abort if reached. It never invokes
a builtin, launches a game, or occupies the shared runtime slot.

The four cases distinguish function/method and aligned/odd operands. Each
checks 32 byte layouts: parameter counts 0 through 5, 6 and 255, crossed with
indices 0, `0x12`, `0x123` and `0x3ff`. Literal opcode and index-byte oracles,
the optional count byte, final cursor and operand positions, table contents,
and trailing sentinel bytes verify exact packed output and reject widened
stores. Each case also checks that parameter count 256 produces the retained
diagnostic without emission or table insertion. A synthetic GetSelf byte is
used only when needed to select the opposite operand parity.

The runner adds these four cases with ASan/UBSan and recovery disabled.
All seven earlier compiler/scanner/parser cases and 27 WS80 cases also pass:

| Actual Release configuration | Receipt directory | Result |
| --- | --- | --- |
| Stock client | `/tmp/ws84-green-stock-client` | 38/38 |
| Stock dedicated | `/tmp/ws84-green-stock-ded` | 38/38 |
| CoD2x client | `/tmp/ws84-green-codx-client` | 38/38 |
| CoD2x dedicated | `/tmp/ws84-green-codx-ded` | 38/38 |

These runs use the actual engine databases under
`/Users/stix/.codex/worktrees/config-catalog/cod2-native/build-macos` and
`build-macos-codx`, with source/include paths remapped to the owned tree.
Fixture assertions are enabled with `-UNDEBUG`; the Release optimization
and FP flags are retained. Build warnings are the inherited compiler
conversion warnings, not sanitizer findings. Every directory contains the
build logs and per-case run logs.

`scr_compiler.c` also compiles separately with unmodified actual Release
commands for all four configurations, including `-O3`, `-DNDEBUG` and
`-ffp-contract=off`. Exact commands, exit codes and log paths are in
`/tmp/ws84-private/release-compile-receipt.json`.

`tools/abi/legacy.py --base 088e9bc` checks both changed compiler/parser files
in five `COD2_X64`-off configurations with zero token mismatches. Its actual
SDK checks also report zero mismatches/errors for the compiler's client and
dedicated commands with `COD2_X64` removed, for both stock and CoD2x. Receipts
are `/tmp/ws84-private/{stock,codx}-legacy.log`.

Compiler-source-only function ABI audits report two translation units, zero
errors, zero mismatches and zero unprototyped floating calls for stock and
CoD2x. They use the remapped actual client/dedicated commands and the retained
empty `tools/abi/baseline.json`. Receipts are
`/tmp/ws84-private/{stock,codx}-compiler-abi.json`. These are bounded
source-level checks, not a full-engine ABI or executable compatibility proof.
`git diff --check` passes.

Reproduce the focused green cases from the owned tree:

```sh
taskpolicy -b nice -n 19 python3 tests/lp64/script/compiler_checks.py \
  --compile-db /Users/stix/.codex/worktrees/config-catalog/cod2-native/build-macos/compile_commands.engine.json \
  --target cod2_macos --output /tmp/ws84-review \
  builtin-method-odd builtin-method-aligned \
  builtin-function-odd builtin-function-aligned
```

Omit the final case arguments for the complete 38-case compiler suite; select
the CoD2x database and/or `--target cod2_macos_ded` for the other configurations.

## Limits

The fixture executes builtin bytecode emission, not a complete script
compile or VM call. Exact bytes are checked against the retained opcode and
16-bit index representation on little-endian arm64; no new original Mac
i386 executable trace was taken. No live gameplay, full-engine link,
benchmark, import/raw-offset audit, or full repository integration gate was
run in this worker. The Linux multilib byte-for-byte legacy comparison is
unavailable on this host; preprocessing is supplementary evidence.

Other typed short/int/float stores in the compiler remain, including string,
integer, branch and loop operands. The allocator inspection confirms that
its name alone cannot establish their alignment. Those writes were
inventoried but were not expanded into this bounded builtin-index fix.
The source-only candidate list is `/tmp/ws84-private/typed-operand-candidates.txt`;
it is not a reachability or alignment proof for those other writes.
Arbitrary packed bytecode emission is therefore not certified by WS84.
# Parent integration review

The combined WS80/WS84 production diff received an independent review with no
production findings. Parent reruns passed all 38 cases in stock client and
CoD2x dedicated configurations using the completed `8657435` build databases
(`/tmp/ws84-parent-stock` and `/tmp/ws84-parent-codx-ded`). The review found that
unknown case selectors silently skipped every test. The runner now rejects them:
`builtin-method-od` exits 2, while `builtin-method-odd` executes and passes.
These checks do not add whole-program VM or live gameplay coverage.
